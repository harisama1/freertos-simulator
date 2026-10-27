/* Listing 10.13 xTaskNotifyFromISR() gửi kết quả ADC trong notification value.
 *
 * vTaskNotifyGiveFromISR() chỉ báo sự kiện; xTaskNotifyFromISR() gửi kèm
 * một giá trị 32 bit. Ở đây:
 *   - task Trigger khởi động chuyển đổi ADC mỗi 50 ms; chuyển đổi xong sau
 *     1 ms -> ngắt ADC1 (ADC_ConversionEndISR);
 *   - ISR đọc kết quả, gửi bằng eSetValueWithoutOverwrite (không ghi đè giá
 *     trị task chưa đọc; pdFAIL = task không theo kịp -> configASSERT);
 *   - vADCTask chờ bằng xTaskNotifyWait( 0, 0, &ulADCValue, 100 ms ).
 * Ở 3000 ms Trigger dừng (mô phỏng lỗi tín hiệu kích ADC): vADCTask hết
 * 100 ms không có giá trị -> nhánh xử lý lỗi.
 * Quan sát: hàng ISR ADC1 mỗi 50 ms; ADC_IN0 là sóng tam giác 0-4095.
 * Thử: thêm vTaskDelay( 120 ) trong vADCTask -> ISR gặp giá trị chưa đọc,
 * xTaskNotifyFromISR() trả pdFAIL, configASSERT dừng chương trình.
 */
#define ADC1_IRQ    3

static TaskHandle_t xADCTaskToNotify;

static void ProcessADCResult( uint32_t ulADCValue )
{
    printf( "[%4lu ms] ADC = %4lu\n", ( unsigned long ) xTaskGetTickCount(), ( unsigned long ) ulADCValue );
}

void vADCTask( void * pvParameters )
{
    uint32_t ulADCValue;
    BaseType_t xResult;
    const TickType_t xADCConversionFrequency = pdMS_TO_TICKS( 50 );

    for( ;; )
    {
        xResult = xTaskNotifyWait( 0, 0, &ulADCValue, xADCConversionFrequency * 2 );

        if( xResult == pdPASS )
        {
            ProcessADCResult( ulADCValue );
        }
        else
        {
            printf( "[%4lu ms] LỖI: quá %lu ms không có kết quả ADC\n", ( unsigned long ) xTaskGetTickCount(),
                    ( unsigned long ) ( xADCConversionFrequency * 2 ) );
        }
    }
}

void ADC_ConversionEndISR( void )
{
    uint32_t ulConversionResult;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE, xResult;

    ulConversionResult = sim_adc_read( 0 );
    xResult = xTaskNotifyFromISR( xADCTaskToNotify, ulConversionResult, eSetValueWithoutOverwrite, &xHigherPriorityTaskWoken );
    configASSERT( xResult == pdPASS );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void vTrigger( void * pv )
{
    TickType_t xLast = xTaskGetTickCount();

    while( xTaskGetTickCount() < pdMS_TO_TICKS( 3000 ) )
    {
        vTaskDelayUntil( &xLast, pdMS_TO_TICKS( 50 ) );
        sim_irq_at( ADC1_IRQ, xTaskGetTickCount() * portTICK_PERIOD_MS + 1 );   /* bắt đầu chuyển đổi */
    }
    printf( "[%4lu ms] Trigger: ngừng kích ADC (mô phỏng lỗi)\n", ( unsigned long ) xTaskGetTickCount() );
    vTaskDelete( NULL );
}

int main( void )
{
    xTaskCreate( vADCTask, "ADC", 1000, NULL, 3, &xADCTaskToNotify );
    xTaskCreate( vTrigger, "Trigger", 1000, NULL, 2, NULL );
    sim_adc_input( 0, "0:0 1000:4095 2000:0 3000:4095" );
    sim_irq_attach( ADC1_IRQ, "ADC1", ADC_ConversionEndISR );
    vTaskStartScheduler();
    for( ;; );
}
