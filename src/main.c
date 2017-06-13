#include <assert.h>

#include <xmc_gpio.h>
#include <xmc_spi.h>
#include <xmc_scu.h>
#include <xmc_ccu4.h>

#include <SEGGER_RTT.h>

#include <tda5340.h>

#include "fmac.h"
#include "config.h"
#include "util.h"
#include "spiclient.h"

static tda5340Ctx tda0;
static fmacCtx fm;
static spiclient spi;

/*	Interrupt handler, forwarding ints to subroutines
 */
void TDA5350IRQHANDLER (void) {
	tda5340IrqHandle (&tda0);
}

void CCU40_0_IRQHandler(void) {
	fmacIrqHandle (&fm);
}

/* Setup clock, called by SystemInit
 */
void SystemCoreClockSetup () {
	static const XMC_SCU_CLOCK_CONFIG_t clkcfg = {
#if UC_SERIES == XMC11
		.idiv = 1, /* 32 mhz mclk */
		.fdiv = 0,
		.pclk_src = XMC_SCU_CLOCK_PCLKSRC_DOUBLE_MCLK, /* 64 mhz pclk */
		.rtc_src = XMC_SCU_CLOCK_RTCCLKSRC_DCO2,
#elif UC_SERIES == XMC45
	/* setup high precision PLL using onboard 12 MHz crystal */
		.syspll_config = {
			/* set to 80 MHz, see manual p. 11-39 */
			.p_div = 3,
			.n_div = 80,
			.k_div = 4,
			.mode = XMC_SCU_CLOCK_SYSPLL_MODE_NORMAL,
			.clksrc = XMC_SCU_CLOCK_SYSPLLCLKSRC_OSCHP,
			},
		.enable_oschp = true,
		.enable_osculp = false,
		.calibration_mode = XMC_SCU_CLOCK_FOFI_CALIBRATION_MODE_FACTORY,
		.fstdby_clksrc = XMC_SCU_HIB_STDBYCLKSRC_OSI,
		.fsys_clksrc = XMC_SCU_CLOCK_SYSCLKSRC_PLL,
		.fsys_clkdiv = 1,
		.fcpu_clkdiv = 1,
		.fccu_clkdiv = 1,
		.fperipheral_clkdiv = 1,
#else
	#error "unsupported mcu"
#endif
		};
	XMC_SCU_CLOCK_Init (&clkcfg);
}

/* 	glue between fmac and spiclient */
/*	init fmac */
static void initMac (void *data, const uint8_t i, const uint8_t n) {
	assert (data != NULL);
	assert (i < n);

	fmacCtx * const fm = data;
	const uint32_t payloadSize = 12;
	fmacInit (fm, i, n, &tda0, payloadSize);
}

/*	trigger tx callback */
static void triggerSend (void *data) {
	assert (data != NULL);

	fmacCtx * const fm = data;
	if (fmacCanSend (fm) && fm->txcb != NULL) {
		const void *data;
		size_t size;
		if (fm->txcb (fm->cbdata, &data, &size)) {
			fmacSend (fm, data, size);
		}
	}
}

int main() {
	SEGGER_RTT_WriteString (0, "RTT bootup complete\r\n");

	XMC_GPIO_CONFIG_t iocfg = {
			.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
			.output_level = XMC_GPIO_OUTPUT_LEVEL_LOW,
			#if UC_SERIES == XMC45
			.output_strength = XMC_GPIO_OUTPUT_STRENGTH_STRONG_SHARP_EDGE,
			#endif
			};
#ifdef TX_LED
	XMC_GPIO_Init (TX_LED, &iocfg);
#endif
#ifdef RX_LED
	XMC_GPIO_Init (RX_LED, &iocfg);
#endif
#ifdef DEBUG_TIMING_PIN
	XMC_GPIO_Init (DEBUG_TIMING_PIN, &iocfg);
#endif

	memset (&tda0, 0, sizeof (tda0));
	tda0.spi = TDA_SPI_CHANNEL;
	tda0.baudrate = TDA_BAUDRATE;
	tda0.retries = 1;
	const uint32_t tdaPriority = NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
			PRIO_TDA_PREEMPT, PRIO_TDA_SUB);
	tda5340Init (&tda0, tdaPriority);

	tda5340Reset (&tda0);
	/* wait until the tda is ready */
	while (tda0.mode != TDA_SLEEP_MODE);

	const uint32_t spiPriority =
			NVIC_EncodePriority (NVIC_GetPriorityGrouping(), PRIO_SPI_PREEMPT,
			PRIO_SPI_SUB);
	memset (&spi, 0, sizeof (spi));
	spiclientInit (&spi, SPICLI_SPI_CHANNEL, spiPriority);

	fm.cbdata = &spi;
	fm.rxcb = spiclientRx;
	fm.txcb = spiclientTx;
	spi.initMac = initMac;
	spi.triggerSend = triggerSend;
	spi.macData = &fm;

#if defined(DEBUG_STATIONID) && defined(DEBUG_NUMSTATIONS)
	initMac (&fm, DEBUG_STATIONID, DEBUG_NUMSTATIONS);
#endif

	while (1);
}

