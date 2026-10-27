/* Example 7.4 Sending and receiving on a queue from within an interrupt. */
#define mainINTERRUPT_NUMBER 3
static QueueHandle_t xIntegerQueue, xStringQueue;
static void vIntegerGenerator( void * pvParameters )
{
    TickType_t xLastExecutionTime = xTaskGetTickCount();
    uint32_t ulValueToSend = 0;
    int i;
    for( ;; )
    {
        vTaskDelayUntil( &xLastExecutionTime, pdMS_TO_TICKS( 200 ) );
        for( i = 0; i < 5; i++ )
        {
            xQueueSendToBack( xIntegerQueue, &ulValueToSend, 0 );
            ulValueToSend++;
        }
        vPrintString( "Generator task - About to generate an interrupt.\r\n" );
        vPortGenerateSimulatedInterrupt( mainINTERRUPT_NUMBER );
        vPrintString( "Generator task - Interrupt generated.\r\n\r\n\r\n" );
    }
}
static uint32_t ulExampleInterruptHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t ulReceivedNumber;
    static const char * pcStrings[] = { "String 0\r\n", "String 1\r\n", "String 2\r\n", "String 3\r\n" };
    while( xQueueReceiveFromISR( xIntegerQueue, &ulReceivedNumber, &xHigherPriorityTaskWoken ) != errQUEUE_EMPTY )
    {
        ulReceivedNumber &= 0x03;
        xQueueSendToBackFromISR( xStringQueue, &pcStrings[ ulReceivedNumber ], &xHigherPriorityTaskWoken );
    }
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
static void vStringPrinter( void * pvParameters )
{
    char * pcString;
    for( ;; )
    {
        xQueueReceive( xStringQueue, &pcString, portMAX_DELAY );
        vPrintString( pcString );
    }
}
int main( void )
{
    xIntegerQueue = xQueueCreate( 10, sizeof( uint32_t ) );
    xStringQueue = xQueueCreate( 10, sizeof( char * ) );
    xTaskCreate( vIntegerGenerator, "IntGen", 1000, NULL, 1, NULL );
    xTaskCreate( vStringPrinter, "String", 1000, NULL, 2, NULL );
    vPortSetInterruptHandler( mainINTERRUPT_NUMBER, ulExampleInterruptHandler );
    vTaskStartScheduler();
    for( ;; );
}
