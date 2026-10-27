/* Example 4.5 Converting the example tasks to use vTaskDelayUntil(). */
void vTaskFunction( void * pvParameters )
{
    char * pcTaskName = ( char * ) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for( ;; )
    {
        vPrintString( pcTaskName );
        vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( 250 ) );
    }
}
static const char * pcTextForTask1 = "Task 1 is running\r\n";
static const char * pcTextForTask2 = "Task 2 is running\r\n";
int main( void )
{
    xTaskCreate( vTaskFunction, "Task 1", 1000, ( void * ) pcTextForTask1, 1, NULL );
    xTaskCreate( vTaskFunction, "Task 2", 1000, ( void * ) pcTextForTask2, 2, NULL );
    vTaskStartScheduler();
    for( ;; );
}
