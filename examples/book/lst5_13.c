/* Listing 5.13-5.15 Queuing pointers: gửi con trỏ tới buffer qua queue (buffer cấp phát động).
 *
 * Queue chỉ chứa con trỏ (sizeof( char * ) = 4 byte trên MCU 32-bit), không
 * chép chuỗi 50 byte. Quy tắc sở hữu (mục 5.5.1):
 *   - trước xQueueSend(): chỉ Sender được ghi buffer;
 *   - sau xQueueReceive(): chỉ Receiver được đọc, và Receiver giải phóng buffer.
 * prvGetBuffer() = pvPortMalloc(), prvReleaseBuffer() = vPortFree().
 *
 * Thêm so với sách: Sender p2 > Receiver p1, Receiver xử lý chậm 200 ms,
 * nên queue luôn đầy (5 con trỏ) và Sender block "chờ chỗ trống".
 * Quan sát:
 *  - tab Thống kê > Heap: tối đa 7 buffer cùng tồn tại = 5 con trỏ trong queue
 *    + 1 buffer Sender đang giữ khi block + 1 buffer Receiver đang in.
 *    Sender (p2) được đánh thức ngay trong xQueueReceive() của Receiver và
 *    cấp phát buffer mới TRƯỚC khi Receiver kịp vPortFree();
 *  - các địa chỉ in ra lặp lại theo chu kỳ 7.
 */
QueueHandle_t xPointerQueue;

static void * prvGetBuffer( size_t xSize )
{
    return pvPortMalloc( xSize );
}

static void prvReleaseBuffer( void * pvBuffer )
{
    vPortFree( pvBuffer );
}

void vStringSendingTask( void * pvParameters )
{
    char * pcStringToSend;
    const size_t xMaxStringLength = 50;
    BaseType_t xStringNumber = 0;

    for( ;; )
    {
        pcStringToSend = ( char * ) prvGetBuffer( xMaxStringLength );
        snprintf( pcStringToSend, xMaxStringLength, "String number %d\r\n", ( int ) xStringNumber );
        xStringNumber++;
        xQueueSend( xPointerQueue, &pcStringToSend, portMAX_DELAY );   /* gửi ĐỊA CHỈ buffer */
    }
}

void vStringReceivingTask( void * pvParameters )
{
    char * pcReceivedString;

    for( ;; )
    {
        xQueueReceive( xPointerQueue, &pcReceivedString, portMAX_DELAY );
        printf( "[%4lu ms] nhận con trỏ %p -> ", ( unsigned long ) xTaskGetTickCount(), ( void * ) pcReceivedString );
        vPrintString( pcReceivedString );
        prvReleaseBuffer( pcReceivedString );
        vTaskDelay( pdMS_TO_TICKS( 200 ) );     /* thêm so với sách: xử lý chậm */
    }
}

int main( void )
{
    xPointerQueue = xQueueCreate( 5, sizeof( char * ) );
    xTaskCreate( vStringSendingTask, "Sender", 1000, NULL, 2, NULL );
    xTaskCreate( vStringReceivingTask, "Receiver", 1000, NULL, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
