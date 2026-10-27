/*
 * Chương 6, Example 6.1/6.2: một timer one-shot (3,333 s) và một timer
 * auto-reload (500 ms). Callback chạy trong ngữ cảnh task "Tmr Svc"
 * (daemon task, ưu tiên configTIMER_TASK_PRIORITY).
 *
 *
 * Quan sát trên Timeline:
 *  - hàng xAutoReloadTimer có vạch hết hạn mỗi 500 ms, xOneShotTimer một
 *    vạch duy nhất ở 3333 ms;
 *  - Tmr Svc chạy ngắn tại đúng các thời điểm đó;
 *  - ID của timer dùng làm bộ đếm: auto-reload tự dừng sau 5 lần.
 */
#define mainONE_SHOT_TIMER_PERIOD      pdMS_TO_TICKS( 3333 )
#define mainAUTO_RELOAD_TIMER_PERIOD   pdMS_TO_TICKS( 500 )

static void prvTimerCallback( TimerHandle_t xTimer );

TimerHandle_t xOneShotTimer, xAutoReloadTimer;

int main( void )
{
    BaseType_t xTimer1Started, xTimer2Started;

    xOneShotTimer = xTimerCreate( "OneShot", mainONE_SHOT_TIMER_PERIOD, pdFALSE, 0, prvTimerCallback );
    xAutoReloadTimer = xTimerCreate( "AutoReload", mainAUTO_RELOAD_TIMER_PERIOD, pdTRUE, 0, prvTimerCallback );

    if( ( xOneShotTimer != NULL ) && ( xAutoReloadTimer != NULL ) )
    {
        xTimer1Started = xTimerStart( xOneShotTimer, 0 );
        xTimer2Started = xTimerStart( xAutoReloadTimer, 0 );

        if( ( xTimer1Started == pdPASS ) && ( xTimer2Started == pdPASS ) )
            vTaskStartScheduler();
    }

    for( ;; );
}

static void prvTimerCallback( TimerHandle_t xTimer )
{
    TickType_t xTimeNow = xTaskGetTickCount();
    uint32_t ulExecutionCount = ( uint32_t ) ( uintptr_t ) pvTimerGetTimerID( xTimer );

    ulExecutionCount++;
    vTimerSetTimerID( xTimer, ( void * ) ( uintptr_t ) ulExecutionCount );

    if( xTimer == xOneShotTimer )
    {
        vPrintStringAndNumber( "One-shot timer callback executing", xTimeNow );
    }
    else
    {
        vPrintStringAndNumber( "Auto-reload timer callback executing", xTimeNow );
        if( ulExecutionCount == 5 )
            xTimerStop( xTimer, 0 );   /* trong callback: block time phải là 0 */
    }
}
