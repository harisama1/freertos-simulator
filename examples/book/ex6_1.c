/* Example 6.1 Creating one-shot and auto-reload timers. */
#define mainONE_SHOT_TIMER_PERIOD     pdMS_TO_TICKS( 3333 )
#define mainAUTO_RELOAD_TIMER_PERIOD  pdMS_TO_TICKS( 500 )
static void prvOneShotTimerCallback( TimerHandle_t xTimer )
{
    vPrintStringAndNumber( "One-shot timer callback executing", xTaskGetTickCount() );
}
static void prvAutoReloadTimerCallback( TimerHandle_t xTimer )
{
    vPrintStringAndNumber( "Auto-reload timer callback executing", xTaskGetTickCount() );
}
int main( void )
{
    TimerHandle_t xAutoReloadTimer, xOneShotTimer;
    BaseType_t xTimer1Started, xTimer2Started;
    xOneShotTimer = xTimerCreate( "OneShot", mainONE_SHOT_TIMER_PERIOD, pdFALSE, 0, prvOneShotTimerCallback );
    xAutoReloadTimer = xTimerCreate( "AutoReload", mainAUTO_RELOAD_TIMER_PERIOD, pdTRUE, 0, prvAutoReloadTimerCallback );
    if( ( xOneShotTimer != NULL ) && ( xAutoReloadTimer != NULL ) )
    {
        xTimer1Started = xTimerStart( xOneShotTimer, 0 );
        xTimer2Started = xTimerStart( xAutoReloadTimer, 0 );
        if( ( xTimer1Started == pdPASS ) && ( xTimer2Started == pdPASS ) )
            vTaskStartScheduler();
    }
    for( ;; );
}
