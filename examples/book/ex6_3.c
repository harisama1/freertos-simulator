/* Example 6.3 Resetting a software timer (backlight). 
 * _kbhit() của Windows được thay bằng phím giả lập: nhấn tại 1000, 3000, 3500 ms. */
#define mainBACKLIGHT_TIMER_PERIOD  pdMS_TO_TICKS( 5000UL )
#define PIN_KEY  0
static TimerHandle_t xBacklightTimer;
static BaseType_t xSimulatedBacklightOn = pdFALSE;
static int _kbhit( void )
{
    static int iLast = 1;
    int iNow = sim_gpio_read( PIN_KEY ), iHit = ( iLast == 1 && iNow == 0 );
    iLast = iNow;
    return iHit;
}
static void prvBacklightTimerCallback( TimerHandle_t xTimer )
{
    TickType_t xTimeNow = xTaskGetTickCount();
    xSimulatedBacklightOn = pdFALSE;
    vPrintStringAndNumber( "Timer expired, turning backlight OFF at time\t\t", xTimeNow );
}
static void vKeyHitTask( void * pvParameters )
{
    const TickType_t xShortDelay = pdMS_TO_TICKS( 50 );
    TickType_t xTimeNow;
    vPrintString( "Press a key to turn the backlight on.\r\n" );
    for( ;; )
    {
        if( _kbhit() != 0 )
        {
            xTimeNow = xTaskGetTickCount();
            if( xSimulatedBacklightOn == pdFALSE )
            {
                xSimulatedBacklightOn = pdTRUE;
                vPrintStringAndNumber( "Key pressed, turning backlight ON at time\t\t", xTimeNow );
            }
            else
            {
                vPrintStringAndNumber( "Key pressed, resetting software timer at time\t\t", xTimeNow );
            }
            xTimerReset( xBacklightTimer, xShortDelay );
        }
        vTaskDelay( xShortDelay );
    }
}
int main( void )
{
    xBacklightTimer = xTimerCreate( "Backlight", mainBACKLIGHT_TIMER_PERIOD, pdFALSE, 0, prvBacklightTimerCallback );
    xTaskCreate( vKeyHitTask, "Key poll", 1000, NULL, 1, NULL );
    sim_gpio_name( PIN_KEY, "KEY" );
    sim_gpio_write( PIN_KEY, 1 );   /* phím nhả = 1 */
    sim_gpio_input( PIN_KEY, "0:1 1000:0 1100:1 3000:0 3100:1 3500:0 3600:1", -1 );
    xTimerStart( xBacklightTimer, 0 );
    vTaskStartScheduler();
    for( ;; );
}
