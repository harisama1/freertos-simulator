/* Listing 10.12 Driver UART RX dùng task notification và xTaskCheckForTimeOut().
 *
 * xUART_Receive( buf, uxWantedBytes, timeout ): chờ tới khi buffer nhận đủ
 * uxWantedBytes hoặc hết tổng thời gian chờ. Mỗi byte tới, ISR chép vào
 * buffer và notify task (nếu có task đang chờ). Task có thể bị đánh thức
 * nhiều lần; xTaskCheckForTimeOut() trừ dần thời gian đã chờ để tổng thời
 * gian không vượt xRxTimeout.
 * Dữ liệu: "ABCDEFGH" ở 50 ms, "XYZ" ở 400 ms, "12345678" ở 900 ms.
 * Task App đọc từng khối 8 byte, timeout 200 ms.
 * Quan sát:
 *  - "XYZ" chỉ có 3 byte -> lần đọc đó trả 3/8 sau đúng 200 ms (tổng, không
 *    phải 200 ms tính từ byte cuối);
 *  - lúc không có dữ liệu: trả 0/8 sau 200 ms;
 *  - App được notify mỗi byte (các hình thoi) nhưng chỉ trả về khi đủ 8 byte
 *    hoặc hết giờ.
 */
#define UART_RX_IRQ    3
#define RXBUF_LEN      64

typedef struct
{
    TaskHandle_t xTaskToNotify;
    TickType_t xRxTimeout;
    uint8_t ucBuf[ RXBUF_LEN ];
    volatile size_t uxCount;
} xUART;

static xUART xUart1 = { NULL, pdMS_TO_TICKS( 200 ) };

static size_t UART_bytes_in_rx_buffer( xUART * px )
{
    return px->uxCount;
}

static size_t UART_read_from_receive_buffer( xUART * px, uint8_t * pucBuffer, size_t uxWantedBytes )
{
    size_t n;

    taskENTER_CRITICAL();
    n = px->uxCount < uxWantedBytes ? px->uxCount : uxWantedBytes;
    memcpy( pucBuffer, px->ucBuf, n );
    memmove( px->ucBuf, px->ucBuf + n, px->uxCount - n );
    px->uxCount -= n;
    taskEXIT_CRITICAL();
    return n;
}

size_t xUART_Receive( xUART * pxUARTInstance, uint8_t * pucBuffer, size_t uxWantedBytes )
{
    size_t uxReceived = 0;
    TickType_t xTicksToWait;
    TimeOut_t xTimeOut;

    vTaskSetTimeOutState( &xTimeOut );
    xTicksToWait = pxUARTInstance->xRxTimeout;
    pxUARTInstance->xTaskToNotify = xTaskGetCurrentTaskHandle();

    while( UART_bytes_in_rx_buffer( pxUARTInstance ) < uxWantedBytes )
    {
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) != pdFALSE )
        {
            break;
        }
        ulTaskNotifyTake( pdTRUE, xTicksToWait );
    }

    pxUARTInstance->xTaskToNotify = NULL;
    uxReceived = UART_read_from_receive_buffer( pxUARTInstance, pucBuffer, uxWantedBytes );
    return uxReceived;
}

static void UART_low_level_receive( xUART * px )
{
    int c = sim_uart_getc();

    if( c >= 0 && px->uxCount < RXBUF_LEN )
        px->ucBuf[ px->uxCount++ ] = ( uint8_t ) c;
}

void xUART_ReceiveISR( xUART * pxUARTInstance )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    UART_low_level_receive( pxUARTInstance );
    if( pxUARTInstance->xTaskToNotify != NULL )
    {
        vTaskNotifyGiveFromISR( pxUARTInstance->xTaskToNotify, &xHigherPriorityTaskWoken );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}

static void USART1_IRQHandler( void )
{
    xUART_ReceiveISR( &xUart1 );
}

static void vApp( void * pv )
{
    uint8_t ucData[ 9 ];

    for( ;; )
    {
        TickType_t t0 = xTaskGetTickCount();
        size_t n = xUART_Receive( &xUart1, ucData, 8 );

        ucData[ n ] = 0;
        printf( "[%4lu ms] nhận %u/8 byte \"%s\" sau %lu ms%s\n", ( unsigned long ) xTaskGetTickCount(), ( unsigned ) n,
                ( char * ) ucData, ( unsigned long ) ( xTaskGetTickCount() - t0 ), n < 8 ? " (timeout)" : "" );
    }
}

int main( void )
{
    xTaskCreate( vApp, "App", 1000, NULL, 2, NULL );
    sim_irq_attach( UART_RX_IRQ, "USART1", USART1_IRQHandler );
    sim_uart_rx( 50, "ABCDEFGH", UART_RX_IRQ );
    sim_uart_rx( 400, "XYZ", UART_RX_IRQ );
    sim_uart_rx( 900, "12345678", UART_RX_IRQ );
    vTaskStartScheduler();
    for( ;; );
}
