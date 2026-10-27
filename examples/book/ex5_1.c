/* Example 5.1 Blocking when receiving from a queue. */
QueueHandle_t xQueue;
static void vSenderTask( void * pvParameters )
{
    int32_t lValueToSend = ( int32_t ) ( intptr_t ) pvParameters;
    BaseType_t xStatus;
    volatile uint32_t ul;
    for( ;; )
    {
        xStatus = xQueueSendToBack( xQueue, &lValueToSend, 0 );
        if( xStatus != pdPASS )
            vPrintString( "Could not send to the queue.\r\n" );
        for( ul = 0; ul < 0x3ffff; ul++ ) { }   /* thêm so với sách: giảm lượng output */
        taskYIELD();
    }
}
static void vReceiverTask( void * pvParameters )
{
    int32_t lReceivedValue;
    BaseType_t xStatus;
    const TickType_t xTicksToWait = pdMS_TO_TICKS( 100 );
    for( ;; )
    {
        if( uxQueueMessagesWaiting( xQueue ) != 0 )
            vPrintString( "Queue should have been empty!\r\n" );
        xStatus = xQueueReceive( xQueue, &lReceivedValue, xTicksToWait );
        if( xStatus == pdPASS )
            vPrintStringAndNumber( "Received = ", lReceivedValue );
        else
            vPrintString( "Could not receive from the queue.\r\n" );
    }
}
int main( void )
{
    xQueue = xQueueCreate( 5, sizeof( int32_t ) );
    if( xQueue != NULL )
    {
        xTaskCreate( vSenderTask, "Sender1", 1000, ( void * ) 100, 1, NULL );
        xTaskCreate( vSenderTask, "Sender2", 1000, ( void * ) 200, 1, NULL );
        xTaskCreate( vReceiverTask, "Receiver", 1000, NULL, 2, NULL );
        vTaskStartScheduler();
    }
    for( ;; );
}
