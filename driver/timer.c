//
// Originally created by RUPC on 2024/1/8.
//
#include "bsp/dp32g030/timer.h"
#include "bsp/dp32g030/irq.h"
#include "bsp/dp32g030/syscon.h"
#include "ARMCM0.h"
#include <stdbool.h>

// 1 ms ISR flag – set by TIMERBASE0 overflow, consumed by main loop
volatile bool gNextTimeslice_1ms = false;
// Software millisecond counter incremented every ISR (rolls over at 0xFFFFFFFF ms, ~49.7 days)
static volatile uint32_t s_millis_counter = 0;

void HandlerTIMER_BASE0(void)
{
    TIMERBASE0_IF = (1 << 0); // write-1-to-clear: acknowledge interrupt
    s_millis_counter++;
    gNextTimeslice_1ms = true;
}

void TIM0_INIT(void)
{
    // Enable TIMERBASE0 clock gate
    SYSCON_DEV_CLK_GATE |= SYSCON_DEV_CLK_GATE_TIMER_BASE0_BITS_ENABLE;
    
    // ISR-driven mode: 1 kHz tick (48 MHz / 48000), overflow every 1 ms
    // At 1kHz, ARR=0 means: count 0→overflow = 1 tick = 1ms
    TIMERBASE0_DIV = (48000U - 1U);     // Prescaler: 48MHz / 48000 = 1kHz
    TIMERBASE0_LOW_LOAD = 0U;           // ARR=0: overflow after 1 tick (1ms)
    
    TIMERBASE0_IF = (1 << 0);           // Clear any pending interrupt
    
    TIMERBASE0_IE = (1 << 0);           // Enable overflow interrupt

    TIMERBASE0_EN = (1 << 0);           // Enable low counter
    NVIC_EnableIRQ((IRQn_Type)DP32_TIMER_BASE0_IRQn);
}

// Returns milliseconds
uint32_t timer_millis(void)
{
    return s_millis_counter;
}

// Returns milliseconds elapsed since previous millis value with rollover protection
// prev: Previous millis value from timer_millis()
uint32_t timer_millis_since(uint32_t prev)
{
    uint32_t cur = timer_millis();
    return (cur >= prev) ? (cur - prev) : (UINT32_MAX - prev + 1U + cur);
}