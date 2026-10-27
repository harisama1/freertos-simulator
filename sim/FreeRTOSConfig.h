/*
 * FreeRTOSConfig.h for the Linux (POSIX port) simulator.
 * Every option the IDE exposes is guarded by #ifndef so frsim.py can
 * override it with -D on the gcc command line.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Scheduling (chapter 4) ---- */
#ifndef configUSE_PREEMPTION
#define configUSE_PREEMPTION                    1
#endif
#ifndef configUSE_TIME_SLICING
#define configUSE_TIME_SLICING                  1
#endif
#ifndef configIDLE_SHOULD_YIELD
#define configIDLE_SHOULD_YIELD                 1
#endif
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ                      1000
#endif
#ifndef configMAX_PRIORITIES
#define configMAX_PRIORITIES                    7
#endif
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#if __SIZEOF_LONG__ == 4                            /* -m32 build: 32-bit tick as on an MCU */
    #define configTICK_TYPE_WIDTH_IN_BITS       TICK_TYPE_WIDTH_32_BITS
#else
    #define configTICK_TYPE_WIDTH_IN_BITS       TICK_TYPE_WIDTH_64_BITS
#endif
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configMAX_TASK_NAME_LEN                 16

/* ---- Memory (chapter 3) ---- */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#ifndef configSUPPORT_STATIC_ALLOCATION
#define configSUPPORT_STATIC_ALLOCATION         0   /* 1: xTaskCreateStatic(); Idle/Timer TCB+stack not from heap */
#endif
#define configKERNEL_PROVIDED_STATIC_MEMORY     0   /* weak defaults in sim_support.c */
#ifndef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 64 * 1024 ) )
#endif
#ifndef configAPPLICATION_ALLOCATED_HEAP
#define configAPPLICATION_ALLOCATED_HEAP        0
#endif
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          0   /* tasks run on pthread stacks; frsim.py estimates stack use statically */

/* ---- Hooks: weak empty defaults in sim_support.c, user may override ---- */
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     1

/* ---- Other kernel objects (later chapters) ---- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    1
#define configUSE_TASK_NOTIFICATIONS            1
#define configQUEUE_REGISTRY_SIZE               10
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )   /* 256 words, STM32CubeMX default */
#define configUSE_CO_ROUTINES                   0

/* ---- Debug / statistics ---- */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
#define configGENERATE_RUN_TIME_STATS           1
/* Run-time counter = simulator microseconds (the POSIX port's default, times(), has 10 ms resolution). */
#define portALT_GET_RUN_TIME_COUNTER_VALUE( x )    ( ( x ) = ( configRUN_TIME_COUNTER_TYPE ) ulSimRunTimeUs() )
#define configRECORD_STACK_HIGH_ADDRESS         1

/* ---- API functions ---- */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xSemaphoreGetMutexHolder        1

#ifndef __ASSEMBLER__
void vSimAssert( const char * pcFile, unsigned long ulLine );
unsigned long ulSimRunTimeUs( void );
void vSimTraceCreate( unsigned long ulStackWords, unsigned long ulNum, unsigned long ulPrio, const char * pcName );
void vSimTraceIn( unsigned long ulNum );
void vSimTraceOut( unsigned long ulNum, int iStillReady );
void vSimTraceReady( unsigned long ulNum );
void vSimTraceDelete( unsigned long ulNum );
void vSimTracePrio( unsigned long ulNum, unsigned long ulPrio );
void vSimTraceTick( unsigned long long ullTick );
void vSimTraceMalloc( void * pv, unsigned long ulSize, long lFree );
void vSimTraceFree( void * pv, unsigned long ulSize, long lFree );
unsigned long ulSimTraceObj( int iType, unsigned long ulLen, unsigned long ulLevel );
void vSimTraceObjName( unsigned long ulObj, const char * pcName );
void vSimTraceQueue( char cOp, unsigned long ulObj, long lLevel, int iFromIsr );
void vSimTraceBlock( char cWhy, unsigned long ulObj, unsigned long ulArg );
void vSimTraceSuspend( unsigned long ulNum );
void vSimTraceNotify( unsigned long ulNum, int iFromIsr );
void vSimTraceInherit( char cOp, unsigned long ulNum, unsigned long ulPrio );
unsigned long ulSimTraceTimer( unsigned long ulPeriod, int iAutoReload, const char * pcName );
void vSimTraceTimerExpired( unsigned long ulObj );
void vSimTraceEventBits( char cOp, unsigned long ulObj, unsigned long ulBits, int iFromIsr );
#endif

#define configASSERT( x )    do { if( ( x ) == 0 ) vSimAssert( __FILE__, __LINE__ ); } while( 0 )


/*
 * Trace hooks for the IDE timeline. They expand inside tasks.c, where
 * pxCurrentTCB and pxReadyTasksLists are visible. Each call only does
 * clock_gettime() + write(), both async-signal-safe (the tick runs in a
 * SIGALRM handler on this port).
 */
#define traceTASK_CREATE( pxNewTCB ) \
    vSimTraceCreate( ( unsigned long ) ( ( pxNewTCB )->pxEndOfStack - ( pxNewTCB )->pxStack + 1 ), \
                     ( pxNewTCB )->uxTCBNumber, ( pxNewTCB )->uxPriority, ( pxNewTCB )->pcTaskName )
#define traceTASK_SWITCHED_IN() \
    vSimTraceIn( pxCurrentTCB->uxTCBNumber )
#define traceTASK_SWITCHED_OUT()                                                              \
    vSimTraceOut( pxCurrentTCB->uxTCBNumber,                                                  \
                  listLIST_ITEM_CONTAINER( &( pxCurrentTCB->xStateListItem ) ) ==             \
                  &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) )
#define traceMOVED_TASK_TO_READY_STATE( pxTCB ) \
    vSimTraceReady( ( pxTCB )->uxTCBNumber )
#define traceTASK_DELETE( pxTCB ) \
    vSimTraceDelete( ( pxTCB )->uxTCBNumber )
#define traceTASK_PRIORITY_SET( pxTCB, uxNewPriority ) \
    vSimTracePrio( ( pxTCB )->uxTCBNumber, ( uxNewPriority ) )
#define traceTASK_INCREMENT_TICK( xTickCount ) \
    vSimTraceTick( ( unsigned long long ) ( xTickCount ) + 1ULL )

/* Heap trace (chapter 3). heap_3 wraps malloc() and has no
 * xPortGetFreeHeapSize(), so frsim.py passes SIM_HEAP_FREE_KNOWN=0. */
#if defined( SIM_HEAP_FREE_KNOWN ) && ( SIM_HEAP_FREE_KNOWN == 1 )
    #define simHEAP_FREE()    ( ( long ) xPortGetFreeHeapSize() )
#else
    #define simHEAP_FREE()    ( -1L )
#endif
#define traceMALLOC( pvAddress, uiSize ) \
    vSimTraceMalloc( ( pvAddress ), ( unsigned long ) ( uiSize ), simHEAP_FREE() )
#define traceFREE( pvAddress, uiSize ) \
    vSimTraceFree( ( pvAddress ), ( unsigned long ) ( uiSize ), simHEAP_FREE() )

/*
 * Kernel objects (chapters 5-10). Each user object gets an id in its
 * uxQueueNumber / uxTimerNumber / uxEventGroupNumber field. Levels are
 * computed here because the macros run before the kernel updates
 * uxMessagesWaiting.
 *   object types: 0 queue, 1 mutex, 2 counting sem, 3 binary sem,
 *                 4 recursive mutex, 9 event group
 */
#define traceQUEUE_CREATE( pxNewQueue ) \
    ( pxNewQueue )->uxQueueNumber = ulSimTraceObj( ( pxNewQueue )->ucQueueType, ( pxNewQueue )->uxLength, 0 )
#define traceCREATE_COUNTING_SEMAPHORE() \
    vSimTraceQueue( 'c', ( ( Queue_t * ) xHandle )->uxQueueNumber, ( long ) uxInitialCount, 0 )
#define traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName ) \
    vSimTraceObjName( ( xQueue )->uxQueueNumber, ( pcQueueName ) )
#define simQ_AFTER_SEND( q ) \
    ( long ) ( ( q )->uxMessagesWaiting < ( q )->uxLength ? ( q )->uxMessagesWaiting + 1 : ( q )->uxLength )
#define traceQUEUE_SEND( q )                     vSimTraceQueue( 'S', ( q )->uxQueueNumber, simQ_AFTER_SEND( q ), 0 )
#define traceQUEUE_SEND_FROM_ISR( q )            vSimTraceQueue( 'S', ( q )->uxQueueNumber, simQ_AFTER_SEND( q ), 1 )
/* Queue set notified by a member's send: the member's own send was already checked for ISR misuse. */
#define traceQUEUE_SET_SEND( q )                 vSimTraceQueue( 'S', ( q )->uxQueueNumber, simQ_AFTER_SEND( q ), 1 )
#define traceQUEUE_SEND_FAILED( q )              vSimTraceQueue( 's', ( q )->uxQueueNumber, ( long ) ( q )->uxMessagesWaiting, 0 )
#define traceQUEUE_SEND_FROM_ISR_FAILED( q )     vSimTraceQueue( 's', ( q )->uxQueueNumber, ( long ) ( q )->uxMessagesWaiting, 1 )
#define traceQUEUE_RECEIVE( q )                  vSimTraceQueue( 'G', ( q )->uxQueueNumber, ( long ) ( q )->uxMessagesWaiting - 1, 0 )
#define traceQUEUE_RECEIVE_FROM_ISR( q )         vSimTraceQueue( 'G', ( q )->uxQueueNumber, ( long ) ( q )->uxMessagesWaiting - 1, 1 )
#define traceQUEUE_RECEIVE_FAILED( q )           vSimTraceQueue( 'g', ( q )->uxQueueNumber, ( long ) ( q )->uxMessagesWaiting, 0 )
#define traceQUEUE_RECEIVE_FROM_ISR_FAILED( q )  vSimTraceQueue( 'g', ( q )->uxQueueNumber, ( long ) ( q )->uxMessagesWaiting, 1 )
#define traceBLOCKING_ON_QUEUE_RECEIVE( q )      vSimTraceBlock( 'r', ( q )->uxQueueNumber, 0 )
#define traceBLOCKING_ON_QUEUE_PEEK( q )         vSimTraceBlock( 'r', ( q )->uxQueueNumber, 0 )
#define traceBLOCKING_ON_QUEUE_SEND( q )         vSimTraceBlock( 's', ( q )->uxQueueNumber, 0 )

#define traceTASK_DELAY()                        vSimTraceBlock( 'd', 0, ( unsigned long ) xTicksToDelay )
#define traceTASK_DELAY_UNTIL( xTimeToWake )     vSimTraceBlock( 'u', 0, ( unsigned long ) ( xTimeToWake ) )
#define traceTASK_SUSPEND( pxTCB )               vSimTraceSuspend( ( pxTCB )->uxTCBNumber )
#define traceTASK_NOTIFY( uxIndex )              vSimTraceNotify( pxTCB->uxTCBNumber, 0 )
#define traceTASK_NOTIFY_FROM_ISR( uxIndex )     vSimTraceNotify( pxTCB->uxTCBNumber, 1 )
#define traceTASK_NOTIFY_GIVE_FROM_ISR( uxIndex ) vSimTraceNotify( pxTCB->uxTCBNumber, 1 )
#define traceTASK_NOTIFY_TAKE_BLOCK( uxIndex )   vSimTraceBlock( 'n', 0, 0 )
#define traceTASK_NOTIFY_WAIT_BLOCK( uxIndex )   vSimTraceBlock( 'n', 0, 0 )
#define traceTASK_PRIORITY_INHERIT( pxTCB, uxPrio )     vSimTraceInherit( 'H', ( pxTCB )->uxTCBNumber, ( uxPrio ) )
#define traceTASK_PRIORITY_DISINHERIT( pxTCB, uxPrio )  vSimTraceInherit( 'h', ( pxTCB )->uxTCBNumber, ( uxPrio ) )

#define traceTIMER_CREATE( pxNewTimer )                                                        \
    ( pxNewTimer )->uxTimerNumber = ulSimTraceTimer( ( unsigned long ) ( pxNewTimer )->xTimerPeriodInTicks, \
                                                     ( ( pxNewTimer )->ucStatus & 0x04U ) != 0U, ( pxNewTimer )->pcTimerName )
#define traceTIMER_EXPIRED( pxTimer )            vSimTraceTimerExpired( ( pxTimer )->uxTimerNumber )

#define traceEVENT_GROUP_CREATE( pxEventBits ) \
    ( pxEventBits )->uxEventGroupNumber = ulSimTraceObj( 9, 0, 0 )
#define traceEVENT_GROUP_SET_BITS( xEG, uxBits )            vSimTraceEventBits( 'e', ( xEG )->uxEventGroupNumber, ( uxBits ), 0 )
#define traceEVENT_GROUP_SET_BITS_FROM_ISR( xEG, uxBits )   vSimTraceEventBits( 'e', ( xEG )->uxEventGroupNumber, ( uxBits ), 1 )
#define traceEVENT_GROUP_CLEAR_BITS( xEG, uxBits )          vSimTraceEventBits( 'x', ( xEG )->uxEventGroupNumber, ( uxBits ), 0 )
#define traceEVENT_GROUP_CLEAR_BITS_FROM_ISR( xEG, uxBits ) vSimTraceEventBits( 'x', ( xEG )->uxEventGroupNumber, ( uxBits ), 1 )
#define traceEVENT_GROUP_WAIT_BITS_BLOCK( xEG, uxBits )     vSimTraceBlock( 'e', ( xEG )->uxEventGroupNumber, ( uxBits ) )
#define traceEVENT_GROUP_SYNC_BLOCK( xEG, uxSet, uxWait )   vSimTraceBlock( 'e', ( xEG )->uxEventGroupNumber, ( uxWait ) )

#endif /* FREERTOS_CONFIG_H */
