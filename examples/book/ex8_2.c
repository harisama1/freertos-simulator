/* Example 8.2 The alternative implementation for print task (gatekeeper). */
static QueueHandle_t xPrintQueue;
static char * pcStringsToPrint[] =
{
    "Task 1 ****************************************************\r\n",
    "Task 2 ----------------------------------------------------\r\n",
    "Message printed from the tick hook interrupt ##############\r\n"
};
static void prvStdioGatekeeperTask( void * pvParameters )
{
    char * pcMessageToPrint;
    for( ;; )
    {
        xQueueReceive( xPrintQueue, &pcMessageToPrint, portMAX_DELAY );
        printf( "%s", pcMessageToPrint );
    }
}
static void prvPrintTask( void * pvParameters )
{
    int iIndexToString = ( int ) ( intptr_t ) pvParameters;
    const TickType_t xMaxBlockTimeTicks = 0x20;
    for( ;; )
    {
        xQueueSendToBack( xPrintQueue, &( pcStringsToPrint[ iIndexToString ] ), 0 );
        vTaskDelay( ( rand() % xMaxBlockTimeTicks ) );
    }
}
void vApplicationTickHook( void )
{
    static int iCount = 0;
    iCount++;
    if( iCount >= 200 )
    {
        xQueueSendToFrontFromISR( xPrintQueue, &( pcStringsToPrint[ 2 ] ), NULL );
        iCount = 0;
    }
}
int main( void )
{
    xPrintQueue = xQueueCreate( 5, sizeof( char * ) );
    if( xPrintQueue != NULL )
    {
        xTaskCreate( prvPrintTask, "Print1", 1000, ( void * ) 0, 1, NULL );
        xTaskCreate( prvPrintTask, "Print2", 1000, ( void * ) 1, 2, NULL );
        xTaskCreate( prvStdioGatekeeperTask, "Gatekeeper", 1000, NULL, 0, NULL );
        vTaskStartScheduler();
    }
    for( ;; );
}
