/* Example 9.1 Experimenting with event groups (xWaitForAllBits = pdFALSE). */
#define mainINTERRUPT_NUMBER 3
#define mainFIRST_TASK_BIT  ( 1UL << 0UL )
#define mainSECOND_TASK_BIT ( 1UL << 1UL )
#define mainISR_BIT         ( 1UL << 2UL )
EventGroupHandle_t xEventGroup;
static void vEventBitSettingTask( void * pvParameters )
{
    const TickType_t xDelay200ms = pdMS_TO_TICKS( 200UL );
    for( ;; )
    {
        vTaskDelay( xDelay200ms );
        vPrintString( "Bit setting task -\t about to set bit 0.\r\n" );
        xEventGroupSetBits( xEventGroup, mainFIRST_TASK_BIT );
        vTaskDelay( xDelay200ms );
        vPrintString( "Bit setting task -\t about to set bit 1.\r\n" );
        xEventGroupSetBits( xEventGroup, mainSECOND_TASK_BIT );
    }
}
static void vPrintStringFromDaemonTask( void * pvParameter1, uint32_t ulParameter2 )
{
    vPrintString( ( const char * ) pvParameter1 );
}
static uint32_t ulEventBitSettingISR( void )
{
    static const char * pcString = "Bit setting ISR -\t about to set bit 2.\r\n";
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerPendFunctionCallFromISR( vPrintStringFromDaemonTask, ( void * ) pcString, 0, &xHigherPriorityTaskWoken );
    xEventGroupSetBitsFromISR( xEventGroup, mainISR_BIT, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
static void vEventBitReadingTask( void * pvParameters )
{
    EventBits_t xEventGroupValue;
    const EventBits_t xBitsToWaitFor = ( mainFIRST_TASK_BIT | mainSECOND_TASK_BIT | mainISR_BIT );
    for( ;; )
    {
        xEventGroupValue = xEventGroupWaitBits( xEventGroup, xBitsToWaitFor, pdTRUE, pdFALSE, portMAX_DELAY );
        if( ( xEventGroupValue & mainFIRST_TASK_BIT ) != 0 )
            vPrintString( "Bit reading task -\t Event bit 0 was set\r\n" );
        if( ( xEventGroupValue & mainSECOND_TASK_BIT ) != 0 )
            vPrintString( "Bit reading task -\t Event bit 1 was set\r\n" );
        if( ( xEventGroupValue & mainISR_BIT ) != 0 )
            vPrintString( "Bit reading task -\t Event bit 2 was set\r\n" );
    }
}
static void vInterruptGenerator( void * pvParameters )
{
    const TickType_t xDelay500ms = pdMS_TO_TICKS( 500UL );
    for( ;; )
    {
        vTaskDelay( xDelay500ms );
        vPortGenerateSimulatedInterrupt( mainINTERRUPT_NUMBER );
    }
}
int main( void )
{
    xEventGroup = xEventGroupCreate();
    xTaskCreate( vEventBitSettingTask, "Bit Setter", 1000, NULL, 1, NULL );
    xTaskCreate( vEventBitReadingTask, "Bit Reader", 1000, NULL, 2, NULL );
    xTaskCreate( vInterruptGenerator, "Int Gen", 1000, NULL, 3, NULL );
    vPortSetInterruptHandler( mainINTERRUPT_NUMBER, ulEventBitSettingISR );
    vTaskStartScheduler();
    for( ;; );
}
