/*
 * MCU: nút bấm có dội phím (bounce) + chống dội bằng software timer.
 *   - BTN (PC13) active-low, mỗi lần nhấn dội 3 lần (6 cạnh trong 6 ms);
 *   - ISR EXTI15_10 tắt ngắt của nút (mô phỏng: bỏ qua cạnh) và khởi động
 *     lại timer xDebounce 20 ms;
 *   - khi timer hết hạn mà nút vẫn = 0 -> một lần nhấn hợp lệ -> đảo LED.
 *
 * frsim: mcu=stm32f103c8
 *
 * Quan sát trên Timeline:
 *  - hàng BTN PC13 có các xung dội; hàng "ISR EXTI15_10" chạy nhiều lần;
 *  - xDebounce chỉ hết hạn 1 lần cho mỗi lần nhấn; LED PA5 đảo đúng 3 lần.
 * Thử: bỏ timer, đảo LED ngay trong ISR -> LED đảo nhiều lần mỗi lần nhấn.
 */
#define EXTI15_10_IRQ   8
#define PIN_BTN         13
#define PIN_LED         5

static TimerHandle_t xDebounce;
static volatile uint32_t ulEdges;

static void EXTI15_10_IRQHandler( void )
{
    BaseType_t xWoken = pdFALSE;

    ulEdges++;
    xTimerResetFromISR( xDebounce, &xWoken );   /* mỗi cạnh dời mốc hết hạn thêm 20 ms */
    portYIELD_FROM_ISR( xWoken );
}

static void prvDebounced( TimerHandle_t t )
{
    if( sim_gpio_read( PIN_BTN ) == 0 )
    {
        sim_gpio_toggle( PIN_LED );
        printf( "[%4lu ms] nhấn hợp lệ (sau %lu cạnh), LED = %d\n",
                ( unsigned long ) ( xTaskGetTickCount() * portTICK_PERIOD_MS ), ( unsigned long ) ulEdges, sim_gpio_read( PIN_LED ) );
    }
    ulEdges = 0;
}

static void vHeartbeat( void * pv )
{
    for( ;; )
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
}

int main( void )
{
    xDebounce = xTimerCreate( "Debounce", pdMS_TO_TICKS( 20 ), pdFALSE, NULL, prvDebounced );
    xTaskCreate( vHeartbeat, "Heartbeat", 128, NULL, 1, NULL );

    sim_gpio_name( PIN_BTN, "BTN PC13" );
    sim_gpio_name( PIN_LED, "LED PA5" );
    sim_irq_attach( EXTI15_10_IRQ, "EXTI15_10", EXTI15_10_IRQHandler );
    sim_button( PIN_BTN, 100, 150, 3, EXTI15_10_IRQ );
    sim_button( PIN_BTN, 500, 60, 3, EXTI15_10_IRQ );
    sim_button( PIN_BTN, 800, 200, 3, EXTI15_10_IRQ );

    vTaskStartScheduler();
    for( ;; );
}
