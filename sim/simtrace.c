/*
 * simtrace.c - writes the same event lines as sim_support.c (FreeRTOS mode),
 * so frsim.py draws the same timeline: "<time> <code> <args>".
 */
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "simtrace.h"

static int fd = -1;
static int ntasks;
static int cur;                     /* running task id, 0 = none */
static int virt;                    /* 1 after st_time(): timestamps are virtual */
static unsigned long long vnow;
static struct timespec t0;

__attribute__( ( constructor ) ) static void st_init( void )
{
    const char * p = getenv( "SIM_TRACE" );

    clock_gettime( CLOCK_MONOTONIC, &t0 );
    if( p != NULL )
        fd = open( p, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
    setvbuf( stdout, NULL, _IOLBF, 0 );
}

static unsigned long long now( void )
{
    struct timespec t;
    long long ns;

    if( virt )
        return vnow;
    clock_gettime( CLOCK_MONOTONIC, &t );
    ns = ( long long ) ( t.tv_sec - t0.tv_sec ) * 1000000000LL + ( t.tv_nsec - t0.tv_nsec );
    return ns > 0 ? ( unsigned long long ) ns / 1000ULL : 0ULL;
}

static void ev( const char * fmt, ... ) __attribute__( ( format( printf, 1, 2 ) ) );

static void ev( const char * fmt, ... )
{
    char buf[ 256 ];
    int n;
    va_list a;

    if( fd < 0 )
        return;
    n = snprintf( buf, sizeof( buf ), "%llu ", now() );
    va_start( a, fmt );
    n += vsnprintf( buf + n, sizeof( buf ) - ( size_t ) n, fmt, a );
    va_end( a );
    if( n > ( int ) sizeof( buf ) - 2 )
        n = ( int ) sizeof( buf ) - 2;
    buf[ n++ ] = '\n';
    if( write( fd, buf, ( size_t ) n ) < 0 )
        fd = -1;
}

static int valid( int id )
{
    return id >= 1 && id <= ntasks;
}

int st_task( const char * name, int prio )
{
    char clean[ 64 ];
    size_t i;

    for( i = 0; name[ i ] != '\0' && i < sizeof( clean ) - 1; i++ )
        clean[ i ] = ( name[ i ] == '\n' ) ? ' ' : name[ i ];
    clean[ i ] = '\0';
    ntasks++;
    ev( "C %d %d %s", ntasks, prio < 0 ? 0 : prio, clean );
    ev( "R %d", ntasks );
    return ntasks;
}

void st_run( int id )
{
    if( !valid( id ) || id == cur )
        return;
    if( cur )
        ev( "O %d 1", cur );
    ev( "I %d", id );
    cur = id;
}

void st_block( int id )
{
    if( !valid( id ) )
        return;
    ev( "O %d 0", id );
    if( id == cur )
        cur = 0;
}

void st_ready( int id )
{
    if( valid( id ) && id != cur )
        ev( "R %d", id );
}

void st_idle( void )
{
    if( cur )
        ev( "O %d 1", cur );
    cur = 0;
}

void st_exit( int id )
{
    if( !valid( id ) )
        return;
    if( id == cur )
    {
        ev( "O %d 0", id );
        cur = 0;
    }
    ev( "D %d", id );
}

void st_time( unsigned long long t )
{
    if( !virt )
    {
        virt = 1;
        vnow = t;
        ev( "V 0" );
    }
    vnow = t;
}

void st_tick( void )
{
    ev( "T 0" );
}

void st_mark( const char * label )
{
    char clean[ 128 ];
    size_t i;

    for( i = 0; label[ i ] != '\0' && i < sizeof( clean ) - 1; i++ )
        clean[ i ] = ( label[ i ] == '\n' ) ? ' ' : label[ i ];
    clean[ i ] = '\0';
    ev( "K %s", clean );
}
