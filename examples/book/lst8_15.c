/* Listing 8.15 Hai task cùng ưu tiên dùng mutex trong vòng lặp chặt: một task gần như bị bỏ đói.
 *
 * Mỗi vòng: tạo chuỗi (nhanh), Take mutex, ghi màn hình (chậm, 1500 us),
 * Give. Task giữ mutex gần như suốt time slice. Khi tick chuyển sang task
 * kia, task kia block "chờ lấy xMutex". Lúc Give, task kia được đánh thức
 * nhưng cùng ưu tiên nên KHÔNG chiếm quyền; task đang chạy lấy lại mutex
 * ngay (Figure 8.7). Task kia chỉ lấy được mutex khi tick rơi đúng vào
 * khoảng ngắn mutex rảnh.
 * Kết quả (in mỗi 250 ms): theo từng pha, một task độc chiếm mutex, task
 * kia gần như 0 lần, cho tới khi tick rơi đúng khe hở. Trên Timeline: một
 * hàng chạy liền, hàng kia chỉ có các vạch ngắn rồi block. Sửa: lst8_16.c.
 */
SemaphoreHandle_t xMutex;
static volatile uint32_t ulTurns[ 2 ];

static void prvBusyUs( uint32_t us )            /* chiếm CPU khoảng us micro giây */
{
    unsigned long ulStart = ulSimRunTimeUs();

    while( ulSimRunTimeUs() - ulStart < us )
        ;
}

static void vGenerateTextInALocalBuffer( char * pc )
{
    snprintf( pc, 128, "frame of %s", pcTaskGetName( NULL ) );   /* nhanh */
}

static void vCopyTextToFrameBuffer( const char * pc )
{
    ( void ) pc;
    prvBusyUs( 1500 );                                           /* chậm: dài hơn 1 tick */
}

void vFunction( void * pvParameter )
{
    const uint32_t i = ( uint32_t ) ( uintptr_t ) pvParameter;
    char cTextBuffer[ 128 ];
    TickType_t xTimeAtWhichMutexWasTaken;

    for( ;; )
    {
        vGenerateTextInALocalBuffer( cTextBuffer );
        xSemaphoreTake( xMutex, portMAX_DELAY );
        xTimeAtWhichMutexWasTaken = xTaskGetTickCount();
        vCopyTextToFrameBuffer( cTextBuffer );
        xSemaphoreGive( xMutex );
        ulTurns[ i ]++;
#if 0
        if( xTaskGetTickCount() != xTimeAtWhichMutexWasTaken )
        {
            taskYIELD();
        }
#else
        ( void ) xTimeAtWhichMutexWasTaken;
#endif
    }
}

static void vMonitor( void * pv )
{
    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 250 ) );
        printf( "[%5lu ms] số lần ghi màn hình: Task 1 = %lu, Task 2 = %lu\n", ( unsigned long ) xTaskGetTickCount(),
                ( unsigned long ) ulTurns[ 0 ], ( unsigned long ) ulTurns[ 1 ] );
        ulTurns[ 0 ] = ulTurns[ 1 ] = 0;
    }
}

int main( void )
{
    xMutex = xSemaphoreCreateMutex();
    xTaskCreate( vFunction, "Task 1", 1000, ( void * ) 0, 1, NULL );
    xTaskCreate( vFunction, "Task 2", 1000, ( void * ) 1, 1, NULL );
    xTaskCreate( vMonitor, "Monitor", 1000, NULL, 2, NULL );
    vTaskStartScheduler();
    for( ;; );
}
