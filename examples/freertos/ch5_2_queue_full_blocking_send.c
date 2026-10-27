/*
 * Chương 5, Example 5.2: ngược với 5.1 - hai task gửi (p2) ưu tiên cao
 * hơn task nhận (p1), dữ liệu là struct có nguồn gửi.
 *
 * Quan sát trên Timeline:
 *  - xQueue đầy (3/3) gần như suốt thời gian chạy;
 *  - Sender block với lý do "chờ gửi vào xQueue (đầy)";
 *  - mỗi lần Receiver lấy một phần tử, một Sender được unblock và chiếm
 *    CPU ngay (preemption) để lấp chỗ trống.
 */
typedef enum { eSender1, eSender2 } DataSource_t;
typedef struct { uint8_t ucValue; DataSource_t eDataSource; } Data_t;

static const Data_t xStructsToSend[ 2 ] = { { 100, eSender1 }, { 200, eSender2 } };

static void vSenderTask( void * pvParameters );
static void vReceiverTask( void * pvParameters );

QueueHandle_t xQueue;

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
        {
            vPrintString( "Could not receive from the queue.\r\n" );
        }
        vTaskDelay( pdMS_TO_TICKS( 20 ) );   /* thêm so với sách: giảm lượng output */
    }
}
