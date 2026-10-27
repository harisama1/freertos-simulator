/* Example 5.3 Using a Queue Set. */
static QueueHandle_t xQueue1 = NULL, xQueue2 = NULL;
static QueueSetHandle_t xQueueSet = NULL;
void vSenderTask1( void * pvParameters )
{
    const TickType_t xBlockTime = pdMS_TO_TICKS( 100 );
    const char * const pcMessage = "Message from vSenderTask1\r\n";
    for( ;; )
    {
        vTaskDelay( xBlockTime );
        xQueueSend( xQueue1, &pcMessage, 0 );
    }
}
void vSenderTask2( void * pvParameters )
{
    const TickType_t xBlockTime = pdMS_TO_TICKS( 200 );
    const char * const pcMessage = "Message from vSenderTask2\r\n";
    for( ;; )
    {
        vTaskDelay( xBlockTime );
        xQueueSend( xQueue2, &pcMessage, 0 );
    }
}
void vReceiverTask( void * pvParameters )
{
    QueueHandle_t xQueueThatContainsData;
    char * pcReceivedString;
    for( ;; )
    {
        xQueueThatContainsData = ( QueueHandle_t ) xQueueSelectFromSet( xQueueSet, portMAX_DELAY );
        xQueueReceive( xQueueThatContainsData, &pcReceivedString, 0 );
        vPrintString( pcReceivedString );
    }
}
int main( void )
{
    xQueue1 = xQueueCreate( 1, sizeof( char * ) );
    xQueue2 = xQueueCreate( 1, sizeof( char * ) );
    xQueueSet = xQueueCreateSet( 1 * 2 );
    xQueueAddToSet( xQueue1, xQueueSet );
    xQueueAddToSet( xQueue2, xQueueSet );
    xTaskCreate( vSenderTask1, "Sender1", 1000, NULL, 1, NULL );
    xTaskCreate( vSenderTask2, "Sender2", 1000, NULL, 1, NULL );
    xTaskCreate( vReceiverTask, "Receiver", 1000, NULL, 2, NULL );
    vTaskStartScheduler();
    for( ;; );
}
