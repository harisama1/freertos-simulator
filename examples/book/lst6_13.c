/* Listing 6.13 xTimerChangePeriod(): check timer tăng tốc nháy LED khi phát hiện lỗi.
 *
 * Check timer (auto-reload, 3000 ms) gọi CheckTasksAreRunningWithoutError()
 * rồi đảo LED. Task Worker tăng bộ đếm mỗi 100 ms; ở 2000 ms Worker "treo"
 * (tự suspend, mô phỏng lỗi). Kiểm tra ở 3000 ms còn thấy bộ đếm tăng (OK), ở 6000 ms thấy không đổi
 * -> xTimerChangePeriod( 200 ms ): LED nháy nhanh báo lỗi.
 * Callback chạy trong daemon task (Tmr Svc) nên dùng block time 0.
 * Quan sát: hàng LED PA5 đổi tần số sau 6000 ms; hàng xCheckTimer đổi chu kỳ.
 */
#define PIN_LED    5

const TickType_t xHealthyTimerPeriod = pdMS_TO_TICKS( 3000 );
const TickType_t xErrorTimerPeriod = pdMS_TO_TICKS( 200 );
static TimerHandle_t xCheckTimer;
static volatile uint32_t ulWorkerCount;

static void vWorker( void * pv )
{
    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
        ulWorkerCount++;
        if( xTaskGetTickCount() >= pdMS_TO_TICKS( 2000 ) )
        {
            printf( "[%4lu ms] Worker: treo (mô phỏng lỗi)\n", ( unsigned long ) xTaskGetTickCount() );
            vTaskSuspend( NULL );
        }
    }
}

static BaseType_t CheckTasksAreRunningWithoutError( void )
{
    static uint32_t ulLastCount = 0;
    BaseType_t xOk = ( ulWorkerCount != ulLastCount );

    ulLastCount = ulWorkerCount;
    return xOk ? pdPASS : pdFAIL;
}

static void ToggleLED( void )
{
    sim_gpio_toggle( PIN_LED );
}

static void prvCheckTimerCallbackFunction( TimerHandle_t xTimer )
{
    static BaseType_t xErrorDetected = pdFALSE;

    if( xErrorDetected == pdFALSE )
    {
        if( CheckTasksAreRunningWithoutError() == pdFAIL )
        {
            printf( "[%4lu ms] check: LỖI -> đổi chu kỳ 3000 -> 200 ms\n", ( unsigned long ) xTaskGetTickCount() );
            xTimerChangePeriod( xTimer, xErrorTimerPeriod, 0 );
            xErrorDetected = pdTRUE;
        }
        else
        {
            printf( "[%4lu ms] check: OK (Worker count = %lu)\n", ( unsigned long ) xTaskGetTickCount(), ( unsigned long ) ulWorkerCount );
        }
    }
    ToggleLED();
}

int main( void )
{
    sim_gpio_name( PIN_LED, "LED PA5" );
    xCheckTimer = xTimerCreate( "Check", xHealthyTimerPeriod, pdTRUE, NULL, prvCheckTimerCallbackFunction );
    xTimerStart( xCheckTimer, 0 );
    xTaskCreate( vWorker, "Worker", 1000, NULL, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
