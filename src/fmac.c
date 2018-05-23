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

#include <assert.h>
#include <stdlib.h>
#include <xmc_ccu4.h>
#include <xmc_scu.h>

#include <8b10b.h>
#include <bitbuffer.h>

#include "crc32.h"
#include "fmac.h"
#include "util.h"
#include "config.h"

/* timer settings */
#define SLICE_COMPARE_LOWER CCU40_CC40
#define SLICE_COMPARE_LOWER_NO (0)
#define SLICE_COMPARE_LOWER_SHADOW XMC_CCU4_SHADOW_TRANSFER_SLICE_0
#define SLICE_COMPARE_UPPER CCU40_CC41
#define SLICE_COMPARE_UPPER_NO (1)
#define SLICE_COMPARE_UPPER_SHADOW XMC_CCU4_SHADOW_TRANSFER_SLICE_1
#define MODULE_PTR        CCU40

#if UC_SERIES == XMC11
/* timer frequency, 8 MHz */
#define TIMER_FREQ (4000000)
#define PRESCALER XMC_CCU4_SLICE_PRESCALER_16
#define CORRECTION_US (0)
#elif UC_SERIES == XMC45
/* timer frequency, 10 MHz */
#define TIMER_FREQ (10000000)
#define PRESCALER XMC_CCU4_SLICE_PRESCALER_8
#define CORRECTION_US (0)
#endif
#define US_TO_TICKS(us) (us*(TIMER_FREQ/1000000))

#define RXTX_SWITCHING_US (500)
#define DELTA_SCALER (1)

#include <SEGGER_RTT.h>
#ifdef DEBUG_FMAC
#define debug(f, ...) SEGGER_RTT_printf(0, "fmac: " f, ##__VA_ARGS__)
#else
#define debug(...)
#endif

/*	Go back to rx as soon as packet is sent
 */
static void txempty (tda5340Ctx * const tda, void * const data) {
	/* one-shot */
	tda->txempty = NULL;
	assert (tda->mode == TDA_TRANSMIT_MODE);
	/* go back to receiving after sending a packet */
	while (!tda5340ModeSet (tda, TDA_RUN_MODE_SLAVE, false, TDA_CONFIG_B));
}

static void txready (tda5340Ctx * const tda, void * const data) {
	fmacCtx * const fm = data;
	/* one-shot */
	tda->txready = NULL;

	assert (tda->mode == TDA_TRANSMIT_MODE);
	tda->txempty = txempty;
	tda5340FifoWrite (tda, fm->txPacket, fm->frameletLen*8);

	TX_LED_FIRE;
}

static void txerror (tda5340Ctx * const tda, void * const data) {
	SEGGER_RTT_printf (0, "txerror\n");
}

/*	Switch to tx mode, callback sends packet
 */
static bool flush (fmacCtx * const fm) {
	assert (fm != NULL);

	tda5340Ctx * const tda = fm->tda;

	TX_LED_FIRE;
	tda->txempty = NULL;
	tda->txready = txready;
	/* XXX: should be receiving, but it is not. frequently happens for xmc4500,
	 * but not xmc1100. a reset does not fix the issue either (→txerror). not
	 * sure why. */
	if (tda->mode == TDA_TRANSMIT_MODE) {
		SEGGER_RTT_printf (0, "flush: already in tx\n");
		txready (tda, fm);
	} else {
		if (!tda5340ModeSet (tda, TDA_TRANSMIT_MODE, false, TDA_CONFIG_A)) {
			TX_LED_FIRE;
			tda->txready = NULL;
			return false;
		}
	}
	return true;
}

static void rxeom (tda5340Ctx * const tda, void * const data) {
	fmacCtx * const fm = data;
	assert (fm != NULL);

	RX_LED_FIRE;

	uint8_t rxPacket[FMAC_MAX_PACKET_LEN];
	bitbuffer rxPacketBuf;
	bitbufferInit (&rxPacketBuf, (uint32_t *) rxPacket, sizeof (rxPacket)*8);

	while (true) {
		uint32_t block;
		uint8_t bits;
		if (!tda5340FifoRead (tda, &block, &bits)) {
			debug ("fifo overflow\n");
			goto done;
		}
		if (bits == 0) {
			/* eom */
			break;
		}
		if (!bitbufferPush32 (&rxPacketBuf, block, bits)) {
			assert (0);
		}
	}
	const uint32_t rxLen = bitbufferLength (&rxPacketBuf);
	//debug ("received %u bits\n", rxLen);

	if (fm->enc.decode (rxPacket, rxLen, fm->rxPacket,
			sizeof (fm->rxPacket)) == PACKET_DECODE_OK && fm->rxcb != NULL) {
		fm->rxcb (fm->cbdata, fm->rxPacket, fm->payloadLen);
	}

	DEBUG_TIMING_FMAC_RCV_FIRE;
done: ;
	RX_LED_FIRE;
}

static void stop () {
	XMC_CCU4_SLICE_StopTimer (SLICE_COMPARE_LOWER);
	XMC_CCU4_SLICE_StopTimer (SLICE_COMPARE_UPPER);
}

static void event (fmacCtx * const fm, const uint32_t timer) {
	assert (!XMC_CCU4_SLICE_IsTimerRunning (SLICE_COMPARE_LOWER));
	assert (!XMC_CCU4_SLICE_IsTimerRunning (SLICE_COMPARE_UPPER));

	XMC_CCU4_SLICE_ClearTimer (SLICE_COMPARE_LOWER);
	XMC_CCU4_SLICE_ClearTimer (SLICE_COMPARE_UPPER);
	//SEGGER_RTT_printf (0, "x: %08x\n", timer);

	XMC_CCU4_SLICE_SetTimerCompareMatch (SLICE_COMPARE_LOWER, timer);
	XMC_CCU4_SLICE_SetTimerCompareMatch (SLICE_COMPARE_UPPER, timer >> 16);

	/* transfer new compare value */
	XMC_CCU4_EnableShadowTransfer (MODULE_PTR, SLICE_COMPARE_LOWER_SHADOW);
	XMC_CCU4_EnableShadowTransfer (MODULE_PTR, SLICE_COMPARE_UPPER_SHADOW);

#if 0
	/* XXX: does not fix the issue with xmc1100 */
	/* wait for transfer to complete, no xmclib function for that? */
	_Static_assert (SLICE_COMPARE_LOWER_NO == 0 && SLICE_COMPARE_UPPER_NO == 1, "fix this as well");
	while (MODULE_PTR->GCST & CCU4_GCST_S0SS_Msk || MODULE_PTR->GCST & CCU4_GCST_S1SS_Msk);
#endif

	XMC_CCU4_SLICE_StartTimer (SLICE_COMPARE_LOWER);
	XMC_CCU4_SLICE_StartTimer (SLICE_COMPARE_UPPER);
}

static void dispatch (fmacCtx * const fm) {
	switch (fm->state) {
		case FMAC_IDLE:
			/* pass */
			assert (0);
			break;

		case FMAC_SEND:
			/* sending sequence */
			++fm->repetition;
			if (fm->repetition == fm->n) {
				fm->state = FMAC_WAIT_END;
				fm->txPacketValid = false;
				/* wait t' */
				event (fm, (fm->kmax*(fm->n-1)+1)*fm->delta);
			} else {
				/* wait t_i */
				event (fm, fm->delta*fm->k[fm->i]);
			}
			if (!flush (fm)) {
				/* ignore failed flush, try again next time */
				debug ("flush failed\n");
			}
			break;

		case FMAC_WAIT_END:
			/* done, ready for a new packet */
			fm->state = FMAC_IDLE;
			/* XXX: we should enforce switching to rx here if it failed for some reason */
			if (fm->txcb != NULL) {
#ifdef DEBUG_RANDOM_DELAY
				const unsigned int wait = rand ()%(DEBUG_RANDOM_DELAY);
				for (volatile unsigned int i = 0; i < wait; i++);
#endif
				const void *data;
				size_t size;
				if (fm->txcb (fm->cbdata, &data, &size)) {
					fmacSend (fm, data, size);
				}
			}
			break;

		default:
			assert (0);
			break;
	}
}

/*	Initialize compare timer slices
 */
static void compareInit (const XMC_CCU4_SLICE_PRESCALER_t prescaler,
		const uint32_t priority) {
	XMC_CCU4_SLICE_COMPARE_CONFIG_t config = {
		.timer_mode 		     = XMC_CCU4_SLICE_TIMER_COUNT_MODE_EA,
		/* monoshot does not work well with timer concat */
		.monoshot   		     = XMC_CCU4_SLICE_TIMER_REPEAT_MODE_REPEAT,
		.shadow_xfer_clear   = 0U,
		.dither_timer_period = 0U,
		.dither_duty_cycle   = 0U,
		.prescaler_mode	     = XMC_CCU4_SLICE_PRESCALER_MODE_NORMAL,
		.mcm_enable		       = 0U,
		.prescaler_initval   = prescaler,
		.float_limit		     = 0U,
		.dither_limit		     = 0U,
		.passive_level 	     = XMC_CCU4_SLICE_OUTPUT_PASSIVE_LEVEL_LOW,
		.timer_concatenation = 0U
	};

	/* Get the slice out of idle mode */
	XMC_CCU4_EnableClock(MODULE_PTR, SLICE_COMPARE_LOWER_NO);
	XMC_CCU4_EnableClock(MODULE_PTR, SLICE_COMPARE_UPPER_NO);

	/* Initialize the Slice */
	XMC_CCU4_SLICE_CompareInit(SLICE_COMPARE_LOWER, &config);
	config.timer_concatenation = 1;
	XMC_CCU4_SLICE_CompareInit(SLICE_COMPARE_UPPER, &config);

	/* the signal is forwarded from the lsb slice to the msb slice and combined
	 * there */
	XMC_CCU4_SLICE_EnableEvent(SLICE_COMPARE_UPPER,
			XMC_CCU4_SLICE_IRQ_ID_COMPARE_MATCH_UP);

	/* Configure interrupts */
	/* compare match */
	XMC_CCU4_SLICE_SetInterruptNode(SLICE_COMPARE_UPPER,
			XMC_CCU4_SLICE_IRQ_ID_COMPARE_MATCH_UP, XMC_CCU4_SLICE_SR_ID_0);
	NVIC_SetPriority(CCU40_0_IRQn, priority);
	NVIC_EnableIRQ(CCU40_0_IRQn);

	XMC_CCU4_SLICE_SetTimerPeriodMatch (SLICE_COMPARE_LOWER, 0xffff);
	XMC_CCU4_SLICE_SetTimerPeriodMatch (SLICE_COMPARE_UPPER, 0xffff);
}

#include <tda5340_reg.h>

const tdaConfigVal tdaConfig[] = {
	/* generic config */
#if UC_SERIES == XMC11
	/* csmTDA uses different oscillator */
	{TDA_XTALCAL0, 0x91},
#elif UC_SERIES == XMC45
	/* TDA shield/eval kit */
	{TDA_XTALCAL0, 0x86},
	{TDA_XTALCAL1, 0x09},
#endif

	{TDA_PLLCFG, 0x08},

	/* mask all interrupts, except … */
	{TDA_IM0, (uint8_t) ~((1<<TDA_IM0_FSYNCB_OFF) | (1<<TDA_IM0_EOMB_OFF))},
	{TDA_IM2, (uint8_t) ~((1<<TDA_IM2_TXEMPTY_OFF) | (1<<TDA_IM2_TXREADY_OFF))},

	/* transmitter config */
	{TDA_A_TXCFG, 0x55}, /* nrz encoding */
	{TDA_A_TXPOWER0, 0x00},
	{TDA_A_TXPOWER1, 0x1F}, /* fsk output power 31 */
	//{TDA_A_TXPOWER1, 0x00}, /* fsk output power 0 */

	TDA_CFG_TXFREQ(A, 8698),
	TDA_CFG_TXBAUDRATE(A, 100),

	/* receiver config */
	{TDA_B_IF1, 0x9B},
	{TDA_B_SYSRCTO, 0x92},
	{TDA_B_DIGRXC, 0x41},
	{TDA_B_PDECSCASK, 0x2A},
	{TDA_B_CDRCFG0, 0x8C}, /* 8 chips runin */
	{TDA_B_SLCCFG, 0x8C}, /* slicer mode: nrz */
	//{TDA_B_SLCCFG, 0x75}, /* slicer mode: bit */
	{TDA_B_CHCFG, 0x01},

	{TDA_B_TVWIN, 5*16+8}, /* 5.5 bits without edge detection are tolerated */

	TDA_CFG_RXFREQ(B, 8698),
	TDA_CFG_RXBAUDRATE(B, 100),

	{TDA_B_TSILENA, 0x10}, /* tsi length 16 chips/bits */
	{TDA_B_EOMC, 0x05}, /* eom by data length and sync loss */

	{TDA_B_TSIPTA0, 0x96},
	{TDA_B_TSIPTA1, 0x59},

	/* AFC. Required if both XMC4500 shield and csmTDA are in use concurrently.
	 * Apparently csmTDA’s frequency generation differs slightly from that of
	 * the shield. */
#if 0
	{TDA_B_AFCSFCFG, 0x1}, /* always on */
	{TDA_B_AFCKCFG0, 500&0xff}, /* lower part of k1=2000=1/50*bitrate, see measurements */
	{TDA_B_AFCKCFG1, ((0x0)<<5) | ((500>>8)&0x1f)}, /* upper part of k1 and k2=1/1*k1 */
#endif
	};
const size_t tdaConfigSize = arraysize (tdaConfig);

/* from paper */
static const uint32_t optimalK[] = {
	2, 3, /* n=2 */
	2, 3, 5, /* n=3 */
	};
static const uint32_t optimalKOff[] = {
	-1,
	-1,
	0, /* n=2 */
	2, /* n=3 */
	};

/*	Init fmac. Needs a buf of at least len bytes for storing temporory packets.
 *	len includes runin and sync.
 */
void fmacInit (fmacCtx * const fm, const uint8_t i, const uint8_t n,
		tda5340Ctx * const tda, const uint8_t payloadLen) {
	assert (i < n);
	assert (fm != NULL);
	assert (tda != NULL);

	packet8b10bInit (&fm->enc);
	fm->txPacketValid = false;
	fm->payloadLen = payloadLen;
	fm->frameletLen = fm->enc.txlen (payloadLen);
	assert (fm->frameletLen < FMAC_MAX_PACKET_LEN);
	fm->tda = tda;
	const uint32_t usPerBit = 10;
	/* delta includes packet time and rx→tx/tx→rx switch, δ=2d, for packet length d */
	fm->delta = US_TO_TICKS ((fm->frameletLen*8*usPerBit+RXTX_SWITCHING_US*2)*2)*DELTA_SCALER+US_TO_TICKS(CORRECTION_US);
	fm->i = i;
	fm->n = n;
	assert (n < sizeof (optimalKOff)/sizeof (*optimalKOff));
	fm->k = &optimalK[optimalKOff[n]];
	fm->kmax = 0;
	for (uint8_t j = 0; j < n; j++) {
		fm->kmax = fm->k[j] > fm->kmax ? fm->k[j] : fm->kmax;
	}
	debug ("fmac init station %u, frameletLen %u, delta %u, kmax %u\n", fm->i,
			fm->frameletLen, fm->delta, fm->kmax);
	fm->initialized = true;

	/* set up tda */
	tda->txerror = txerror;
	tda->rxeom = rxeom;
	tda->data = fm;
	tda->fsInitFifo = true;
	bool ret = tda5340RegWriteBulk (tda, tdaConfig, tdaConfigSize);
	assert (ret);
	/* payload bits (8b10b encoded) */
	tda5340RegWrite (tda, TDA_B_EOMDLEN, fm->enc.rxlen (fm->payloadLen));
	tda5340ModeSet (tda, TDA_RUN_MODE_SLAVE, false, TDA_CONFIG_B);

	crc32Init (payloadLen+4);

	XMC_CCU4_SetModuleClock(MODULE_PTR, XMC_CCU4_CLOCK_SCU);
	XMC_CCU4_Init(MODULE_PTR, XMC_CCU4_SLICE_MCMS_ACTION_TRANSFER_PR_CR);
	XMC_CCU4_StartPrescaler(MODULE_PTR);

	const uint32_t priority = NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
			PRIO_SCHED_PREEMPT, PRIO_SCHED_SUB);
	compareInit (PRESCALER, priority);

	/* make sure txcb is called upon startup */
	fm->state = FMAC_WAIT_END;
	dispatch (fm);
}

/*	Start sending payload data of len bytes, excluding preable and crc32
 */
bool fmacSend (fmacCtx * const fm, const uint8_t * const buf, const uint8_t len) {
	if (!fm->initialized || !fmacCanSend (fm)) {
		return false;
	}
	DEBUG_TIMING_FMAC_SEND_FIRE;

	assert (!fm->txPacketValid);
	assert (len == fm->payloadLen);
	size_t actualLenBits = fm->enc.encode (buf, len, fm->txPacket, sizeof (fm->txPacket));
	assert (actualLenBits <= fm->frameletLen*8);

	fm->state = FMAC_SEND;
	fm->txPacketValid = true;
	fm->repetition = 0;
	dispatch (fm);

	return true;
}

void fmacIrqHandle (fmacCtx * const fm) {
	assert (fm != NULL);

	DEBUG_TIMING_FMAC_IRQ_FIRE;
	/* XXX: cleared by hardware? */
	//XMC_CCU4_SLICE_ClearEvent(SLICE_COMPARE_LOWER, XMC_CCU4_SLICE_IRQ_ID_COMPARE_MATCH_UP);
	//XMC_CCU4_SLICE_ClearEvent(SLICE_COMPARE_UPPER, XMC_CCU4_SLICE_IRQ_ID_COMPARE_MATCH_UP);
	stop ();
	//SEGGER_RTT_printf (0, "h: %04x%04x\n", XMC_CCU4_SLICE_GetTimerValue (CCU40_CC41), XMC_CCU4_SLICE_GetTimerValue (CCU40_CC40));

	dispatch (fm);
}

