/* Example 9.2 Synchronizing tasks (xEventGroupSync). */
#define mainFIRST_TASK_BIT  ( 1UL << 0UL )
#define mainSECOND_TASK_BIT ( 1UL << 1UL )
#define mainTHIRD_TASK_BIT  ( 1UL << 2UL )
EventGroupHandle_t xEventGroup;
static void vSyncingTask( void * pvParameters )
{
    const TickType_t xMaxDelay = pdMS_TO_TICKS( 4000UL );
    const TickType_t xMinDelay = pdMS_TO_TICKS( 200UL );
    TickType_t xDelayTime;
    EventBits_t uxThisTasksSyncBit = ( EventBits_t ) ( uintptr_t ) pvParameters;
    const EventBits_t uxAllSyncBits = ( mainFIRST_TASK_BIT | mainSECOND_TASK_BIT | mainTHIRD_TASK_BIT );
    for( ;; )
    {
        xDelayTime = ( rand() % xMaxDelay ) + xMinDelay;
        vTaskDelay( xDelayTime );
        vPrintTwoStrings( pcTaskGetTaskName( NULL ), "reached sync point" );
        xEventGroupSync( xEventGroup, uxThisTasksSyncBit, uxAllSyncBits, portMAX_DELAY );
        vPrintTwoStrings( pcTaskGetTaskName( NULL ), "exited sync point" );
    }
}
int main( void )
{
    xEventGroup = xEventGroupCreate();
    xTaskCreate( vSyncingTask, "Task 1", 1000, ( void * ) mainFIRST_TASK_BIT, 1, NULL );
    xTaskCreate( vSyncingTask, "Task 2", 1000, ( void * ) mainSECOND_TASK_BIT, 1, NULL );
    xTaskCreate( vSyncingTask, "Task 3", 1000, ( void * ) mainTHIRD_TASK_BIT, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
