/* Example 4.7 Defining an idle task hook function. */
volatile unsigned long ulIdleCycleCount = 0UL;
void vApplicationIdleHook( void )
{
    ulIdleCycleCount++;
}
void vTaskFunction( void * pvParameters )
{
    char * pcTaskName = ( char * ) pvParameters;
    const TickType_t xDelay250ms = pdMS_TO_TICKS( 250 );
    for( ;; )
    {
        vPrintStringAndNumber( pcTaskName, ulIdleCycleCount );
        vTaskDelay( xDelay250ms );
    }
}
static const char * pcTextForTask1 = "Task 1 is running, ulIdleCycleCount =";
static const char * pcTextForTask2 = "Task 2 is running, ulIdleCycleCount =";
int main( void )
{
    xTaskCreate( vTaskFunction, "Task 1", 1000, ( void * ) pcTextForTask1, 1, NULL );
    xTaskCreate( vTaskFunction, "Task 2", 1000, ( void * ) pcTextForTask2, 2, NULL );
    vTaskStartScheduler();
    for( ;; );
}
