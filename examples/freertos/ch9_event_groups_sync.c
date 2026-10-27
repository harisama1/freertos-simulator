/*
 * Chương 9: event group.
 *   - ISR nút bấm (EXTI0) set bit 0; task Sensor set bit 1 mỗi 300 ms;
 *   - task Logger chờ CẢ HAI bit (xWaitForAllBits = pdTRUE) rồi xoá.
 *
 *
 * Quan sát trên Timeline:
 *  - Logger block "chờ bit 0x3 của xEvents";
 *  - hàng xEvents: vạch set/clear, rê chuột để xem bit và ai set;
 *  - hàng BTN: mức GPIO (nhấn = 0) tại 250 ms và 900 ms.
 * Lưu ý: xEventGroupSetBitsFromISR() chuyển việc set bit sang daemon task
 * (Tmr Svc), nên Tmr Svc chạy ngay sau ISR.
 */
#define BIT_BUTTON    ( 1UL << 0 )
#define BIT_SENSOR    ( 1UL << 1 )
#define EXTI0_IRQ     6
#define PIN_BTN       0

static EventGroupHandle_t xEvents;

static void EXTI0_IRQHandler( void )
{
    BaseType_t xWoken = pdFALSE;

    if( sim_gpio_read( PIN_BTN ) == 0 )       /* cạnh xuống = nhấn */
        xEventGroupSetBitsFromISR( xEvents, BIT_BUTTON, &xWoken );
    portYIELD_FROM_ISR( xWoken );
}

static void vSensor( void * pv )
{
    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 300 ) );
        xEventGroupSetBits( xEvents, BIT_SENSOR );
    }
}

static void vLogger( void * pv )
{
    for( ;; )
    {
        EventBits_t b = xEventGroupWaitBits( xEvents, BIT_BUTTON | BIT_SENSOR, pdTRUE, pdTRUE, portMAX_DELAY );

        printf( "[%4lu ms] Logger: bits = 0x%lx -> ghi log\n",
                ( unsigned long ) ( xTaskGetTickCount() * portTICK_PERIOD_MS ), ( unsigned long ) b );
    }
}

int main( void )
{
    xEvents = xEventGroupCreate();
    xTaskCreate( vSensor, "Sensor", 256, NULL, 1, NULL );
    xTaskCreate( vLogger, "Logger", 256, NULL, 2, NULL );

    sim_gpio_name( PIN_BTN, "BTN" );
    sim_irq_attach( EXTI0_IRQ, "EXTI0", EXTI0_IRQHandler );
    sim_button( PIN_BTN, 250, 80, 0, EXTI0_IRQ );
    sim_button( PIN_BTN, 900, 80, 0, EXTI0_IRQ );

    vTaskStartScheduler();
    for( ;; );
}
