/* Listing 8.14 Recursive mutex: task đang giữ mutex lấy lại chính mutex đó.
 *
 * prvLog() lấy xRecursiveMutex rồi gọi prvLogHeader(), hàm này cũng lấy
 * xRecursiveMutex (trường hợp thực tế: hàm con dùng chung tài nguyên).
 * Mỗi xSemaphoreTakeRecursive() tăng bộ đếm lồng nhau; mutex chỉ thực sự
 * được trả khi số lần Give bằng số lần Take.
 * Hai task Logger1/Logger2 (cùng p1) gọi prvLog() mỗi 300 ms / 450 ms.
 * Quan sát: hàng xRecursiveMutex trên Timeline cho biết task đang giữ; một
 * Logger có lúc block "chờ lấy xRecursiveMutex" cho tới khi Give lần thứ 2.
 * Thử: đổi thành xSemaphoreCreateMutex() + xSemaphoreTake/Give thường ->
 * lần Take thứ hai trong cùng task tự block chính nó: hết 20 ms trả pdFAIL
 * (với portMAX_DELAY sẽ deadlock).
 */
static SemaphoreHandle_t xRecursiveMutex;
const TickType_t xMaxBlock20ms = pdMS_TO_TICKS( 20 );

static void prvBusyMs( uint32_t ms )
{
    TickType_t xEnd = xTaskGetTickCount() + pdMS_TO_TICKS( ms );

    while( xTaskGetTickCount() < xEnd )
        ;
}

static void prvLogHeader( void )
{
    if( xSemaphoreTakeRecursive( xRecursiveMutex, xMaxBlock20ms ) == pdPASS )   /* lồng lần 2 */
    {
        printf( "[%4lu ms] %s: header (lồng 2 lớp)\n", ( unsigned long ) xTaskGetTickCount(), pcTaskGetName( NULL ) );
        xSemaphoreGiveRecursive( xRecursiveMutex );                              /* đếm 2 -> 1, chưa trả */
    }
    else
    {
        printf( "[%4lu ms] %s: Take lần 2 THẤT BẠI\n", ( unsigned long ) xTaskGetTickCount(), pcTaskGetName( NULL ) );
    }
}

static void prvLog( const char * pcMsg )
{
    if( xSemaphoreTakeRecursive( xRecursiveMutex, xMaxBlock20ms ) == pdPASS )   /* lần 1 */
    {
        prvLogHeader();
        printf( "[%4lu ms] %s: %s\n", ( unsigned long ) xTaskGetTickCount(), pcTaskGetName( NULL ), pcMsg );
        prvBusyMs( 10 );                                                         /* giữ mutex 10 ms */
        xSemaphoreGiveRecursive( xRecursiveMutex );                              /* đếm 1 -> 0, trả mutex */
    }
    else
    {
        printf( "[%4lu ms] %s: hết 20 ms vẫn không lấy được mutex\n", ( unsigned long ) xTaskGetTickCount(), pcTaskGetName( NULL ) );
    }
}

static void vLogger( void * pv )
{
    const TickType_t xPeriod = pdMS_TO_TICKS( ( uint32_t ) ( uintptr_t ) pv );

    for( ;; )
    {
        prvLog( "body" );
        vTaskDelay( xPeriod );
    }
}

int main( void )
{
    xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
    xTaskCreate( vLogger, "Logger1", 1000, ( void * ) 300, 1, NULL );
    xTaskCreate( vLogger, "Logger2", 1000, ( void * ) 450, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
