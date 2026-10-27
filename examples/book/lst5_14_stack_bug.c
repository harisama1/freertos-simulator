/* Mục 5.5.1 (lỗi cần tránh): gửi con trỏ tới dữ liệu nằm trên stack của task.
 *
 * Sách: "A pointer should never be used to access data that has been
 * allocated on a task stack". Ở đây Sender ghi chuỗi vào mảng cục bộ
 * cBuffer[] rồi gửi địa chỉ của nó; mảng đó bị ghi đè ở vòng lặp sau.
 * Sender p2 > Receiver p1: Sender gửi liền 5 lần (queue đầy) trước khi
 * Receiver chạy.
 * Kết quả sai: mọi con trỏ trong queue là cùng một địa chỉ. Queue là FIFO
 * nên Receiver lẽ ra in "String number 0, 1, 2...", nhưng in "6, 7, 8...":
 * nội dung đã bị Sender ghi đè trước khi Receiver đọc; số 0-5 bị mất.
 * Sửa: dùng lst5_13.c (buffer cấp phát động) hoặc lst5_14_pool.c (pool).
 */
QueueHandle_t xPointerQueue;

void vStringSendingTask( void * pvParameters )
{
    char cBuffer[ 50 ];                     /* SAI: buffer trên stack của Sender */
    char * pcStringToSend = cBuffer;
    BaseType_t xStringNumber = 0;

    for( ;; )
    {
        snprintf( cBuffer, sizeof( cBuffer ), "String number %d\r\n", ( int ) xStringNumber );
        xStringNumber++;
        xQueueSend( xPointerQueue, &pcStringToSend, portMAX_DELAY );
    }
}

void vStringReceivingTask( void * pvParameters )
{
    char * pcReceivedString;

    for( ;; )
    {
        xQueueReceive( xPointerQueue, &pcReceivedString, portMAX_DELAY );
        printf( "[%4lu ms] con trỏ %p -> ", ( unsigned long ) xTaskGetTickCount(), ( void * ) pcReceivedString );
        vPrintString( pcReceivedString );
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
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
