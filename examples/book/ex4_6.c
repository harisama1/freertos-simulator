/* Example 4.6 Combining blocking and non-blocking tasks. */
void vContinuousProcessingTask( void * pvParameters )
{
    char * pcTaskName = ( char * ) pvParameters;
    volatile uint32_t ul;
    for( ;; )
    {
        vPrintString( pcTaskName );
        for( ul = 0; ul < 0xfffff; ul++ ) { }   /* thêm so với sách: giảm lượng output */
    }
}
void vPeriodicTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay3ms = pdMS_TO_TICKS( 3 );
    for( ;; )
    {
        vPrintString( "Periodic task is running\r\n" );
        vTaskDelayUntil( &xLastWakeTime, xDelay3ms );
    }
}
int main( void )
{
    xTaskCreate( vContinuousProcessingTask, "Task 1", 1000, ( void * ) "Continuous task 1 running\r\n", 1, NULL );
    xTaskCreate( vContinuousProcessingTask, "Task 2", 1000, ( void * ) "Continuous task 2 running\r\n", 1, NULL );
    xTaskCreate( vPeriodicTask, "Task 3", 1000, NULL, 2, NULL );
    vTaskStartScheduler();
    for( ;; );
}
