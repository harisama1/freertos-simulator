/*
 * Chương 7: ISR nhận UART đưa từng byte vào queue; task gom thành dòng.
 * Dùng ngoại vi giả lập trong sim/sim_hw.h:
 *   sim_uart_rx(at_ms, "text", irq)  byte đến mỗi 1 ms, mỗi byte một ngắt
 *   sim_uart_getc()                  đọc thanh ghi dữ liệu RX trong ISR
 *
 *
 * Quan sát trên Timeline:
 *  - hàng "ISR USART1_IRQ" có một vạch cho mỗi byte;
 *  - xRxQueue tăng khi byte đến, về 0 khi task LineTask lấy ra;
 *  - LineTask chỉ chạy khi có byte (block "chờ nhận từ xRxQueue").
 * Thử: đổi xQueueSendFromISR thành xQueueSend -> simulator cảnh báo gọi
 * API của task trong ISR.
 */
#define UART1_IRQ    5

static QueueHandle_t xRxQueue;

static void USART1_IRQHandler( void )
{
    BaseType_t xWoken = pdFALSE;
    int c;

    while( ( c = sim_uart_getc() ) >= 0 )
    {
        char ch = ( char ) c;

        if( xQueueSendFromISR( xRxQueue, &ch, &xWoken ) != pdPASS )
            ;   /* queue đầy: mất byte (overrun phần mềm) */
    }
    portYIELD_FROM_ISR( xWoken );
}

static void vLineTask( void * pvParameters )
{
    char line[ 64 ];
    size_t n = 0;
    char ch;

    for( ;; )
    {
        xQueueReceive( xRxQueue, &ch, portMAX_DELAY );
        if( ( ch == '\n' ) || ( n == sizeof( line ) - 1 ) )
        {
            line[ n ] = '\0';
            printf( "[%4lu ms] command: \"%s\"\n", ( unsigned long ) ( xTaskGetTickCount() * portTICK_PERIOD_MS ), line );
            n = 0;
        }
        else
        {
            line[ n++ ] = ch;
        }
    }
}

int main( void )
{
    xRxQueue = xQueueCreate( 16, sizeof( char ) );
    xTaskCreate( vLineTask, "LineTask", 256, NULL, 2, NULL );

    sim_irq_attach( UART1_IRQ, "USART1_IRQ", USART1_IRQHandler );
    sim_uart_rx( 100, "LED ON\n", UART1_IRQ );
    sim_uart_rx( 400, "READ ADC 3\n", UART1_IRQ );
    sim_uart_rx( 700, "RESET\n", UART1_IRQ );

    vTaskStartScheduler();
    for( ;; );
}
