/*
 * sim_support.c - runtime support for the FreeRTOS POSIX simulator:
 * safe printing, weak default hooks, assert handler, timeline trace.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"
#include "sim_hw.h"

static int iTraceFd = -1;
static unsigned long ulCurTask;   /* TCB number of the running task, 0 before the scheduler */
static unsigned long ulNextObj;   /* ids of queues, semaphores, mutexes, timers, event groups */
static const char * pcNextTaskFn; /* entry function and stack depth of the task being created */
static unsigned long ulNextTaskDepth;
static int iTimestamp;
static int iDebug;                /* SIM_DEBUG=1: record executed code blocks and output offsets */
static unsigned long long ullOutBytes;
static struct timespec xT0;

static unsigned long long prvNowUs( void )
{
    struct timespec t;
    long long ns;

    clock_gettime( CLOCK_MONOTONIC, &t );
    ns = ( long long ) ( t.tv_sec - xT0.tv_sec ) * 1000000000LL +
         ( long long ) ( t.tv_nsec - xT0.tv_nsec );
    return ( ns > 0 ) ? ( unsigned long long ) ( ns / 1000LL ) : 0ULL;
}

__attribute__( ( constructor ) ) static void prvSimInit( void )
{
    const char * p;

    clock_gettime( CLOCK_MONOTONIC, &xT0 );
    p = getenv( "SIM_TRACE" );
    if( p != NULL )
        iTraceFd = open( p, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
    p = getenv( "SIM_TIMESTAMP" );
    iTimestamp = ( p != NULL ) && ( p[ 0 ] == '1' );
    p = getenv( "SIM_DEBUG" );
    iDebug = ( p != NULL ) && ( p[ 0 ] == '1' );
}

/* ------------------------------------------------------------------ */
/* Output                                                              */

static void prvWriteAll( int fd, const char * buf, size_t len )
{
    while( len > 0 )
    {
        ssize_t n = write( fd, buf, len );

        if( n <= 0 )
            return;
        buf += n;
        len -= ( size_t ) n;
    }
}

/* Format into a local buffer and write() it inside a critical section:
 * no stdio lock is ever held across a task switch. */
static int prvVPrint( const char * fmt, va_list args )
{
    char buf[ 1024 ];
    int len = 0;
    int n;
    static int iAtLineStart = 1;

    taskENTER_CRITICAL();

    if( iTimestamp && iAtLineStart )
    {
        unsigned long long us = prvNowUs();

        len = snprintf( buf, sizeof( buf ), "[%6llu.%03llu ms] ",
                        us / 1000ULL, us % 1000ULL );
    }

    n = vsnprintf( buf + len, sizeof( buf ) - ( size_t ) len, fmt, args );
    if( n > ( int ) ( sizeof( buf ) - ( size_t ) len - 1 ) )
        n = ( int ) ( sizeof( buf ) - ( size_t ) len - 1 );
    if( n > 0 )
    {
        len += n;
        iAtLineStart = ( buf[ len - 1 ] == '\n' );
    }
    prvWriteAll( STDOUT_FILENO, buf, ( size_t ) len );
    ullOutBytes += ( unsigned long long ) len;
    if( iDebug )
    {
        const long long v[] = { ( long long ) ullOutBytes };

        vSimEvent( 'o', 1, v, NULL );
    }

    taskEXIT_CRITICAL();
    return n;
}

int sim_printf( const char * fmt, ... )
{
    va_list args;
    int n;

    va_start( args, fmt );
    n = prvVPrint( fmt, args );
    va_end( args );
    return n;
}

int sim_puts( const char * s )
{
    return sim_printf( "%s\n", s );
}

int sim_putchar( int c )
{
    sim_printf( "%c", c );
    return c;
}

void vPrintString( const char * pcString )
{
    sim_printf( "%s", pcString );
}

void vPrintLine( const char * pcString )
{
    sim_printf( "%s\n", pcString );
}

void vPrintStringAndNumber( const char * pcString, uint32_t ulValue )
{
    sim_printf( "%s %lu\n", pcString, ( unsigned long ) ulValue );
}

void vPrintLineAndNumber( const char * pcString, uint32_t ulValue )
{
    sim_printf( "%s %lu\n", pcString, ( unsigned long ) ulValue );
}

void vPrintTwoStrings( const char * pcString1, const char * pcString2 )
{
    sim_printf( "%s %s\n", pcString1, pcString2 );
}

/* ------------------------------------------------------------------ */
/* Assert and default hooks (weak: the user's file may define them)    */

void vSimAssert( const char * pcFile, unsigned long ulLine )
{
    char buf[ 256 ];
    int n = snprintf( buf, sizeof( buf ), "\n[sim] configASSERT failed: %s:%lu\n",
                      pcFile, ulLine );

    prvWriteAll( STDERR_FILENO, buf, ( size_t ) n );
    _exit( 3 );
}

unsigned long ulSimRunTimeUs( void )
{
    return ( unsigned long ) prvNowUs();
}

__attribute__( ( weak ) ) void vApplicationIdleHook( void )
{
}

__attribute__( ( weak ) ) void vApplicationTickHook( void )
{
}

__attribute__( ( weak ) ) void vApplicationMallocFailedHook( void )
{
    static const char msg[] = "[sim] vApplicationMallocFailedHook: pvPortMalloc() returned NULL (heap exhausted)\n";

    prvWriteAll( STDERR_FILENO, msg, sizeof( msg ) - 1 );
}

__attribute__( ( weak ) ) void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxTCB,
                                                              StackType_t ** ppxStack,
                                                              configSTACK_DEPTH_TYPE * puxStackSize )
{
    static StaticTask_t xTCB;
    static StackType_t uxStack[ configMINIMAL_STACK_SIZE ];

    *ppxTCB = &xTCB;
    *ppxStack = uxStack;
    *puxStackSize = configMINIMAL_STACK_SIZE;
}

__attribute__( ( weak ) ) void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTCB,
                                                               StackType_t ** ppxStack,
                                                               configSTACK_DEPTH_TYPE * puxStackSize )
{
    static StaticTask_t xTCB;
    static StackType_t uxStack[ configTIMER_TASK_STACK_DEPTH ];

    *ppxTCB = &xTCB;
    *ppxStack = uxStack;
    *puxStackSize = configTIMER_TASK_STACK_DEPTH;
}

/* ------------------------------------------------------------------ */
/* Timeline trace: one text line per event, "<us> <code> <args>"        */

static char * prvU( char * p, unsigned long long v )
{
    char tmp[ 24 ];
    int i = 0;

    do
    {
        tmp[ i++ ] = ( char ) ( '0' + ( v % 10 ) );
        v /= 10;
    } while( v != 0 );

    while( i > 0 )
        *p++ = tmp[ --i ];
    return p;
}

/* nvals numbers, then an optional name. A negative value is written as
 * "-" (unknown). */
static void prvDebugFlush( void );

void vSimEvent( char code, int nvals, const long long * vals, const char * name )
{
    char buf[ 128 ];
    char * p = buf;
    int i;

    if( iTraceFd < 0 )
        return;
    if( iDebug && ( code != 'E' ) && ( code != 'W' ) && ( code != 'T' ) )
        prvDebugFlush();

    p = prvU( p, prvNowUs() );
    *p++ = ' ';
    *p++ = code;
    for( i = 0; i < nvals; i++ )
    {
        *p++ = ' ';
        if( vals[ i ] < 0 )
            *p++ = '-';
        else
            p = prvU( p, ( unsigned long long ) vals[ i ] );
    }
    if( name != NULL )
    {
        *p++ = ' ';
        while( *name != '\0' && p < buf + sizeof( buf ) - 2 )
        {
            *p++ = ( *name == '\n' ) ? ' ' : *name;
            name++;
        }
    }
    *p++ = '\n';
    prvWriteAll( iTraceFd, buf, ( size_t ) ( p - buf ) );
}

#define EV( code, name, ... )                                              \
    do {                                                                   \
        const long long v[] = { __VA_ARGS__ };                             \
        vSimEvent( code, ( int ) ( sizeof( v ) / sizeof( v[ 0 ] ) ), v, name ); \
    } while( 0 )

/* ulStackWords is derived from the TCB, which loses up to one word to the
 * 8-byte alignment of the top of stack; the depth passed to xTaskCreate()
 * is used when the wrapper in sim_prelude.h recorded it. */
void vSimTraceCreate( unsigned long ulStackWords, unsigned long ulNum, unsigned long ulPrio, const char * pcName )
{
    unsigned long ulDepth = ulNextTaskDepth ? ulNextTaskDepth
                            : ( ulStackWords + ( 8 / sizeof( StackType_t ) ) - 1 ) / ( 8 / sizeof( StackType_t ) ) * ( 8 / sizeof( StackType_t ) );

    EV( 'C', pcName, ( long long ) ulNum, ( long long ) ulPrio );
    EV( 'k', pcNextTaskFn, ( long long ) ulNum, ( long long ) ulDepth, ( long long ) sizeof( StackType_t ) );
    pcNextTaskFn = NULL;
    ulNextTaskDepth = 0;
}

void vSimTraceIn( unsigned long ulNum )
{
    ulCurTask = ulNum;
    EV( 'I', NULL, ( long long ) ulNum );
}

void vSimTraceOut( unsigned long ulNum, int iStillReady )
{
    EV( 'O', NULL, ( long long ) ulNum, iStillReady ? 1 : 0 );
}

void vSimTraceReady( unsigned long ulNum )
{
    EV( 'R', NULL, ( long long ) ulNum );
}

void vSimTraceDelete( unsigned long ulNum )
{
    EV( 'D', NULL, ( long long ) ulNum );
}

void vSimTracePrio( unsigned long ulNum, unsigned long ulPrio )
{
    EV( 'P', NULL, ( long long ) ulNum, ( long long ) ulPrio );
}

/* Called at the start of xTaskIncrementTick(), inside the tick interrupt.
 * Ticks replayed by xTaskResumeAll() repeat a value and are skipped. */
void vSimTraceTick( unsigned long long ullTick )
{
    static unsigned long long ullLast;

    if( ullTick <= ullLast )
        return;
    ullLast = ullTick;
    EV( 'T', NULL, ( long long ) ullTick );
    vSimHwTick( ( TickType_t ) ullTick );
}

void vSimTraceMalloc( void * pv, unsigned long ulSize, long lFree )
{
    EV( 'M', NULL, ( long long ) ( uintptr_t ) pv, ( long long ) ulSize, ( long long ) lFree, ( long long ) ulCurTask );
}

void vSimTraceFree( void * pv, unsigned long ulSize, long lFree )
{
    EV( 'F', NULL, ( long long ) ( uintptr_t ) pv, ( long long ) ulSize, ( long long ) lFree, ( long long ) ulCurTask );
}

/* ------------------------------------------------------------------ */
/* Kernel objects (chapters 5-10)                                      */

/* Who performed an operation: task number, or 100000 + irq inside an ISR. */
static long long prvCtx( void )
{
    return ( iSimIrq >= 0 ) ? 100000LL + iSimIrq : ( long long ) ulCurTask;
}

/* A task-level API used inside an ISR: allowed by the compiler, wrong on
 * real hardware. Reported once per API. */
static void prvMisuse( char cOp, const char * pcApi )
{
    static char seen[ 128 ];

    EV( 'Z', pcApi, ( long long ) iSimIrq );
    if( !seen[ ( unsigned char ) cOp & 127 ] )
    {
        char buf[ 200 ];
        int n = snprintf( buf, sizeof( buf ), "[sim] WARNING: %s called inside an ISR (IRQ %d); use the ...FromISR() version\n",
                          pcApi, iSimIrq );

        seen[ ( unsigned char ) cOp & 127 ] = 1;
        prvWriteAll( STDERR_FILENO, buf, ( size_t ) n );
    }
}

unsigned long ulSimTraceObj( int iType, unsigned long ulLen, unsigned long ulLevel )
{
    unsigned long ulId = ++ulNextObj;

    EV( 'Q', NULL, ( long long ) ulId, iType, ( long long ) ulLen, ( long long ) ulLevel );
    return ulId;
}

void vSimTraceObjName( unsigned long ulObj, const char * pcName )
{
    EV( 'N', pcName, ( long long ) ulObj, 0 );
}

void vSimTraceQueue( char cOp, unsigned long ulObj, long lLevel, int iFromIsr )
{
    if( !iFromIsr && ( iSimIrq >= 0 ) && ( cOp != 'c' ) )
        prvMisuse( cOp, ( cOp == 'S' || cOp == 's' ) ? "xQueueSend()/xSemaphoreGive()" : "xQueueReceive()/xSemaphoreTake()" );
    EV( cOp, NULL, ( long long ) ulObj, ( long long ) lLevel, prvCtx() );
}

/* The running task is about to block: why (r/s queue, d/u delay,
 * n notification, e event group), on which object, extra argument. */
void vSimTraceBlock( char cWhy, unsigned long ulObj, unsigned long ulArg )
{
    char why[ 2 ] = { cWhy, 0 };

    if( iSimIrq >= 0 )
    {
        char buf[ 200 ];
        int n = snprintf( buf, sizeof( buf ), "\n[sim] ERROR: a blocking API was called inside an ISR (IRQ %d); "
                          "an ISR must never block (use ...FromISR(), block time 0)\n", iSimIrq );

        prvWriteAll( STDERR_FILENO, buf, ( size_t ) n );
        _exit( 4 );
    }
    EV( 'B', why, ( long long ) ulCurTask, ( long long ) ulObj, ( long long ) ulArg );
}

void vSimTraceSuspend( unsigned long ulNum )
{
    EV( 'U', NULL, ( long long ) ulNum, prvCtx() );
}

void vSimTraceNotify( unsigned long ulNum, int iFromIsr )
{
    if( !iFromIsr && ( iSimIrq >= 0 ) )
        prvMisuse( 'A', "xTaskNotify()/xTaskNotifyGive()" );
    EV( 'A', NULL, ( long long ) ulNum, prvCtx() );
}

void vSimTraceInherit( char cOp, unsigned long ulNum, unsigned long ulPrio )
{
    EV( cOp, NULL, ( long long ) ulNum, ( long long ) ulPrio );
}

unsigned long ulSimTraceTimer( unsigned long ulPeriod, int iAutoReload, const char * pcName )
{
    unsigned long ulId = ++ulNextObj;

    EV( 'Y', pcName, ( long long ) ulId, ( long long ) ulPeriod, iAutoReload ? 1 : 0 );
    return ulId;
}

void vSimTraceTimerExpired( unsigned long ulObj )
{
    EV( 'X', NULL, ( long long ) ulObj );
}

void vSimTraceEventBits( char cOp, unsigned long ulObj, unsigned long ulBits, int iFromIsr )
{
    if( !iFromIsr && ( iSimIrq >= 0 ) )
        prvMisuse( cOp, cOp == 'e' ? "xEventGroupSetBits()" : "xEventGroupClearBits()" );
    EV( cOp, NULL, ( long long ) ulObj, ( long long ) ulBits, prvCtx() );
}

/* ------------------------------------------------------------------ */
/* Called through the wrappers in sim_prelude.h: they tag each object  */
/* with the source line that created it, so frsim.py can name it after */
/* the variable it is assigned to.                                     */

static void prvTagLine( unsigned long ulObj, int iLine )
{
    EV( 'N', NULL, ( long long ) ulObj, iLine );
}

void * pvSimTagQueue( void * pvQueue, int iLine )
{
    if( pvQueue != NULL )
        prvTagLine( uxQueueGetQueueNumber( ( QueueHandle_t ) pvQueue ), iLine );
    return pvQueue;
}

void * pvSimTagTimer( void * pvTimer, int iLine )
{
    if( pvTimer != NULL )
        prvTagLine( uxTimerGetTimerNumber( ( TimerHandle_t ) pvTimer ), iLine );
    return pvTimer;
}

void * pvSimTagEventGroup( void * pvGroup, int iLine )
{
    if( pvGroup != NULL )
        prvTagLine( uxEventGroupGetNumber( ( EventGroupHandle_t ) pvGroup ), iLine );
    return pvGroup;
}

void vSimTaskFn( const char * pcFn, unsigned long ulDepth )
{
    pcNextTaskFn = pcFn;
    ulNextTaskDepth = ulDepth;
}

long lSimTaskCreated( long lResult )
{
    pcNextTaskFn = NULL;
    ulNextTaskDepth = 0;
    return lResult;
}

void * pvSimTaskCreated( void * pvHandle )
{
    pcNextTaskFn = NULL;
    ulNextTaskDepth = 0;
    return pvHandle;
}

/* ------------------------------------------------------------------ */
/* Debug mode: the user file is compiled with                          */
/* -fsanitize-coverage=trace-pc, so every basic block calls this hook. */
/* "E ctx pc" = block entered. A block seen again among the last few   */
/* blocks of the same thread, with no kernel event in between, is a    */
/* busy loop: it is counted, not written, and "W ctx n" reports n.     */

#define simDBG_HIST    8

static __thread unsigned long ulHist[ simDBG_HIST ];
static __thread int iHistN, iHistPos, iInDbg;
static __thread unsigned long ulLooped;

static long long prvDbgCtx( void )
{
    return ( iSimIrq >= 0 ) ? 100000LL + iSimIrq : ( long long ) ulCurTask;
}

static void prvDebugFlush( void )
{
    if( iInDbg )
        return;
    iInDbg = 1;
    if( ulLooped )
    {
        const long long v[] = { prvDbgCtx(), ( long long ) ulLooped };

        vSimEvent( 'W', 2, v, NULL );
        ulLooped = 0;
    }
    iHistN = iHistPos = 0;
    iInDbg = 0;
}

void __sanitizer_cov_trace_pc( void )
{
    unsigned long pc = ( unsigned long ) __builtin_return_address( 0 );
    int i;

    if( !iDebug || ( iTraceFd < 0 ) || iInDbg )
        return;
    for( i = 0; i < iHistN; i++ )
    {
        if( ulHist[ i ] == pc )
        {
            ulLooped++;
            return;
        }
    }
    iInDbg = 1;
    if( ulLooped )
    {
        const long long w[] = { prvDbgCtx(), ( long long ) ulLooped };

        vSimEvent( 'W', 2, w, NULL );
        ulLooped = 0;
        iHistN = iHistPos = 0;
    }
    {
        const long long v[] = { prvDbgCtx(), ( long long ) pc };

        vSimEvent( 'E', 2, v, NULL );
    }
    ulHist[ iHistPos ] = pc;
    iHistPos = ( iHistPos + 1 ) % simDBG_HIST;
    if( iHistN < simDBG_HIST )
        iHistN++;
    iInDbg = 0;
}

void vSimLine( int iLine )
{
    if( iDebug )
    {
        const long long v[] = { prvDbgCtx(), iLine };

        vSimEvent( 'u', 2, v, NULL );
    }
}
