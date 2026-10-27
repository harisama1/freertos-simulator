/*
 * sim_prelude.h - force-included (gcc -include) before the user's file,
 * so code copied from the book compiles without its #include lines.
 */
#ifndef SIM_PRELUDE_H
#define SIM_PRELUDE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"
#include "sim_hw.h"

/* Print helpers used by the book's examples (old and new names). */
void vPrintString( const char * pcString );
void vPrintLine( const char * pcString );
void vPrintStringAndNumber( const char * pcString, uint32_t ulValue );
void vPrintLineAndNumber( const char * pcString, uint32_t ulValue );
void vPrintTwoStrings( const char * pcString1, const char * pcString2 );

/*
 * stdio takes pthread mutexes; the kernel may switch tasks while one is
 * held, which deadlocks the POSIX port. Route printing through a
 * critical-section + write() implementation instead.
 */
int sim_printf( const char * fmt, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
int sim_puts( const char * s );
int sim_putchar( int c );
void vSimLine( int iLine );     /* debug mode: the next kernel event comes from this source line */
#define printf( ... )    ( vSimLine( __LINE__ ), sim_printf( __VA_ARGS__ ) )
#define puts( s )        ( vSimLine( __LINE__ ), sim_puts( s ) )
#define putchar( c )     ( vSimLine( __LINE__ ), sim_putchar( c ) )

/*
 * ISRs: the POSIX port's portYIELD_FROM_ISR() would switch threads inside
 * the tick signal handler. Record the request instead; sim_hw.c performs
 * the switch when the simulated interrupt returns.
 */
#undef portYIELD_FROM_ISR
#undef portEND_SWITCHING_ISR
#define portYIELD_FROM_ISR( x )       vSimYieldFromISR( ( x ) != pdFALSE )
#define portEND_SWITCHING_ISR( x )    portYIELD_FROM_ISR( x )

/*
 * Tag what the program creates, for the timeline: the task entry function
 * (for the static stack estimate) and the source line of each queue,
 * semaphore, mutex, timer and event group (frsim.py names the object after
 * the variable assigned on that line). A macro is not expanded again
 * inside its own replacement, so the real function is still called.
 */
void vSimTaskFn( const char * pcFn, unsigned long ulDepth );
long lSimTaskCreated( long lResult );
void * pvSimTaskCreated( void * pvHandle );
void * pvSimTagQueue( void * pvQueue, int iLine );
void * pvSimTagTimer( void * pvTimer, int iLine );
void * pvSimTagEventGroup( void * pvGroup, int iLine );

#define xTaskCreate( pxTaskCode, pcName, uxStackDepth, ... )                     \
    ( ( BaseType_t ) ( vSimLine( __LINE__ ), vSimTaskFn( #pxTaskCode, ( unsigned long ) ( uxStackDepth ) ), \
                       lSimTaskCreated( xTaskCreate( pxTaskCode, pcName, uxStackDepth, __VA_ARGS__ ) ) ) )
#define xTaskCreateStatic( pxTaskCode, pcName, uxStackDepth, ... )               \
    ( ( TaskHandle_t ) ( vSimLine( __LINE__ ), vSimTaskFn( #pxTaskCode, ( unsigned long ) ( uxStackDepth ) ), \
                         pvSimTaskCreated( xTaskCreateStatic( pxTaskCode, pcName, uxStackDepth, __VA_ARGS__ ) ) ) )

#define xQueueGenericCreate( ... ) \
    ( ( QueueHandle_t ) pvSimTagQueue( xQueueGenericCreate( __VA_ARGS__ ), __LINE__ ) )
#define xQueueGenericCreateStatic( ... ) \
    ( ( QueueHandle_t ) pvSimTagQueue( xQueueGenericCreateStatic( __VA_ARGS__ ), __LINE__ ) )
#define xQueueCreateMutex( ... ) \
    ( ( QueueHandle_t ) pvSimTagQueue( xQueueCreateMutex( __VA_ARGS__ ), __LINE__ ) )
#define xQueueCreateMutexStatic( ... ) \
    ( ( QueueHandle_t ) pvSimTagQueue( xQueueCreateMutexStatic( __VA_ARGS__ ), __LINE__ ) )
#define xQueueCreateCountingSemaphore( ... ) \
    ( ( QueueHandle_t ) pvSimTagQueue( xQueueCreateCountingSemaphore( __VA_ARGS__ ), __LINE__ ) )
#define xQueueCreateCountingSemaphoreStatic( ... ) \
    ( ( QueueHandle_t ) pvSimTagQueue( xQueueCreateCountingSemaphoreStatic( __VA_ARGS__ ), __LINE__ ) )
#define xQueueCreateSet( ... ) \
    ( ( QueueSetHandle_t ) pvSimTagQueue( xQueueCreateSet( __VA_ARGS__ ), __LINE__ ) )
#define xTimerCreate( ... ) \
    ( ( TimerHandle_t ) pvSimTagTimer( xTimerCreate( __VA_ARGS__ ), __LINE__ ) )
#define xTimerCreateStatic( ... ) \
    ( ( TimerHandle_t ) pvSimTagTimer( xTimerCreateStatic( __VA_ARGS__ ), __LINE__ ) )
#define xEventGroupCreate() \
    ( ( EventGroupHandle_t ) pvSimTagEventGroup( xEventGroupCreate(), __LINE__ ) )
#define xEventGroupCreateStatic( ... ) \
    ( ( EventGroupHandle_t ) pvSimTagEventGroup( xEventGroupCreateStatic( __VA_ARGS__ ), __LINE__ ) )

/*
 * Debug mode: record the source line of every kernel/peripheral call, so a
 * kernel event (task switch, queue send, ...) is shown on the line that
 * caused it. A basic block can span several lines; these calls cannot.
 */
#define simLINE_CALL( fn, ... )    ( vSimLine( __LINE__ ), fn( __VA_ARGS__ ) )
#define vTaskDelay( ... )                     simLINE_CALL( vTaskDelay, __VA_ARGS__ )
#define xTaskDelayUntil( ... )                simLINE_CALL( xTaskDelayUntil, __VA_ARGS__ )
#define vTaskPrioritySet( ... )               simLINE_CALL( vTaskPrioritySet, __VA_ARGS__ )
#define vTaskDelete( ... )                    simLINE_CALL( vTaskDelete, __VA_ARGS__ )
#define vTaskSuspend( ... )                   simLINE_CALL( vTaskSuspend, __VA_ARGS__ )
#define vTaskResume( ... )                    simLINE_CALL( vTaskResume, __VA_ARGS__ )
#define xTaskResumeFromISR( ... )             simLINE_CALL( xTaskResumeFromISR, __VA_ARGS__ )
#define xQueueGenericSend( ... )              simLINE_CALL( xQueueGenericSend, __VA_ARGS__ )
#define xQueueGenericSendFromISR( ... )       simLINE_CALL( xQueueGenericSendFromISR, __VA_ARGS__ )
#define xQueueGiveFromISR( ... )              simLINE_CALL( xQueueGiveFromISR, __VA_ARGS__ )
#define xQueueReceive( ... )                  simLINE_CALL( xQueueReceive, __VA_ARGS__ )
#define xQueuePeek( ... )                     simLINE_CALL( xQueuePeek, __VA_ARGS__ )
#define xQueueSemaphoreTake( ... )            simLINE_CALL( xQueueSemaphoreTake, __VA_ARGS__ )
#define xQueueReceiveFromISR( ... )           simLINE_CALL( xQueueReceiveFromISR, __VA_ARGS__ )
#define xQueueTakeMutexRecursive( ... )       simLINE_CALL( xQueueTakeMutexRecursive, __VA_ARGS__ )
#define xQueueGiveMutexRecursive( ... )       simLINE_CALL( xQueueGiveMutexRecursive, __VA_ARGS__ )
#define xQueueSelectFromSet( ... )            simLINE_CALL( xQueueSelectFromSet, __VA_ARGS__ )
#define xTaskGenericNotify( ... )             simLINE_CALL( xTaskGenericNotify, __VA_ARGS__ )
#define xTaskGenericNotifyFromISR( ... )      simLINE_CALL( xTaskGenericNotifyFromISR, __VA_ARGS__ )
#define vTaskGenericNotifyGiveFromISR( ... )  simLINE_CALL( vTaskGenericNotifyGiveFromISR, __VA_ARGS__ )
#define ulTaskGenericNotifyTake( ... )        simLINE_CALL( ulTaskGenericNotifyTake, __VA_ARGS__ )
#define xTaskGenericNotifyWait( ... )         simLINE_CALL( xTaskGenericNotifyWait, __VA_ARGS__ )
#define xEventGroupSetBits( ... )             simLINE_CALL( xEventGroupSetBits, __VA_ARGS__ )
#define xEventGroupClearBits( ... )           simLINE_CALL( xEventGroupClearBits, __VA_ARGS__ )
#define xEventGroupWaitBits( ... )            simLINE_CALL( xEventGroupWaitBits, __VA_ARGS__ )
#define xEventGroupSync( ... )                simLINE_CALL( xEventGroupSync, __VA_ARGS__ )
#define xEventGroupSetBitsFromISR( ... )      simLINE_CALL( xEventGroupSetBitsFromISR, __VA_ARGS__ )
#define xTimerGenericCommandFromTask( ... )   simLINE_CALL( xTimerGenericCommandFromTask, __VA_ARGS__ )
#define xTimerGenericCommandFromISR( ... )    simLINE_CALL( xTimerGenericCommandFromISR, __VA_ARGS__ )
#define xTimerPendFunctionCallFromISR( ... )  simLINE_CALL( xTimerPendFunctionCallFromISR, __VA_ARGS__ )
#define xTimerPendFunctionCall( ... )         simLINE_CALL( xTimerPendFunctionCall, __VA_ARGS__ )
#define pvPortMalloc( ... )                   simLINE_CALL( pvPortMalloc, __VA_ARGS__ )
#define vPortFree( ... )                      simLINE_CALL( vPortFree, __VA_ARGS__ )
#define vPrintString( ... )                   simLINE_CALL( vPrintString, __VA_ARGS__ )
#define vPrintLine( ... )                     simLINE_CALL( vPrintLine, __VA_ARGS__ )
#define vPrintStringAndNumber( ... )          simLINE_CALL( vPrintStringAndNumber, __VA_ARGS__ )
#define vPrintLineAndNumber( ... )            simLINE_CALL( vPrintLineAndNumber, __VA_ARGS__ )
#define vPrintTwoStrings( ... )               simLINE_CALL( vPrintTwoStrings, __VA_ARGS__ )
#define vPortGenerateSimulatedInterrupt( ... ) simLINE_CALL( vPortGenerateSimulatedInterrupt, __VA_ARGS__ )
#define sim_irq_raise( ... )                  simLINE_CALL( sim_irq_raise, __VA_ARGS__ )
#define sim_gpio_write( ... )                 simLINE_CALL( sim_gpio_write, __VA_ARGS__ )
#define sim_gpio_toggle( ... )                simLINE_CALL( sim_gpio_toggle, __VA_ARGS__ )
#undef taskYIELD
#define taskYIELD()                           ( vSimLine( __LINE__ ), portYIELD() )

#endif /* SIM_PRELUDE_H */
