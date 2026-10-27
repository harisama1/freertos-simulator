/*
 * Chương 8: priority inversion và priority inheritance với mutex.
 *   Low  (p1) giữ xMutex 30 ms;
 *   High (p3) cần xMutex ở t = 10 ms;
 *   Med  (p2) chạy CPU-bound 40 ms, bắt đầu ở t = 15 ms.
 *
 *
 * Quan sát trên Timeline (bấm "Toàn bộ"):
 *  - hàng xMutex: thanh màu của task đang giữ mutex;
 *  - khi High block "chờ lấy xMutex", Low được nâng lên p3 (vòng tròn
 *    đánh dấu inherit) nên Med KHÔNG chen vào được;
 *  - Low trả mutex -> về p1, High chạy ngay.
 * Thử: đổi xSemaphoreCreateMutex() thành xSemaphoreCreateBinary() và give
 * một lần trước khi chạy: không còn kế thừa ưu tiên, High phải chờ cả Med
 * (priority inversion không chặn).
 */
static SemaphoreHandle_t xMutex;

static void prvBusyMs( uint32_t ms )
{
    TickType_t xEnd = xTaskGetTickCount() + pdMS_TO_TICKS( ms );

    while( xTaskGetTickCount() < xEnd )
        ;
}

static void vLow( void * pv )
{
    for( ;; )
    {
        xSemaphoreTake( xMutex, portMAX_DELAY );
        printf( "[%3lu] Low: lấy mutex\n", ( unsigned long ) xTaskGetTickCount() );
        prvBusyMs( 30 );
        printf( "[%3lu] Low: trả mutex\n", ( unsigned long ) xTaskGetTickCount() );
        xSemaphoreGive( xMutex );
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}

static void vMed( void * pv )
{
    vTaskDelay( pdMS_TO_TICKS( 15 ) );
    for( ;; )
    {
        printf( "[%3lu] Med: bắt đầu 40 ms tính toán\n", ( unsigned long ) xTaskGetTickCount() );
        prvBusyMs( 40 );
        printf( "[%3lu] Med: xong\n", ( unsigned long ) xTaskGetTickCount() );
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}

static void vHigh( void * pv )
{
    vTaskDelay( pdMS_TO_TICKS( 10 ) );
    for( ;; )
    {
        printf( "[%3lu] High: cần mutex\n", ( unsigned long ) xTaskGetTickCount() );
        xSemaphoreTake( xMutex, portMAX_DELAY );
        printf( "[%3lu] High: có mutex\n", ( unsigned long ) xTaskGetTickCount() );
        prvBusyMs( 5 );
        xSemaphoreGive( xMutex );
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}

int main( void )
{
    xMutex = xSemaphoreCreateMutex();
    xTaskCreate( vLow, "Low", 256, NULL, 1, NULL );
    xTaskCreate( vMed, "Med", 256, NULL, 2, NULL );
    xTaskCreate( vHigh, "High", 256, NULL, 3, NULL );
    vTaskStartScheduler();
    for( ;; );
}
