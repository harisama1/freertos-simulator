/*
 * sim_hw.c - simulated interrupts and peripherals (see sim_hw.h).
 *
 * Interrupt model on the POSIX port:
 *  - external events are dispatched from vSimHwTick(), which runs inside
 *    the tick interrupt (SIGALRM handler, signals masked). A higher
 *    priority task unblocked by a FromISR call sets xYieldPendings, and
 *    xTaskIncrementTick() then switches to it when the tick ISR ends.
 *  - sim_irq_raise() from a task runs the ISR at once inside a critical
 *    section (the port's FROM_ISR mask is a no-op, so taskENTER_CRITICAL
 *    is what actually masks the tick), then yields if the ISR asked to.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"
#include "sim_hw.h"

#define simMAX_STIM      2048
#define simMAX_PERIODIC  16
#define simUART_BUF      256
#define simMAX_POINTS    64

__thread int iSimIrq = -1;
static __thread int iYieldReq;

typedef struct
{
    const char * pcName;
    SimIsr_t pxIsr;
    uint32_t ( * pxLegacy )( void );
} Irq_t;

typedef struct
{
    TickType_t xDue;
    uint8_t ucKind;     /* 0 irq, 1 gpio level, 2 uart byte */
    uint8_t ucDone;
    int16_t sIrq;
    int iArg;           /* pin or byte */
    int iLevel;
} Stim_t;

static Irq_t xIrq[ simMAX_IRQ ];
static Stim_t xStim[ simMAX_STIM ];
static int iNumStim;
static struct { int irq; TickType_t period, next; } xPeriodic[ simMAX_PERIODIC ];
static int iNumPeriodic;
static unsigned char ucPin[ simMAX_PINS ];
static unsigned char ucRx[ simUART_BUF ];
static unsigned uRxHead, uRxTail;
static struct { int n; uint32_t ms[ simMAX_POINTS ]; uint32_t val[ simMAX_POINTS ]; } xAdc[ simMAX_ADC ];

#define EV( code, name, ... )                                                  \
    do {                                                                       \
        const long long v[] = { __VA_ARGS__ };                                 \
        vSimEvent( code, ( int ) ( sizeof( v ) / sizeof( v[ 0 ] ) ), v, name ); \
    } while( 0 )

static void prvWarn( const char * msg )
{
    if( write( STDERR_FILENO, msg, strlen( msg ) ) < 0 )
    {
    }
}

static TickType_t prvMsToTicks( uint32_t ms )
{
    return ( TickType_t ) ( ( ( uint64_t ) ms * configTICK_RATE_HZ + 999U ) / 1000U );
}

static uint32_t prvNowMs( void )
{
    TickType_t t = ( iSimIrq >= 0 ) ? xTaskGetTickCountFromISR() : xTaskGetTickCount();

    return ( uint32_t ) ( ( uint64_t ) t * 1000U / configTICK_RATE_HZ );
}

static int prvValidIrq( int irq )
{
    if( ( irq >= 0 ) && ( irq < simMAX_IRQ ) )
        return 1;
    prvWarn( "[sim] invalid IRQ number (0..31)\n" );
    return 0;
}

static int prvValidPin( int pin )
{
    if( ( pin >= 0 ) && ( pin < simMAX_PINS ) )
        return 1;
    prvWarn( "[sim] invalid GPIO pin (0..63)\n" );
    return 0;
}

static void prvAddStim( TickType_t xDue, int kind, int irq, int arg, int level )
{
    if( iNumStim >= simMAX_STIM )
    {
        prvWarn( "[sim] too many scheduled input events (max 2048)\n" );
        return;
    }
    xStim[ iNumStim ].xDue = xDue;
    xStim[ iNumStim ].ucKind = ( uint8_t ) kind;
    xStim[ iNumStim ].ucDone = 0;
    xStim[ iNumStim ].sIrq = ( int16_t ) irq;
    xStim[ iNumStim ].iArg = arg;
    xStim[ iNumStim ].iLevel = level;
    iNumStim++;
}

/* ------------------------------------------------------------------ */
/* Interrupts                                                          */

static void prvRunIsr( int irq )
{
    static unsigned char ucWarned[ simMAX_IRQ ];
    int iPrev = iSimIrq;

    if( ( xIrq[ irq ].pxIsr == NULL ) && ( xIrq[ irq ].pxLegacy == NULL ) )
    {
        if( !ucWarned[ irq ] )
        {
            char buf[ 96 ];

            snprintf( buf, sizeof( buf ), "[sim] IRQ %d fired but has no handler (sim_irq_attach)\n", irq );
            prvWarn( buf );
            ucWarned[ irq ] = 1;
        }
        return;
    }
    iSimIrq = irq;
    EV( 'i', NULL, irq );
    if( xIrq[ irq ].pxIsr != NULL )
        xIrq[ irq ].pxIsr();
    else
        ( void ) xIrq[ irq ].pxLegacy();   /* the yield request comes from portYIELD_FROM_ISR() */
    EV( 'j', NULL, irq );
    iSimIrq = iPrev;
}

void sim_irq_attach( int irq, const char * name, SimIsr_t isr )
{
    if( !prvValidIrq( irq ) )
        return;
    xIrq[ irq ].pcName = name;
    xIrq[ irq ].pxIsr = isr;
    xIrq[ irq ].pxLegacy = NULL;
    EV( 'J', name ? name : "", irq );
}

void vPortSetInterruptHandler( uint32_t ulInterruptNumber, uint32_t ( * pvHandler )( void ) )
{
    char name[ 16 ];

    if( !prvValidIrq( ( int ) ulInterruptNumber ) )
        return;
    snprintf( name, sizeof( name ), "IRQ%lu", ( unsigned long ) ulInterruptNumber );
    xIrq[ ulInterruptNumber ].pcName = NULL;
    xIrq[ ulInterruptNumber ].pxIsr = NULL;
    xIrq[ ulInterruptNumber ].pxLegacy = pvHandler;
    EV( 'J', name, ( long long ) ulInterruptNumber );
}

void vSimYieldFromISR( int iYield )
{
    if( !iYield )
        return;
    if( iSimIrq >= 0 )
    {
        iYieldReq = 1;
        EV( 'y', NULL, iSimIrq );
    }
    else if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
    {
        taskYIELD();
    }
}

void sim_irq_raise( int irq )
{
    int y;

    if( !prvValidIrq( irq ) )
        return;
    if( ( iSimIrq >= 0 ) || ( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING ) )
    {
        prvRunIsr( irq );       /* nested, or before the scheduler: no switch possible */
        return;
    }
    taskENTER_CRITICAL();
    iYieldReq = 0;
    prvRunIsr( irq );
    y = iYieldReq;
    iYieldReq = 0;
    taskEXIT_CRITICAL();
    if( y )
        taskYIELD();            /* PendSV after the ISR returns */
}

void vPortGenerateSimulatedInterrupt( uint32_t ulInterruptNumber )
{
    sim_irq_raise( ( int ) ulInterruptNumber );
}

void sim_irq_at( int irq, uint32_t ms )
{
    if( prvValidIrq( irq ) )
        prvAddStim( prvMsToTicks( ms ), 0, irq, 0, 0 );
}

void sim_irq_every( int irq, uint32_t period_ms, uint32_t first_ms )
{
    if( !prvValidIrq( irq ) )
        return;
    if( iNumPeriodic >= simMAX_PERIODIC )
    {
        prvWarn( "[sim] too many periodic interrupts (max 16)\n" );
        return;
    }
    xPeriodic[ iNumPeriodic ].irq = irq;
    xPeriodic[ iNumPeriodic ].period = prvMsToTicks( period_ms ) ? prvMsToTicks( period_ms ) : 1;
    xPeriodic[ iNumPeriodic ].next = prvMsToTicks( first_ms );
    iNumPeriodic++;
}

int sim_irq_current( void )
{
    return iSimIrq;
}

/* ------------------------------------------------------------------ */
/* GPIO                                                                */

static void prvSetPin( int pin, int level )
{
    level = level ? 1 : 0;
    if( ucPin[ pin ] != level )
    {
        ucPin[ pin ] = ( unsigned char ) level;
        EV( 'L', NULL, pin, level );
    }
}

void sim_gpio_name( int pin, const char * name )
{
    if( prvValidPin( pin ) )
        EV( 'l', name ? name : "", pin, ucPin[ pin ] );
}

void sim_gpio_write( int pin, int level )
{
    if( prvValidPin( pin ) )
        prvSetPin( pin, level );
}

void sim_gpio_toggle( int pin )
{
    if( prvValidPin( pin ) )
        prvSetPin( pin, !ucPin[ pin ] );
}

int sim_gpio_read( int pin )
{
    return prvValidPin( pin ) ? ucPin[ pin ] : 0;
}

void sim_gpio_input( int pin, const char * script, int irq )
{
    unsigned ms;
    int level, n;

    if( !prvValidPin( pin ) || ( script == NULL ) )
        return;
    if( ( irq >= 0 ) && !prvValidIrq( irq ) )
        irq = -1;
    while( sscanf( script, " %u:%d%n", &ms, &level, &n ) == 2 )
    {
        prvAddStim( prvMsToTicks( ms ), 1, irq, pin, level );
        script += n;
    }
}

void sim_button( int pin, uint32_t press_ms, uint32_t hold_ms, int bounces, int irq )
{
    int i;

    if( !prvValidPin( pin ) )
        return;
    if( ( irq >= 0 ) && !prvValidIrq( irq ) )
        irq = -1;
    prvSetPin( pin, 1 );                    /* pull-up: released = 1 */
    for( i = 0; i < 2 * bounces; i++ )      /* contact bounce, 1 ms per edge */
        prvAddStim( prvMsToTicks( press_ms + ( uint32_t ) i ), 1, irq, pin, i & 1 );
    prvAddStim( prvMsToTicks( press_ms + 2U * ( uint32_t ) bounces ), 1, irq, pin, 0 );
    for( i = 0; i < 2 * bounces; i++ )
        prvAddStim( prvMsToTicks( press_ms + hold_ms + ( uint32_t ) i ), 1, irq, pin, !( i & 1 ) );
    prvAddStim( prvMsToTicks( press_ms + hold_ms + 2U * ( uint32_t ) bounces ), 1, irq, pin, 1 );
}

/* ------------------------------------------------------------------ */
/* UART                                                                */

void sim_uart_rx( uint32_t at_ms, const char * text, int irq )
{
    uint32_t i;

    if( text == NULL )
        return;
    if( ( irq >= 0 ) && !prvValidIrq( irq ) )
        irq = -1;
    for( i = 0; text[ i ] != '\0'; i++ )
        prvAddStim( prvMsToTicks( at_ms + i ), 2, irq, ( unsigned char ) text[ i ], 0 );
}

int sim_uart_getc( void )
{
    int c;

    if( uRxHead == uRxTail )
        return -1;
    c = ucRx[ uRxTail % simUART_BUF ];
    uRxTail++;
    return c;
}

/* ------------------------------------------------------------------ */
/* ADC                                                                 */

void sim_adc_input( int ch, const char * script )
{
    unsigned ms, val;
    int n;

    if( ( ch < 0 ) || ( ch >= simMAX_ADC ) || ( script == NULL ) )
        return;
    xAdc[ ch ].n = 0;
    while( ( xAdc[ ch ].n < simMAX_POINTS ) && ( sscanf( script, " %u:%u%n", &ms, &val, &n ) == 2 ) )
    {
        xAdc[ ch ].ms[ xAdc[ ch ].n ] = ms;
        xAdc[ ch ].val[ xAdc[ ch ].n ] = val;
        xAdc[ ch ].n++;
        script += n;
    }
}

uint32_t sim_adc_read( int ch )
{
    uint32_t t = prvNowMs();
    int i;

    if( ( ch < 0 ) || ( ch >= simMAX_ADC ) || ( xAdc[ ch ].n == 0 ) )
        return 0;
    if( t <= xAdc[ ch ].ms[ 0 ] )
        return xAdc[ ch ].val[ 0 ];
    for( i = 1; i < xAdc[ ch ].n; i++ )
    {
        if( t <= xAdc[ ch ].ms[ i ] )
        {
            uint32_t t0 = xAdc[ ch ].ms[ i - 1 ], t1 = xAdc[ ch ].ms[ i ];
            int64_t v0 = xAdc[ ch ].val[ i - 1 ], v1 = xAdc[ ch ].val[ i ];

            return ( uint32_t ) ( v0 + ( v1 - v0 ) * ( int64_t ) ( t - t0 ) / ( int64_t ) ( t1 > t0 ? t1 - t0 : 1 ) );
        }
    }
    return xAdc[ ch ].val[ xAdc[ ch ].n - 1 ];
}

/* ------------------------------------------------------------------ */
/* Tick: deliver the external events that are due                     */

void vSimHwTick( unsigned long ulTick )
{
    TickType_t xNow = ( TickType_t ) ulTick;
    int i;

    for( i = 0; i < iNumStim; i++ )
    {
        Stim_t * s = &xStim[ i ];

        if( s->ucDone || ( s->xDue > xNow ) )
            continue;
        s->ucDone = 1;
        if( s->ucKind == 1 )
        {
            if( ucPin[ s->iArg ] == ( s->iLevel ? 1 : 0 ) )
                continue;       /* no edge */
            prvSetPin( s->iArg, s->iLevel );
        }
        else if( s->ucKind == 2 )
        {
            if( uRxHead - uRxTail < simUART_BUF )
                ucRx[ uRxHead++ % simUART_BUF ] = ( unsigned char ) s->iArg;
            else
                prvWarn( "[sim] UART RX overrun: the ISR did not read the bytes\n" );
        }
        if( s->sIrq >= 0 )
            prvRunIsr( s->sIrq );
    }
    for( i = 0; i < iNumPeriodic; i++ )
    {
        while( xPeriodic[ i ].next <= xNow )
        {
            xPeriodic[ i ].next += xPeriodic[ i ].period;
            prvRunIsr( xPeriodic[ i ].irq );
        }
    }
}
