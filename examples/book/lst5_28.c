/* Listing 5.28-5.32 Mailbox: queue độ dài 1 với xQueueOverwrite() và xQueuePeek().
 *
 * Mailbox giữ giá trị mới nhất; dữ liệu không "đi qua" mà nằm lại cho mọi
 * task đọc:
 *   - Writer ghi giá trị cảm biến mỗi 500 ms bằng xQueueOverwrite()
 *     (không bao giờ block, ghi đè giá trị cũ);
 *   - ReaderA (mỗi 200 ms) và ReaderB (mỗi 350 ms) đọc bằng xQueuePeek(),
 *     không lấy item ra, nên cả hai cùng thấy một giá trị;
 *   - vReadMailbox() so sánh time stamp để biết dữ liệu có mới không.
 * Quan sát:
 *  - lúc đầu mailbox rỗng: hai Reader block "chờ nhận từ xMailbox" tới 500 ms;
 *  - hàng xMailbox luôn ở mức 1 sau lần ghi đầu;
 *  - ReaderA thấy "cũ" khoảng 1-2 lần giữa hai lần ghi.
 */
typedef struct xExampleStructure
{
    TickType_t xTimeStamp;
    uint32_t ulValue;
} Example_t;

QueueHandle_t xMailbox;

void vUpdateMailbox( uint32_t ulNewValue )
{
    Example_t xData;

    xData.ulValue = ulNewValue;
    xData.xTimeStamp = xTaskGetTickCount();
    xQueueOverwrite( xMailbox, &xData );
}

BaseType_t vReadMailbox( Example_t * pxData )
{
    TickType_t xPreviousTimeStamp;
    BaseType_t xDataUpdated;

    xPreviousTimeStamp = pxData->xTimeStamp;
    xQueuePeek( xMailbox, pxData, portMAX_DELAY );

    if( pxData->xTimeStamp > xPreviousTimeStamp )
        xDataUpdated = pdTRUE;
    else
        xDataUpdated = pdFALSE;

    return xDataUpdated;
}

static void vWriter( void * pv )
{
    uint32_t ulSensor = 20;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
        ulSensor += 3;
        vUpdateMailbox( ulSensor );
    }
}

static void vReader( void * pv )
{
    const TickType_t xPeriod = pdMS_TO_TICKS( ( uint32_t ) ( uintptr_t ) pv );
    Example_t xData = { 0, 0 };

    for( ;; )
    {
        BaseType_t xNew = vReadMailbox( &xData );

        printf( "[%4lu ms] %s: value = %lu (ghi lúc %lu ms) %s\n", ( unsigned long ) xTaskGetTickCount(),
                pcTaskGetName( NULL ), ( unsigned long ) xData.ulValue, ( unsigned long ) xData.xTimeStamp,
                xNew ? "MỚI" : "cũ" );
        vTaskDelay( xPeriod );
    }
}

int main( void )
{
    xMailbox = xQueueCreate( 1, sizeof( Example_t ) );
    xTaskCreate( vWriter, "Writer", 1000, NULL, 3, NULL );
    xTaskCreate( vReader, "ReaderA", 1000, ( void * ) 200, 2, NULL );
    xTaskCreate( vReader, "ReaderB", 1000, ( void * ) 350, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
