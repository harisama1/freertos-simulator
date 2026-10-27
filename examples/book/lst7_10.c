/* Listing 7.10 Task xử lý trì hoãn cho ngắt UART RX: xử lý hết FIFO, có timeout.
 *
 * Hai điểm sách nêu sau Example 7.1:
 *  1. Binary semaphore chỉ chốt (latch) được MỘT sự kiện: nhiều byte tới
 *     trong lúc task đang bận thì chỉ còn một lần give. Vì vậy sau mỗi lần
 *     take, task đọc hết FIFO (while( UART_RxCount() > 0 )).
 *  2. Không chờ portMAX_DELAY: hết xMaxExpectedBlockTime (500 ms) mà không có
 *     ngắt -> coi là lỗi phần cứng, gọi UART_ClearErrors().
 * Dữ liệu: "HELLO" ở 100 ms, "WORLD" ở 300 ms (1 byte/ms, 1 ngắt/byte),
 * im lặng tới 2600 ms, rồi "AGAIN".
 * Quan sát:
 *  - 15 ngắt nhưng chỉ 6 lần take: mỗi lần take đọc cả chuỗi 5 byte;
 *  - lần take rỗng ngay sau đó: ngắt tới trong lúc task đang đọc FIFO đã
 *    được semaphore chốt lại; FIFO đã hết nên vô hại (không mất byte nào);
 *  - dòng "timeout" mỗi 500 ms trong khoảng im lặng.
 */
#define UART_RX_IRQ    3

SemaphoreHandle_t xBinarySemaphore;
static int iPending = -1;              /* byte đã đọc trước khỏi FIFO giả lập */

static void UART_RX_IRQHandler( void )
{
    BaseType_t xWoken = pdFALSE;

    xSemaphoreGiveFromISR( xBinarySemaphore, &xWoken );
    portYIELD_FROM_ISR( xWoken );
}

static int UART_RxCount( void )
{
    if( iPending < 0 )
        iPending = sim_uart_getc();
    return iPending >= 0;
}

static void UART_ProcessNextRxEvent( void )
{
    volatile uint32_t ul;

    printf( "%c", iPending );
    iPending = -1;
    for( ul = 0; ul < 0x3fffff; ul++ ) { }   /* xử lý mỗi byte tốn thời gian */
}

static void UART_ClearErrors( void )
{
    printf( "\n[%4lu ms] timeout 500 ms: không có ngắt -> UART_ClearErrors()\n", ( unsigned long ) xTaskGetTickCount() );
}

static void vUARTReceiveHandlerTask( void * pvParameters )
{
    const TickType_t xMaxExpectedBlockTime = pdMS_TO_TICKS( 500 );
    uint32_t ulTakes = 0;

    for( ;; )
    {
        if( xSemaphoreTake( xBinarySemaphore, xMaxExpectedBlockTime ) == pdPASS )
        {
            printf( "\n[%4lu ms] take #%lu: ", ( unsigned long ) xTaskGetTickCount(), ( unsigned long ) ++ulTakes );
            while( UART_RxCount() > 0 )
            {
                UART_ProcessNextRxEvent();
            }
        }
        else
        {
            UART_ClearErrors();
        }
    }
}

int main( void )
{
    xBinarySemaphore = xSemaphoreCreateBinary();
    xTaskCreate( vUARTReceiveHandlerTask, "UART Rx", 1000, NULL, 3, NULL );
    sim_irq_attach( UART_RX_IRQ, "USART1", UART_RX_IRQHandler );
    sim_uart_rx( 100, "HELLO", UART_RX_IRQ );
    sim_uart_rx( 300, "WORLD", UART_RX_IRQ );
    sim_uart_rx( 2600, "AGAIN", UART_RX_IRQ );
    vTaskStartScheduler();
    for( ;; );
}
