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

