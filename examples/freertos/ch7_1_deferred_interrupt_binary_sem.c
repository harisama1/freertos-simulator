/*
 * Chương 7, Example 7.1: xử lý ngắt trì hoãn (deferred interrupt
 * processing) bằng binary semaphore. Giữ nguyên API Windows port của sách:
 * vPortSetInterruptHandler() / vPortGenerateSimulatedInterrupt().
 *
 * Quan sát trên Timeline:
 *  - hàng "ISR IRQ3": mỗi 500 ms một lần ngắt;
 *  - hàng xBinarySemaphore: 0 -> 1 khi ISR give, 1 -> 0 khi Handler take;
 *  - Handler (p3) chạy ngay sau ISR, trước khi Periodic (p1) chạy tiếp,
 *    vì ISR gọi portYIELD_FROM_ISR(pdTRUE).
 * Thử: bỏ dòng portYIELD_FROM_ISR -> Handler chỉ chạy ở tick kế tiếp.
 */
#define mainINTERRUPT_NUMBER    3

static void vHandlerTask( void * pvParameters );
static void vPeriodicTask( void * pvParameters );
static uint32_t ulExampleInterruptHandler( void );

SemaphoreHandle_t xBinarySemaphore;

int main( void )
{
    xBinarySemaphore = xSemaphoreCreateBinary();

    if( xBinarySemaphore != NULL )
    {
        xTaskCreate( vHandlerTask, "Handler", 1000, NULL, 3, NULL );
        xTaskCreate( vPeriodicTask, "Periodic", 1000, NULL, 1, NULL );
        vPortSetInterruptHandler( mainINTERRUPT_NUMBER, ulExampleInterruptHandler );
        vTaskStartScheduler();
    }

    for( ;; );
}

static void vPeriodicTask( void * pvParameters )
{
    const TickType_t xDelay500ms = pdMS_TO_TICKS( 500UL );

    for( ;; )
    {
        vTaskDelay( xDelay500ms );
        vPrintString( "Periodic task - About to generate an interrupt.\r\n" );
        vPortGenerateSimulatedInterrupt( mainINTERRUPT_NUMBER );
        vPrintString( "Periodic task - Interrupt generated.\r\n\r\n\r\n" );
    }
}

static void vHandlerTask( void * pvParameters )
{
    for( ;; )
    {
        xSemaphoreTake( xBinarySemaphore, portMAX_DELAY );
        vPrintString( "Handler task - Processing event.\r\n" );
    }
}

static uint32_t ulExampleInterruptHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR( xBinarySemaphore, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
