/* Example 4.8 Changing task priorities. */
TaskHandle_t xTask2Handle = NULL;
void vTask1( void * pvParameters )
{
    UBaseType_t uxPriority = uxTaskPriorityGet( NULL );
    volatile uint32_t ul;
    for( ;; )
    {
        vPrintString( "Task 1 is running\r\n" );
        for( ul = 0; ul < mainDELAY_LOOP_COUNT / 4; ul++ ) { }
        vPrintString( "About to raise the Task 2 priority\r\n" );
        vTaskPrioritySet( xTask2Handle, ( uxPriority + 1 ) );
    }
}
void vTask2( void * pvParameters )
{
    UBaseType_t uxPriority = uxTaskPriorityGet( NULL );
    volatile uint32_t ul;
    for( ;; )
    {
        vPrintString( "Task 2 is running\r\n" );
        for( ul = 0; ul < mainDELAY_LOOP_COUNT / 4; ul++ ) { }
        vPrintString( "About to lower the Task 2 priority\r\n" );
        vTaskPrioritySet( NULL, ( uxPriority - 2 ) );
    }
}
int main( void )
{
    xTaskCreate( vTask1, "Task 1", 1000, NULL, 2, NULL );
    xTaskCreate( vTask2, "Task 2", 1000, NULL, 1, &xTask2Handle );
    vTaskStartScheduler();
    for( ;; );
}
