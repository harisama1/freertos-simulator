/* Listing 8.16 Gọi taskYIELD() sau khi trả mutex để hai task cùng ưu tiên chia đều.
 *
 * Như lst8_15.c, thêm: nếu tick count đã đổi trong lúc giữ mutex (tức time
 * slice đã hết) thì taskYIELD() sau xSemaphoreGive(), nhường CPU cho task
 * đang chờ mutex. Không yield mỗi vòng để tránh chuyển task quá dày.
 * Kết quả: số lần ghi của hai task xấp xỉ nhau.
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
#if 1
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
