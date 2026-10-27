/*
 * sim_hw.h - simulated MCU peripherals for the FreeRTOS simulator.
 * Force-included (via sim_prelude.h) in FreeRTOS mode.
 *
 * Times are milliseconds after vTaskStartScheduler(). External events
 * (sim_irq_at, sim_irq_every, sim_gpio_input, sim_button, sim_uart_rx)
 * are delivered inside the tick interrupt, so their resolution is one
 * tick. An interrupt raised by a task (sim_irq_raise) runs immediately.
 *
 * An ISR runs in interrupt context: only ...FromISR() APIs are valid.
 * End it with portYIELD_FROM_ISR( xHigherPriorityTaskWoken ).
 *
 *   interrupts   sim_irq_attach(irq, "name", isr)   isr: void isr(void)
 *                sim_irq_raise(irq)                 from a task (NVIC_SetPendingIRQ)
 *                sim_irq_at(irq, ms)                one external event
 *                sim_irq_every(irq, period_ms, first_ms)   hardware timer
 *   GPIO         sim_gpio_name(pin, "LED"), sim_gpio_write/toggle/read(pin)
 *                sim_gpio_input(pin, "100:1 250:0", irq)   input levels over time, irq on each edge (-1: none)
 *                sim_button(pin, press_ms, hold_ms, bounces, irq)  active-low push button
 *   UART         sim_uart_rx(at_ms, "text", irq)   one byte per ms, irq per byte
 *                sim_uart_getc()                   next received byte, -1 if empty
 *   ADC          sim_adc_input(ch, "0:0 1000:4095")  piecewise-linear signal (ms:value)
 *                sim_adc_read(ch)
 *
 * Book compatibility (Windows port, chapter 7):
 *   vPortSetInterruptHandler(n, handler), vPortGenerateSimulatedInterrupt(n)
 */
#ifndef SIM_HW_H
#define SIM_HW_H

#include <stdint.h>

#define simMAX_IRQ                         32
#define simMAX_PINS                        64
#define simMAX_ADC                         8
#define portFIRST_USER_INTERRUPT_NUMBER    3

typedef void ( * SimIsr_t )( void );

void sim_irq_attach( int irq, const char * name, SimIsr_t isr );
void sim_irq_raise( int irq );
void sim_irq_at( int irq, uint32_t ms );
void sim_irq_every( int irq, uint32_t period_ms, uint32_t first_ms );
int  sim_irq_current( void );

void sim_gpio_name( int pin, const char * name );
void sim_gpio_write( int pin, int level );
void sim_gpio_toggle( int pin );
int  sim_gpio_read( int pin );
void sim_gpio_input( int pin, const char * script, int irq );
void sim_button( int pin, uint32_t press_ms, uint32_t hold_ms, int bounces, int irq );

void sim_uart_rx( uint32_t at_ms, const char * text, int irq );
int  sim_uart_getc( void );

void sim_adc_input( int ch, const char * script );
uint32_t sim_adc_read( int ch );

void vPortSetInterruptHandler( uint32_t ulInterruptNumber, uint32_t ( * pvHandler )( void ) );
void vPortGenerateSimulatedInterrupt( uint32_t ulInterruptNumber );

/* Internal: shared with sim_support.c and the portYIELD_FROM_ISR wrapper.
 * Thread-local: each task is a pthread, and an ISR that (wrongly) calls a
 * task-level API can switch threads before it returns. */
extern __thread int iSimIrq;     /* IRQ being serviced on this thread, -1 in task context */
void vSimYieldFromISR( int iYield );
void vSimHwTick( unsigned long ulTick );
void vSimEvent( char code, int nvals, const long long * vals, const char * name );

#endif /* SIM_HW_H */
