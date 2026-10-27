/* Example 8.1 Rewriting vPrintString() to use a semaphore (mutex). */
SemaphoreHandle_t xMutex;
static void prvNewPrintString( const char * pcString )
{
    xSemaphoreTake( xMutex, portMAX_DELAY );
    {
        volatile uint32_t ul;
        printf( "%s", pcString );
        for( ul = 0; ul < 0x2ffff; ul++ ) { }   /* thêm so với sách: in chậm như UART, để thấy mutex bị giữ */
    }
    xSemaphoreGive( xMutex );
}
static void prvPrintTask( void * pvParameters )
{
    char * pcStringToPrint = ( char * ) pvParameters;
    const TickType_t xMaxBlockTimeTicks = 0x20;
    for( ;; )
    {
        prvNewPrintString( pcStringToPrint );
        vTaskDelay( ( rand() % xMaxBlockTimeTicks ) );
    }
}
int main( void )
{
    xMutex = xSemaphoreCreateMutex();
    if( xMutex != NULL )
    {
        xTaskCreate( prvPrintTask, "Print1", 1000, "Task 1 ***************************************\r\n", 1, NULL );
        xTaskCreate( prvPrintTask, "Print2", 1000, "Task 2 ---------------------------------------\r\n", 2, NULL );
        vTaskStartScheduler();
    }
    for( ;; );
}
