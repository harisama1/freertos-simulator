/*
 * Chương 10: task notification thay binary semaphore.
 * Timer phần cứng TIM2 ngắt mỗi 100 ms; ISR đọc ADC rồi gọi
 * vTaskNotifyGiveFromISR(); task xử lý chờ bằng ulTaskNotifyTake().
 *
 *
 * Quan sát trên Timeline:
 *  - hàng "ISR TIM2" mỗi 100 ms; hình thoi trên hàng Process = được notify;
 *  - Process block "chờ task notification", không cần đối tượng kernel nào
 *    (nhẹ hơn semaphore: không tốn RAM cho Queue_t);
 *  - LED (PA5) bật khi ADC > 2048.
 */
#define TIM2_IRQ    7
#define PIN_LED     5

static TaskHandle_t xProcess;
static volatile uint32_t ulLastAdc;

static void TIM2_IRQHandler( void )
{
    BaseType_t xWoken = pdFALSE;

    ulLastAdc = sim_adc_read( 0 );
    vTaskNotifyGiveFromISR( xProcess, &xWoken );
    portYIELD_FROM_ISR( xWoken );
}

static void vProcess( void * pv )
{
    for( ;; )
    {
        uint32_t n = ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        uint32_t v = ulLastAdc;

        sim_gpio_write( PIN_LED, v > 2048 );
        printf( "[%4lu ms] notify x%lu, ADC = %4lu, LED %s\n", ( unsigned long ) ( xTaskGetTickCount() * portTICK_PERIOD_MS ),
                ( unsigned long ) n, ( unsigned long ) v, v > 2048 ? "ON" : "off" );
    }
}

int main( void )
{
    xTaskCreate( vProcess, "Process", 256, NULL, 2, &xProcess );

    sim_gpio_name( PIN_LED, "LED PA5" );
    sim_adc_input( 0, "0:0 500:4095 1000:0" );      /* tam giác 0 -> 4095 -> 0 trong 1 s */
    sim_irq_attach( TIM2_IRQ, "TIM2", TIM2_IRQHandler );
    sim_irq_every( TIM2_IRQ, 100, 100 );

    vTaskStartScheduler();
    for( ;; );
}
