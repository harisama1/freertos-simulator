/* Example 6.2 Using the callback function parameter and the software timer ID. */
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
