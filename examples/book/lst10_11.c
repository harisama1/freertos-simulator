/* Listing 10.11 Driver UART TX dùng task notification thay binary semaphore.
 *
 * xUART_Send() lưu handle task gọi, xoá notification cũ (ulTaskNotifyTake
 * với block 0), bắt đầu truyền, rồi block chờ ISR "truyền xong" notify.
 * Không cần tạo semaphore cho mỗi UART (Listing 10.10) -> tiết kiệm RAM.
 * UART giả lập: 1 byte/ms, ngắt TX-complete sau byte cuối.
 * xTxTimeout = 40 ms: chuỗi 12 byte xong kịp; chuỗi 60 byte hết giờ ->
 * xUART_Send() trả 0. Ngắt tới muộn vẫn được notify (tăng notification
 * value) trong lúc App đang vTaskDelay, và bị xoá ở lần gửi sau nhờ
 * ulTaskNotifyTake( pdTRUE, 0 ); nếu không xoá, lần gửi sau sẽ trả về ngay
 * dù chưa truyền xong.
 * Quan sát: task App block "chờ task notification"; hình thoi notify trên
 * hàng App ngay sau mỗi lần ISR USART2_TC chạy.
 */
#define UART_TC_IRQ    3

typedef struct
{
    TaskHandle_t xTaskToNotify;
    TickType_t xTxTimeout;
} xUART;

static xUART xUart2 = { NULL, pdMS_TO_TICKS( 40 ) };

static void UART_low_level_send( xUART * pxUARTInstance, uint8_t * pucDataSource, size_t uxLength )
{
    ( void ) pxUARTInstance;
    ( void ) pucDataSource;
    sim_irq_at( UART_TC_IRQ, xTaskGetTickCount() * portTICK_PERIOD_MS + ( uint32_t ) uxLength );   /* xong sau uxLength ms */
}

BaseType_t xUART_Send( xUART * pxUARTInstance, uint8_t * pucDataSource, size_t uxLength )
{
    BaseType_t xReturn;

    pxUARTInstance->xTaskToNotify = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake( pdTRUE, 0 );
    UART_low_level_send( pxUARTInstance, pucDataSource, uxLength );
    xReturn = ( BaseType_t ) ulTaskNotifyTake( pdTRUE, pxUARTInstance->xTxTimeout );
    return xReturn;
}

void xUART_TransmitEndISR( xUART * pxUARTInstance )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    configASSERT( pxUARTInstance->xTaskToNotify != NULL );
    vTaskNotifyGiveFromISR( pxUARTInstance->xTaskToNotify, &xHigherPriorityTaskWoken );
    pxUARTInstance->xTaskToNotify = NULL;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void USART2_TC_IRQHandler( void )
{
    xUART_TransmitEndISR( &xUart2 );
}

static void vApp( void * pv )
{
    static uint8_t ucShort[] = "hello world\n";
    static uint8_t ucLong[ 60 ];
    uint32_t n = 0;

    for( ;; )
    {
        uint8_t * puc = ( n % 3 == 2 ) ? ucLong : ucShort;
        size_t len = ( puc == ucLong ) ? sizeof( ucLong ) : sizeof( ucShort ) - 1;
        TickType_t t0 = xTaskGetTickCount();
        BaseType_t xOk = xUART_Send( &xUart2, puc, len );

        printf( "[%4lu ms] gửi %2u byte: %s sau %lu ms\n", ( unsigned long ) t0, ( unsigned ) len,
                xOk ? "xong" : "TIMEOUT", ( unsigned long ) ( xTaskGetTickCount() - t0 ) );
        n++;
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
    }
}

int main( void )
{
    xTaskCreate( vApp, "App", 1000, NULL, 2, NULL );
    sim_irq_attach( UART_TC_IRQ, "USART2_TC", USART2_TC_IRQHandler );
    vTaskStartScheduler();
    for( ;; );
}
