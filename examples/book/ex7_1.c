/* Example 7.1 Using a binary semaphore to synchronize a task with an interrupt. */
#define mainINTERRUPT_NUMBER 3
SemaphoreHandle_t xBinarySemaphore;
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
