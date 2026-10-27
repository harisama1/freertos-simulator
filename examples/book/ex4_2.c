/* Example 4.2 Using the task parameter. */
void vTaskFunction( void * pvParameters )
{
    char * pcTaskName = ( char * ) pvParameters;
    volatile uint32_t ul;
    for( ;; )
    {
        vPrintString( pcTaskName );
        for( ul = 0; ul < mainDELAY_LOOP_COUNT; ul++ ) { }
    }
}
static const char * pcTextForTask1 = "Task 1 is running\r\n";
static const char * pcTextForTask2 = "Task 2 is running\r\n";
int main( void )
{
    xTaskCreate( vTaskFunction, "Task 1", 1000, ( void * ) pcTextForTask1, 1, NULL );
    xTaskCreate( vTaskFunction, "Task 2", 1000, ( void * ) pcTextForTask2, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
