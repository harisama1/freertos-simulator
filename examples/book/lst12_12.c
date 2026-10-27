/* Listing 12.4-12.12 Thống kê thời gian chạy: vTaskGetRunTimeStatistics() và vTaskListTasks().
 *
 * configGENERATE_RUN_TIME_STATS = 1: kernel cộng dồn thời gian chạy của mỗi
 * task theo bộ đếm portGET_RUN_TIME_COUNTER_VALUE() (trên MCU là timer phần
 * cứng chạy nhanh hơn tick 10-100 lần, Listing 12.10-12.11; ở đây là đồng hồ
 * micro giây của máy).
 * Ba task tải khác nhau: Heavy (bận ~60% mỗi 10 ms), Light (~10%), Sleeper
 * (gần như chỉ ngủ). prvStatsTask in bảng mỗi 2500 ms (sách: 5000 ms).
 * vTaskListTasks(): trạng thái X=Running B=Blocked R=Ready D=Deleted
 * S=Suspended, ưu tiên, stack còn trống (high water mark), số thứ tự task.
 * So sánh cột % với cột cpu % của tab Thống kê.
 * Lưu ý: cột Stack ở simulator gần như không đổi (task chạy trên stack của
 * pthread, không dùng stack FreeRTOS cấp); nhu cầu stack thật trên MCU xem
 * bảng ước lượng tĩnh ở tab Thống kê.
 */
#define RUN_TIME_STATS_STRING_BUFFER_LENGTH    1024

static void prvBusyMs( uint32_t ms )
{
    TickType_t xEnd = xTaskGetTickCount() + pdMS_TO_TICKS( ms );

    while( xTaskGetTickCount() < xEnd )
        ;
}

static void vLoad( void * pv )
{
    const uint32_t ulBusy = ( uint32_t ) ( uintptr_t ) pv;
    TickType_t xLast = xTaskGetTickCount();

    for( ;; )
    {
        prvBusyMs( ulBusy );
        vTaskDelayUntil( &xLast, pdMS_TO_TICKS( 10 ) );
    }
}

static void vPrintMultipleLines( const char * pc )
{
    printf( "%s", pc );
}

static void prvStatsTask( void * pvParameters )
{
    TickType_t xLastExecutionTime;
    static char cStringBuffer[ RUN_TIME_STATS_STRING_BUFFER_LENGTH ];
    const TickType_t xBlockPeriod = pdMS_TO_TICKS( 2500 );

    xLastExecutionTime = xTaskGetTickCount();

    for( ;; )
    {
        xTaskDelayUntil( &xLastExecutionTime, xBlockPeriod );

        vTaskGetRunTimeStatistics( cStringBuffer, RUN_TIME_STATS_STRING_BUFFER_LENGTH );
        printf( "\n[%lu ms] Run-time stats\nTask\t\tAbs (us)\t%%\n", ( unsigned long ) xTaskGetTickCount() );
        printf( "-------------------------------------------\n" );
        vPrintMultipleLines( cStringBuffer );

        vTaskListTasks( cStringBuffer, RUN_TIME_STATS_STRING_BUFFER_LENGTH );
        printf( "\nTask\t\tState\tPrio\tStack\tNum\n" );
        printf( "-------------------------------------------\n" );
        vPrintMultipleLines( cStringBuffer );
    }
}

int main( void )
{
    xTaskCreate( vLoad, "Heavy", 1000, ( void * ) 6, 2, NULL );
    xTaskCreate( vLoad, "Light", 1000, ( void * ) 1, 3, NULL );
    xTaskCreate( vLoad, "Sleeper", 1000, ( void * ) 0, 1, NULL );
    xTaskCreate( prvStatsTask, "Stats", 1000, NULL, 4, NULL );
    vTaskStartScheduler();
    for( ;; );
}
