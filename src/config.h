#pragma once

/* user/application configuration */

#if 0
/* fixed parameters for debugging without SPI adapter */
#define DEBUG_STATIONID (0)
#define DEBUG_NUMSTATIONS (2)
/* continuously send packets */
//#define DEBUG_CONTINUOUS_SEND
#endif

/* toggle pin for timing events */
#define DEBUG_TIMING_PIN P2_1
//#define DEBUG_TIMING_FMAC_IRQ
//#define DEBUG_TIMING_FMAC_SEND
#define DEBUG_TIMING_FMAC_RCV

/* en/disable debugging output */
//#define DEBUG_FMAC
//#define DEBUG_SPICLIENT

#define TDA_BAUDRATE (2000000)

/* hardware units used */
#if UC_SERIES == XMC45
#include <xmc_gpio.h>
#define TX_LED P1_0
#define RX_LED P1_1
#endif

#if UC_SERIES == XMC11
	#define TDA_SPI_CHANNEL XMC_SPI0_CH0;
#elif UC_SERIES == XMC45
	#define TDA_SPI_CHANNEL XMC_SPI1_CH1;
#endif

#if UC_SERIES == XMC11
	#define SPICLI_SPI_CHANNEL XMC_SPI0_CH1
#elif UC_SERIES == XMC45
	#define SPICLI_SPI_CHANNEL XMC_SPI1_CH0
#endif

#if UC_SERIES == XMC11
/* XMC1100 has four priority levels from high to low: 0, 64, 128, 192 */
#define PRIO_STEP (64)
/* highest priority. we must react immediately with response */
static const uint32_t PRIO_SPI_PREEMPT = 0*PRIO_STEP;
static const uint32_t PRIO_SPI_SUB = 0;

/* must have higher priority than scheduler, since sched is waiting (polling)
 * for tda */
static const uint32_t PRIO_TDA_PREEMPT = 2*PRIO_STEP;
static const uint32_t PRIO_TDA_SUB = 0;

static const uint32_t PRIO_SCHED_PREEMPT = 1*PRIO_STEP;
static const uint32_t PRIO_SCHED_SUB = 0;
#undef PRIO_STEP
#elif UC_SERIES == XMC45
/* highest priority. we must react immediately with response */
static const uint32_t PRIO_SPI_PREEMPT = 14;
static const uint32_t PRIO_SPI_SUB = 0;

/* must have higher priority than scheduler, since sched is waiting (polling)
 * for tda */
static const uint32_t PRIO_TDA_PREEMPT = 15;
static const uint32_t PRIO_TDA_SUB = 0;

static const uint32_t PRIO_SCHED_PREEMPT = 16;
static const uint32_t PRIO_SCHED_SUB = 0;
#endif

