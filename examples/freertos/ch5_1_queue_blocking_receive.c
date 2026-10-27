/*
 * Chương 5, Example 5.1: hai task gửi (p1) và một task nhận (p2) qua queue
 * 5 phần tử int32_t.
 *
 * Quan sát trên Timeline:
 *  - Receiver ưu tiên cao hơn nên lấy dữ liệu ngay khi có: hàng xQueue
 *    chỉ lên 1 rồi về 0, không bao giờ đầy;
 *  - Receiver block với lý do "chờ nhận từ xQueue";
 *  - hai Sender chia CPU theo time slice (cùng ưu tiên 1, taskYIELD).
 */
static void vSenderTask( void * pvParameters );
static void vReceiverTask( void * pvParameters );

QueueHandle_t xQueue;

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

static void vSenderTask( void * pvParameters )
{
    int32_t lValueToSend = ( int32_t ) ( intptr_t ) pvParameters;

    for( ;; )
    {
        if( xQueueSendToBack( xQueue, &lValueToSend, 0 ) != pdPASS )
            vPrintString( "Could not send to the queue.\r\n" );
        taskYIELD();
    }
}

static void vReceiverTask( void * pvParameters )
{
    int32_t lReceivedValue;
    const TickType_t xTicksToWait = pdMS_TO_TICKS( 100 );

    for( ;; )
    {
        if( uxQueueMessagesWaiting( xQueue ) != 0 )
            vPrintString( "Queue should have been empty!\r\n" );

        if( xQueueReceive( xQueue, &lReceivedValue, xTicksToWait ) == pdPASS )
            vPrintStringAndNumber( "Received = ", lReceivedValue );
        else
            vPrintString( "Could not receive from the queue.\r\n" );
    }
}
