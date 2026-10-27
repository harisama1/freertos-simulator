/* Listing 5.27 Queue set chứa hai queue và một binary semaphore.
 *
 * Thành viên của xQueueSet:
 *   xCharPointerQueue  queue con trỏ char *   (task StrTx gửi mỗi 300 ms)
 *   xUint32tQueue      queue uint32_t         (task NumTx gửi mỗi 450 ms)
 *   xBinarySemaphore   ISR TIM6 give mỗi 1000 ms
 * vAMoreRealisticReceiverTask chờ trên set tối đa 100 ms; giá trị trả về là
 * handle của thành viên có dữ liệu (NULL = hết giờ), sau đó đọc chính
 * thành viên đó với block time 0.
 * Độ dài set = tổng độ dài các thành viên = 2 + 2 + 1.
 * Quan sát: Receiver block "chờ queue set"; mỗi dòng in ra thành viên được chọn.
 */
#define TIM6_IRQ    5

QueueHandle_t xCharPointerQueue;
QueueHandle_t xUint32tQueue;
SemaphoreHandle_t xBinarySemaphore;
QueueSetHandle_t xQueueSet;

static void TIM6_IRQHandler( void )
{
    BaseType_t xWoken = pdFALSE;

    xSemaphoreGiveFromISR( xBinarySemaphore, &xWoken );
    portYIELD_FROM_ISR( xWoken );
}

static void vStrTx( void * pv )
{
    static const char * const pcMsgs[] = { "alpha", "beta", "gamma" };
    const char * pc;
    uint32_t i = 0;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 300 ) );
        pc = pcMsgs[ i++ % 3 ];
        xQueueSend( xCharPointerQueue, &pc, 0 );
    }
}

static void vNumTx( void * pv )
{
    uint32_t ul = 100;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 450 ) );
        xQueueSend( xUint32tQueue, &ul, 0 );
        ul++;
    }
}

void vAMoreRealisticReceiverTask( void * pvParameters )
{
    QueueSetMemberHandle_t xHandle;
    char * pcReceivedString;
    uint32_t ulRecievedValue;
    const TickType_t xDelay100ms = pdMS_TO_TICKS( 100 );
    uint32_t ulTimeouts = 0;

    for( ;; )
    {
        xHandle = xQueueSelectFromSet( xQueueSet, xDelay100ms );

        if( xHandle == NULL )
        {
            ulTimeouts++;                       /* hết 100 ms, không thành viên nào có dữ liệu */
        }
        else if( xHandle == ( QueueSetMemberHandle_t ) xCharPointerQueue )
        {
            xQueueReceive( xCharPointerQueue, &pcReceivedString, 0 );
            printf( "[%4lu ms] xCharPointerQueue: \"%s\"   (timeout trước đó: %lu)\n",
                    ( unsigned long ) xTaskGetTickCount(), pcReceivedString, ( unsigned long ) ulTimeouts );
            ulTimeouts = 0;
        }
        else if( xHandle == ( QueueSetMemberHandle_t ) xUint32tQueue )
        {
            xQueueReceive( xUint32tQueue, &ulRecievedValue, 0 );
            printf( "[%4lu ms] xUint32tQueue:     %lu      (timeout trước đó: %lu)\n",
                    ( unsigned long ) xTaskGetTickCount(), ( unsigned long ) ulRecievedValue, ( unsigned long ) ulTimeouts );
            ulTimeouts = 0;
        }
        else if( xHandle == ( QueueSetMemberHandle_t ) xBinarySemaphore )
        {
            xSemaphoreTake( xBinarySemaphore, 0 );
            printf( "[%4lu ms] xBinarySemaphore: take (ngắt TIM6)\n", ( unsigned long ) xTaskGetTickCount() );
        }
    }
}

int main( void )
{
    xCharPointerQueue = xQueueCreate( 2, sizeof( char * ) );
    xUint32tQueue = xQueueCreate( 2, sizeof( uint32_t ) );
    xBinarySemaphore = xSemaphoreCreateBinary();
    xQueueSet = xQueueCreateSet( 2 + 2 + 1 );
    xQueueAddToSet( xCharPointerQueue, xQueueSet );
    xQueueAddToSet( xUint32tQueue, xQueueSet );
    xQueueAddToSet( xBinarySemaphore, xQueueSet );

    xTaskCreate( vStrTx, "StrTx", 1000, NULL, 1, NULL );
    xTaskCreate( vNumTx, "NumTx", 1000, NULL, 1, NULL );
    xTaskCreate( vAMoreRealisticReceiverTask, "Receiver", 1000, NULL, 2, NULL );
    sim_irq_attach( TIM6_IRQ, "TIM6", TIM6_IRQHandler );
    sim_irq_every( TIM6_IRQ, 1000, 1000 );
    vTaskStartScheduler();
    for( ;; );
}
