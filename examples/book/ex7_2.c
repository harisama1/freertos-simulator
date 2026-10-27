/* Example 7.2 Using a counting semaphore to synchronize a task with an interrupt. */
#define mainINTERRUPT_NUMBER 3
SemaphoreHandle_t xCountingSemaphore;
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
        xSemaphoreTake( xCountingSemaphore, portMAX_DELAY );
        vPrintString( "Handler task - Processing event.\r\n" );
    }
}
static uint32_t ulExampleInterruptHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* 'Give' the semaphore multiple times: simulates several interrupts latched. */
    xSemaphoreGiveFromISR( xCountingSemaphore, &xHigherPriorityTaskWoken );
    xSemaphoreGiveFromISR( xCountingSemaphore, &xHigherPriorityTaskWoken );
    xSemaphoreGiveFromISR( xCountingSemaphore, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
int main( void )
{
    xCountingSemaphore = xSemaphoreCreateCounting( 10, 0 );
    if( xCountingSemaphore != NULL )
    {
        xTaskCreate( vHandlerTask, "Handler", 1000, NULL, 3, NULL );
        xTaskCreate( vPeriodicTask, "Periodic", 1000, NULL, 1, NULL );
        vPortSetInterruptHandler( mainINTERRUPT_NUMBER, ulExampleInterruptHandler );
        vTaskStartScheduler();
    }
    for( ;; );
}
