/* Listing 5.14-5.15 Queuing pointers: prvGetBuffer() lấy buffer từ pool cấp phát sẵn.
 *
 * Cách thứ hai mà sách nêu cho prvGetBuffer(): pool 3 buffer tĩnh, không
 * dùng heap lúc chạy (hay dùng trên MCU nhỏ để tránh phân mảnh heap).
 * Các buffer rảnh nằm trong queue xFreeQueue (cũng là queue con trỏ):
 *   Sender:   xQueueReceive( xFreeQueue )   -> lấy buffer rảnh
 *             xQueueSend( xPointerQueue )   -> chuyển quyền sở hữu cho Receiver
 *   Receiver: xQueueReceive( xPointerQueue) -> dùng buffer
 *             xQueueSend( xFreeQueue )      -> trả buffer về pool
 * Quan sát:
 *  - Sender block "chờ nhận từ xFreeQueue" khi cả 3 buffer đang được Receiver giữ;
 *  - chỉ có 3 địa chỉ buffer xuất hiện, lặp vòng;
 *  - Heap không đổi sau khi khởi động (không có malloc/free trong vòng lặp).
 */
#define POOL_SIZE    3
#define BUF_LEN      50

QueueHandle_t xPointerQueue, xFreeQueue;
static char ucPool[ POOL_SIZE ][ BUF_LEN ];

static void * prvGetBuffer( size_t xSize )
{
    char * pc;

    ( void ) xSize;
    xQueueReceive( xFreeQueue, &pc, portMAX_DELAY );
    return pc;
}

static void prvReleaseBuffer( void * pvBuffer )
{
    xQueueSend( xFreeQueue, &pvBuffer, 0 );    /* luôn có chỗ: queue dài bằng pool */
}

void vStringSendingTask( void * pvParameters )
{
    char * pcStringToSend;
    BaseType_t xStringNumber = 0;

    for( ;; )
    {
        pcStringToSend = ( char * ) prvGetBuffer( BUF_LEN );
        snprintf( pcStringToSend, BUF_LEN, "String number %d\r\n", ( int ) xStringNumber );
        xStringNumber++;
        xQueueSend( xPointerQueue, &pcStringToSend, portMAX_DELAY );
        vTaskDelay( pdMS_TO_TICKS( 100 ) );    /* thêm so với sách */
    }
}

void vStringReceivingTask( void * pvParameters )
{
    char * pcReceivedString;

    for( ;; )
    {
        xQueueReceive( xPointerQueue, &pcReceivedString, portMAX_DELAY );
        printf( "[%4lu ms] buffer %p -> ", ( unsigned long ) xTaskGetTickCount(), ( void * ) pcReceivedString );
        vPrintString( pcReceivedString );
        vTaskDelay( pdMS_TO_TICKS( 250 ) );    /* xử lý chậm hơn tốc độ gửi */
        prvReleaseBuffer( pcReceivedString );
    }
}

int main( void )
{
    int i;
    char * pc;

    xPointerQueue = xQueueCreate( POOL_SIZE, sizeof( char * ) );
    xFreeQueue = xQueueCreate( POOL_SIZE, sizeof( char * ) );
    for( i = 0; i < POOL_SIZE; i++ )
    {
        pc = ucPool[ i ];
        xQueueSend( xFreeQueue, &pc, 0 );
    }
    xTaskCreate( vStringSendingTask, "Sender", 1000, NULL, 1, NULL );
    xTaskCreate( vStringReceivingTask, "Receiver", 1000, NULL, 2, NULL );
    vTaskStartScheduler();
    for( ;; );
}
