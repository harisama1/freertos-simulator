/* Listing 5.16-5.20 Một queue nhận nhiều loại dữ liệu: IPStackEvent_t của FreeRTOS+TCP.
 *
 * Mỗi item là { eEventType, pvData }: kiểu sự kiện quyết định cách hiểu pvData
 *   eNetworkDownEvent  pvData không dùng (NULL)
 *   eNetworkRxEvent    pvData = con trỏ tới NetworkBufferDescriptor_t (gói nhận được)
 *   eTCPAcceptEvent    pvData = handle socket (Socket_t)
 * Mã giả của sách được làm cho chạy được:
 *   - ISR ETH_RX (mỗi 700 ms) lấy một descriptor trong pool, gửi eNetworkRxEvent;
 *   - task App gọi FreeRTOS_accept() giả lập ở 1500 ms -> eTCPAcceptEvent;
 *   - task Link báo mất mạng ở 0 ms và 4000 ms -> eNetworkDownEvent;
 *   - task IP chờ queue tối đa xNextIPSleep = 1000 ms; hết giờ -> eNoEvent.
 * Quan sát: hàng xNetworkEventQueue trên Timeline; task IP block
 * "chờ nhận từ xNetworkEventQueue"; mỗi dòng in ra kiểu sự kiện và pvData.
 */
#define ETH_RX_IRQ    4

typedef enum
{
    eNoEvent = -1,
    eNetworkDownEvent = 0,
    eNetworkRxEvent,
    eTCPAcceptEvent
} eIPEvent_t;

typedef struct IP_TASK_COMMANDS
{
    eIPEvent_t eEventType;
    void * pvData;
} IPStackEvent_t;

typedef struct { uint8_t ucData[ 64 ]; size_t xDataLength; } NetworkBufferDescriptor_t;
typedef struct { uint16_t usPort; } FreeRTOS_Socket_t;
typedef FreeRTOS_Socket_t * Socket_t;

static QueueHandle_t xNetworkEventQueue;
static NetworkBufferDescriptor_t xRxPool[ 4 ];
static FreeRTOS_Socket_t xServerSocket = { 80 };
static uint32_t ulPacket;

/* ---- phía gửi (Listing 5.17-5.19) ---- */
static void xSendEventStructToIPTask( const IPStackEvent_t * pxEvent )
{
    xQueueSendToBack( xNetworkEventQueue, pxEvent, portMAX_DELAY );
}

static void ETH_RX_IRQHandler( void )      /* vSendRxDataToTheTCPTask() chạy trong ISR */
{
    BaseType_t xWoken = pdFALSE;
    NetworkBufferDescriptor_t * pxRxedData = &xRxPool[ ulPacket % 4 ];
    IPStackEvent_t xEventStruct;

    pxRxedData->xDataLength = ( size_t ) snprintf( ( char * ) pxRxedData->ucData, sizeof( pxRxedData->ucData ),
                                                   "packet #%lu", ( unsigned long ) ulPacket++ );
    xEventStruct.eEventType = eNetworkRxEvent;
    xEventStruct.pvData = ( void * ) pxRxedData;
    xQueueSendToBackFromISR( xNetworkEventQueue, &xEventStruct, &xWoken );
    portYIELD_FROM_ISR( xWoken );
}

static void vSendAcceptRequestToTheTCPTask( Socket_t xSocket )
{
    IPStackEvent_t xEventStruct;

    xEventStruct.eEventType = eTCPAcceptEvent;
    xEventStruct.pvData = ( void * ) xSocket;
    xSendEventStructToIPTask( &xEventStruct );
}

static void vSendNetworkDownEventToTheTCPTask( void )
{
    IPStackEvent_t xEventStruct;

    xEventStruct.eEventType = eNetworkDownEvent;
    xEventStruct.pvData = NULL;
    xSendEventStructToIPTask( &xEventStruct );
}

/* ---- phía nhận (Listing 5.20) ---- */
static void prvProcessNetworkDownEvent( void )
{
    printf( "[%4lu ms] IP: eNetworkDownEvent, pvData = NULL -> kết nối lại\n", ( unsigned long ) xTaskGetTickCount() );
}

static void prvHandleEthernetPacket( NetworkBufferDescriptor_t * pxBuf )
{
    printf( "[%4lu ms] IP: eNetworkRxEvent, pvData = %p -> \"%s\" (%u byte)\n", ( unsigned long ) xTaskGetTickCount(),
            ( void * ) pxBuf, ( char * ) pxBuf->ucData, ( unsigned ) pxBuf->xDataLength );
}

static void xTCPCheckNewClient( FreeRTOS_Socket_t * xSocket )
{
    printf( "[%4lu ms] IP: eTCPAcceptEvent, pvData = socket port %u\n", ( unsigned long ) xTaskGetTickCount(), xSocket->usPort );
}

static void vIPTask( void * pv )
{
    IPStackEvent_t xReceivedEvent;
    const TickType_t xNextIPSleep = pdMS_TO_TICKS( 1000 );

    for( ;; )
    {
        xReceivedEvent.eEventType = eNoEvent;
        xQueueReceive( xNetworkEventQueue, &xReceivedEvent, xNextIPSleep );

        switch( xReceivedEvent.eEventType )
        {
            case eNetworkDownEvent:
                prvProcessNetworkDownEvent();
                break;
            case eNetworkRxEvent:
                prvHandleEthernetPacket( ( NetworkBufferDescriptor_t * ) ( xReceivedEvent.pvData ) );
                break;
            case eTCPAcceptEvent:
                xTCPCheckNewClient( ( FreeRTOS_Socket_t * ) ( xReceivedEvent.pvData ) );
                break;
            default:
                printf( "[%4lu ms] IP: eNoEvent (hết %lu ms chờ) -> xử lý timer của stack\n",
                        ( unsigned long ) xTaskGetTickCount(), ( unsigned long ) xNextIPSleep );
                break;
        }
    }
}

static void vLinkTask( void * pv )
{
    vSendNetworkDownEventToTheTCPTask();
    vTaskDelay( pdMS_TO_TICKS( 4000 ) );
    vSendNetworkDownEventToTheTCPTask();
    vTaskDelete( NULL );
}

static void vAppTask( void * pv )
{
    vTaskDelay( pdMS_TO_TICKS( 1500 ) );
    vSendAcceptRequestToTheTCPTask( &xServerSocket );     /* FreeRTOS_accept() */
    vTaskDelete( NULL );
}

int main( void )
{
    xNetworkEventQueue = xQueueCreate( 5, sizeof( IPStackEvent_t ) );
    xTaskCreate( vIPTask, "IP", 1000, NULL, 3, NULL );
    xTaskCreate( vLinkTask, "Link", 1000, NULL, 2, NULL );
    xTaskCreate( vAppTask, "App", 1000, NULL, 1, NULL );
    sim_irq_attach( ETH_RX_IRQ, "ETH_RX", ETH_RX_IRQHandler );
    sim_irq_every( ETH_RX_IRQ, 700, 700 );
    vTaskStartScheduler();
    for( ;; );
}
