/*
Copyright (c) 2015–2018 Lars-Dominik Braun <lars@6xq.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#define _unused_ __attribute__((unused))
#define arraysize(a) (sizeof (a)/sizeof (*a))

#include <xmc_gpio.h>

#include "config.h"

#ifdef DEBUG_TIMING_PIN
	#define DEBUG_TIMING_FIRE XMC_GPIO_ToggleOutput (DEBUG_TIMING_PIN)

	#ifdef DEBUG_TIMING_FMAC_IRQ
		#define DEBUG_TIMING_FMAC_IRQ_FIRE DEBUG_TIMING_FIRE
	#else
		#define DEBUG_TIMING_FMAC_IRQ_FIRE
	#endif

	#ifdef DEBUG_TIMING_FMAC_SEND
		#define DEBUG_TIMING_FMAC_SEND_FIRE DEBUG_TIMING_FIRE
	#else
		#define DEBUG_TIMING_FMAC_SEND_FIRE
	#endif

	#ifdef DEBUG_TIMING_FMAC_RCV
		#define DEBUG_TIMING_FMAC_RCV_FIRE DEBUG_TIMING_FIRE
	#else
		#define DEBUG_TIMING_FMAC_RCV_FIRE
	#endif
#endif

#ifdef TX_LED
	#define TX_LED_FIRE XMC_GPIO_ToggleOutput (TX_LED)
#else
	#define TX_LED_FIRE
#endif

#ifdef RX_LED
	#define RX_LED_FIRE XMC_GPIO_ToggleOutput (RX_LED)
#else
	#define RX_LED_FIRE
#endif

#if UC_SERIES == XMC11
/* emulate if not available */
inline static uint32_t NVIC_GetPriorityGrouping() {
	return 0;
}

inline static uint32_t NVIC_EncodePriority (uint32_t PriorityGroup,
		uint32_t PreemptPriority, uint32_t SubPriority) {
	return PreemptPriority;
}
#endif

void hexdump (const void * const data, const size_t size);

