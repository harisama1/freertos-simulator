/* Example 5.2 Blocking when sending to a queue, and sending structures on a queue. */
typedef enum { eSender1, eSender2 } DataSource_t;
typedef struct { uint8_t ucValue; DataSource_t eDataSource; } Data_t;
static const Data_t xStructsToSend[ 2 ] = { { 100, eSender1 }, { 200, eSender2 } };
QueueHandle_t xQueue;
static void vSenderTask( void * pvParameters )
{
    const TickType_t xTicksToWait = pdMS_TO_TICKS( 100 );
    for( ;; )
    {
        if( xQueueSendToBack( xQueue, pvParameters, xTicksToWait ) != pdPASS )
            vPrintString( "Could not send to the queue.\r\n" );
    }
}
static void vReceiverTask( void * pvParameters )
{
    Data_t xReceivedStructure;
    volatile uint32_t ul;
    for( ;; )
    {
        if( uxQueueMessagesWaiting( xQueue ) != 3 )
            vPrintString( "Queue should have been full!\r\n" );
        if( xQueueReceive( xQueue, &xReceivedStructure, 0 ) == pdPASS )
        {
            if( xReceivedStructure.eDataSource == eSender1 )
                vPrintStringAndNumber( "From Sender 1 = ", xReceivedStructure.ucValue );
            else
                vPrintStringAndNumber( "From Sender 2 = ", xReceivedStructure.ucValue );
        }
        else
            vPrintString( "Could not receive from the queue.\r\n" );
        for( ul = 0; ul < 0x3ffff; ul++ ) { }   /* thêm so với sách: giảm lượng output */
    }
}
int main( void )
{
    xQueue = xQueueCreate( 3, sizeof( Data_t ) );
    if( xQueue != NULL )
    {
        xTaskCreate( vSenderTask, "Sender1", 1000, ( void * ) &( xStructsToSend[ 0 ] ), 2, NULL );
        xTaskCreate( vSenderTask, "Sender2", 1000, ( void * ) &( xStructsToSend[ 1 ] ), 2, NULL );
        xTaskCreate( vReceiverTask, "Receiver", 1000, NULL, 1, NULL );
        vTaskStartScheduler();
    }
    for( ;; );
}
