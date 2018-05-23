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

/* user/application configuration */

//#define USE_SPI
#define USE_UART

#if 0
/* fixed parameters for debugging without SPI adapter */
#define DEBUG_STATIONID (0)
#define DEBUG_NUMSTATIONS (2)
/* continuously send packets */
//#define DEBUG_CONTINUOUS_SEND
/* wait random time before querying tx callback, depends on continuous send */
//#define DEBUG_RANDOM_DELAY (1000000)
#endif

/* toggle pin for timing events */
#define DEBUG_TIMING_PIN P2_1
//#define DEBUG_TIMING_FMAC_IRQ
//#define DEBUG_TIMING_FMAC_SEND
#define DEBUG_TIMING_FMAC_RCV

/* en/disable debugging output */
//#define DEBUG_FMAC
#define DEBUG_SPICLIENT
//#define DEBUG_DUMP_TXDATA
#define DEBUG_DUMP_RXDATA

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
/* lowest priority, spi interface is asynchronous */
static const uint32_t PRIO_SPI_PREEMPT = 2*PRIO_STEP;
static const uint32_t PRIO_SPI_SUB = 0;

/* must have higher priority than scheduler, since sched is waiting (polling)
 * for tda */
static const uint32_t PRIO_TDA_PREEMPT = 0*PRIO_STEP;
static const uint32_t PRIO_TDA_SUB = 0;

static const uint32_t PRIO_SCHED_PREEMPT = 1*PRIO_STEP;
static const uint32_t PRIO_SCHED_SUB = 0;
#undef PRIO_STEP
#elif UC_SERIES == XMC45
/* lowest priority, spi interface is asynchronous */
static const uint32_t PRIO_SPI_PREEMPT = 16;
static const uint32_t PRIO_SPI_SUB = 0;

/* must have higher priority than scheduler, since sched is waiting (polling)
 * for tda */
static const uint32_t PRIO_TDA_PREEMPT = 14;
static const uint32_t PRIO_TDA_SUB = 0;

static const uint32_t PRIO_SCHED_PREEMPT = 15;
static const uint32_t PRIO_SCHED_SUB = 0;
#endif

