/* Listing 10.14-10.18 Server task: request qua queue, trả lời bằng task notification.
 *
 * Task ServerTask là nơi duy nhất nói chuyện với "cloud server". Task
 * ứng dụng gửi CloudCommand_t (kèm handle của chính nó) vào xServerTaskQueue
 * rồi block bằng xTaskNotifyWait() tối đa 250 ms:
 *   - CloudRead():  server trả dữ liệu bằng xTaskNotify( eSetValueWithOverwrite );
 *   - CloudWrite(): server trả mã trạng thái bit bằng xTaskNotify( eSetBits ),
 *     chỉ chạm 4 bit CLOUD_WRITE_STATUS_BIT_MASK.
 * Không cần queue trả lời riêng cho mỗi task: notification chính là "hộp
 * thư" của task.
 * Giả lập độ trễ mạng: GetCloudData() 20 ms; SetCloudData() 20 ms, riêng lần
 * ghi thứ 3 mất 300 ms -> CloudWrite() hết 250 ms, trả 0.
 * Quan sát: Sensor và Display block "chờ task notification"; ServerTask
 * block "chờ nhận từ xServerTaskQueue".
 */
typedef enum
{
    eRead,
    eWrite
} Operation_t;

typedef struct CloudCommand
{
    Operation_t eOperation;
    uint32_t ulDataID;
    uint32_t ulDataValue;
    TaskHandle_t xTaskToNotify;
} CloudCommand_t;

#define SEND_SUCCESSFUL_BIT               ( 0x01 << 0 )
#define OPERATION_TIMED_OUT_BIT           ( 0x01 << 1 )
#define NO_INTERNET_CONNECTION_BIT        ( 0x01 << 2 )
#define CANNOT_LOCATE_CLOUD_SERVER_BIT    ( 0x01 << 3 )
#define CLOUD_WRITE_STATUS_BIT_MASK                                   \
    ( SEND_SUCCESSFUL_BIT | OPERATION_TIMED_OUT_BIT |                 \
      NO_INTERNET_CONNECTION_BIT | CANNOT_LOCATE_CLOUD_SERVER_BIT )

static QueueHandle_t xServerTaskQueue;
static uint32_t ulCloudStore[ 4 ] = { 7, 0, 0, 0 };

/* ---- "cloud" giả lập ---- */
static uint32_t GetCloudData( uint32_t ulDataID )
{
    vTaskDelay( pdMS_TO_TICKS( 20 ) );
    return ulCloudStore[ ulDataID & 3 ];
}

static uint32_t SetCloudData( uint32_t ulDataID, uint32_t ulDataValue )
{
    static uint32_t ulWrites = 0;

    vTaskDelay( pdMS_TO_TICKS( ( ++ulWrites == 3 ) ? 300 : 20 ) );
    ulCloudStore[ ulDataID & 3 ] = ulDataValue;
    return SEND_SUCCESSFUL_BIT;
}

/* ---- API phía ứng dụng (Listing 10.15, 10.17) ---- */
BaseType_t CloudRead( uint32_t ulDataID, uint32_t * pulValue )
{
    CloudCommand_t xRequest;

    xRequest.eOperation = eRead;
    xRequest.ulDataID = ulDataID;
    xRequest.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xTaskNotifyWait( 0, 0, NULL, 0 );
    xQueueSend( xServerTaskQueue, &xRequest, portMAX_DELAY );
    return xTaskNotifyWait( 0, 0, pulValue, pdMS_TO_TICKS( 250 ) );
}

uint32_t CloudWrite( uint32_t ulDataID, uint32_t ulDataValue )
{
    CloudCommand_t xRequest;
    uint32_t ulNotificationValue = 0;

    xRequest.eOperation = eWrite;
    xRequest.ulDataID = ulDataID;
    xRequest.ulDataValue = ulDataValue;
    xRequest.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xTaskNotifyWait( 0, CLOUD_WRITE_STATUS_BIT_MASK, NULL, 0 );
    xQueueSend( xServerTaskQueue, &xRequest, portMAX_DELAY );
    xTaskNotifyWait( 0, CLOUD_WRITE_STATUS_BIT_MASK, &ulNotificationValue, pdMS_TO_TICKS( 250 ) );
    return ( ulNotificationValue & CLOUD_WRITE_STATUS_BIT_MASK );
}

/* ---- server (Listing 10.16 + 10.18) ---- */
void ServerTask( void * pvParameters )
{
    CloudCommand_t xCommand;
    uint32_t ulReceivedValue, ulBitwiseStatusCode;

    for( ;; )
    {
        xQueueReceive( xServerTaskQueue, &xCommand, portMAX_DELAY );

        switch( xCommand.eOperation )
        {
            case eRead:
                ulReceivedValue = GetCloudData( xCommand.ulDataID );
                xTaskNotify( xCommand.xTaskToNotify, ulReceivedValue, eSetValueWithOverwrite );
                break;
            case eWrite:
                ulBitwiseStatusCode = SetCloudData( xCommand.ulDataID, xCommand.ulDataValue );
                xTaskNotify( xCommand.xTaskToNotify, ulBitwiseStatusCode, eSetBits );
                break;
        }
    }
}

/* ---- hai task ứng dụng ---- */
static void vSensor( void * pv )
{
    uint32_t ulTemp = 25, ulStatus;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 400 ) );
        ulStatus = CloudWrite( 1, ulTemp );
        printf( "[%4lu ms] Sensor : CloudWrite(id 1, %lu) -> status 0x%lx %s\n", ( unsigned long ) xTaskGetTickCount(),
                ( unsigned long ) ulTemp, ( unsigned long ) ulStatus, ulStatus & SEND_SUCCESSFUL_BIT ? "OK" : "(timeout)" );
        ulTemp++;
    }
}

static void vDisplay( void * pv )
{
    uint32_t ulValue = 0;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 300 ) );
        if( CloudRead( 1, &ulValue ) == pdPASS )
            printf( "[%4lu ms] Display: CloudRead(id 1) = %lu\n", ( unsigned long ) xTaskGetTickCount(), ( unsigned long ) ulValue );
        else
            printf( "[%4lu ms] Display: CloudRead(id 1) timeout\n", ( unsigned long ) xTaskGetTickCount() );
    }
}

int main( void )
{
    xServerTaskQueue = xQueueCreate( 4, sizeof( CloudCommand_t ) );
    xTaskCreate( ServerTask, "Server", 1000, NULL, 3, NULL );
    xTaskCreate( vSensor, "Sensor", 1000, NULL, 2, NULL );
    xTaskCreate( vDisplay, "Display", 1000, NULL, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
