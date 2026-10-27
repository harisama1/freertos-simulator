/* Example 10.1 Using a task notification in place of a semaphore, method 1. */
#define mainINTERRUPT_NUMBER 3
const TickType_t xInterruptFrequency = pdMS_TO_TICKS( 500UL );
static TaskHandle_t xHandlerTask = NULL;
static void vPeriodicTask( void * pvParameters )
{
    for( ;; )
    {
        vTaskDelay( xInterruptFrequency );
        vPrintString( "Periodic task - About to generate an interrupt.\r\n" );
        vPortGenerateSimulatedInterrupt( mainINTERRUPT_NUMBER );
        vPrintString( "Periodic task - Interrupt generated.\r\n\r\n\r\n" );
    }
}
static void vHandlerTask( void * pvParameters )
{
    const TickType_t xMaxExpectedBlockTime = xInterruptFrequency + pdMS_TO_TICKS( 10 );
    uint32_t ulEventsToProcess;
    for( ;; )
    {
        ulEventsToProcess = ulTaskNotifyTake( pdTRUE, xMaxExpectedBlockTime );
        if( ulEventsToProcess != 0 )
        {
            while( ulEventsToProcess > 0 )
            {
                vPrintString( "Handler task - Processing event.\r\n" );
                ulEventsToProcess--;
            }
        }
        else
        {
            /* An interrupt did not arrive within the expected time. */
        }
    }
}
static uint32_t ulExampleInterruptHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR( xHandlerTask, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
int main( void )
{
    xTaskCreate( vHandlerTask, "Handler", 1000, NULL, 3, &xHandlerTask );
    xTaskCreate( vPeriodicTask, "Periodic", 1000, NULL, 1, NULL );
    vPortSetInterruptHandler( mainINTERRUPT_NUMBER, ulExampleInterruptHandler );
    vTaskStartScheduler();
    for( ;; );
}
