/*
 * simtrace.h - timeline trace API for your own scheduler written in C.
 * Force-included in "C" mode; no FreeRTOS involved.
 *
 *   int id = st_task("blink", 2);   register a timeline row (name, priority)
 *   st_run(id);                     id is now running (the previous one becomes Ready)
 *   st_block(id);                   id is Blocked (waiting, sleeping, suspended)
 *   st_ready(id);                   id is Ready (e.g. woken by an event)
 *   st_idle();                      CPU idle (the running task becomes Ready)
 *   st_exit(id);                    id terminated
 *   st_time(t);                     switch to virtual time (your tick counter); otherwise real time in us
 *   st_tick();                      draw a tick line at the current time
 *   st_mark("text");                annotate the current time
 */
#ifndef SIMTRACE_H
#define SIMTRACE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int  st_task( const char * name, int prio );
void st_run( int id );
void st_block( int id );
void st_ready( int id );
void st_idle( void );
void st_exit( int id );
void st_time( unsigned long long t );
void st_tick( void );
void st_mark( const char * label );

#endif /* SIMTRACE_H */
