#include "spiclient.h"
#include <xmc_gpio.h>
#include <assert.h>
#include <stdio.h>
#include "util.h"
#include "config.h"

#include <SEGGER_RTT.h>
#ifdef DEBUG_SPICLIENT
#define debug(f, ...) SEGGER_RTT_printf(0, "spi: " f, ##__VA_ARGS__)
#else
#define debug(...)
#endif

#ifdef DEBUG_SPI_LED
#define DEBUG_RX XMC_GPIO_ToggleOutput (RX_LED);
#define DEBUG_TX XMC_GPIO_ToggleOutput (TX_LED);
#else
#define DEBUG_RX
#define DEBUG_TX
#endif

/* spi pins */
#if UC_SERIES == XMC11
	#define SPI_MISO P2_10 /* SDO, DOUT0 */
	#define SPI_MISO_ALT XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT7
	#define SPI_MOSI P0_6 /* SDI, DX0C */
	#define SPI_MOSI_SRC USIC0_C1_DX0_P0_6
	#define SPI_SS   P2_0 /* NCS, SELIN, DX2F */
	#define SPI_SS_SRC USIC0_C1_DX2_P2_0
	#define SPI_SCLK P0_7 /* SCLK, SCLIN, DX1C */
	#define SPI_SCLK_SRC USIC0_C1_DX1_P0_7
	#define INTERRUPT P2_11 /* interrupt pin for incoming packets */
	/* XXX figure out why this is sr[0] */
	#define ISR USIC0_0_IRQHandler
	#define IRQN USIC0_0_IRQn
#elif UC_SERIES == XMC45
	#define SPI_MISO P0_5 /* SDO, DOUT0 */
	#define SPI_MISO_ALT XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT2
	#define SPI_MOSI P0_4 /* SDI, DX0A */
	#define SPI_MOSI_SRC USIC1_C0_DX0_P0_4
	#define SPI_SS   P0_6 /* NCS, SELIN, DX2A */
	#define SPI_SS_SRC USIC1_C0_DX2_P0_6
	#define SPI_SCLK P0_11 /* SCLK, SCLIN, DX1A */
	#define SPI_SCLK_SRC USIC1_C0_DX1_P0_11
	#define INTERRUPT P0_12 /* interrupt pin for incoming packets */
	#define ISR USIC1_0_IRQHandler
	#define IRQN USIC1_0_IRQn
#endif

typedef enum {
	MODE_RX = 0x0,
	MODE_TX = 0x1,
	MODE_IGNORE = 0x2,
} spiMode;

typedef enum {
	CMD_INVALID = 0x0,
	CMD_READBUF = 0x1,
	CMD_WRITEBUF = 0x2,
	CMD_READREG = 0x3,
	CMD_WRITEREG = 0x4,
	/* not an actual command, but the #cmd’s above */
	CMD_COUNT = 0x5,
} spiclientCommand;

#define PAYLOAD_SIZE (12)

/* request sizes for commands, in bytes */
static const uint8_t commandRequestSize[] = {
	0,
	0,
	PAYLOAD_SIZE, /* 8 byte packet data */
	1, /* 1 byte reg address */
	5, /* 1 byte reg address + 4 bytes reg data */
	};
_Static_assert (arraysize (commandRequestSize) == CMD_COUNT, "every command has a fixed size");

typedef enum {
	REG_INVALID = 0x0,
	/* packets pending in rx queue */
	REG_RXPENDING = 0x2,
	/* packets pending in tx queue */
	REG_TXPENDING = 0x3,
	/* buffer overflows, reset on read */
	REG_RXOVERFLOW = 0x4,
	/* configuration register */
	REG_CONFIG = 0x5,
	/* not an actual register */
	REG_COUNT = 0x6,
} spiclientRegister;

static spiclient *staticClient;
static uint8_t upBuffer[128];

static void reset (spiclient * const client) {
	client->reqPos = 0;
	client->mode = MODE_RX;
}

static void initFifos (spiclient * const client) {
	fifoInit (&client->rxFifo, client->rxData, sizeof (client->rxData), SPICLIENT_RX_ITEM_SIZE);
	fifoInit (&client->txFifo, client->txData, sizeof (client->txData), SPICLIENT_TX_ITEM_SIZE);
}

#include "fmac.h"

/*	Called whenever station wants to send data (i.e. this node own the current slot)
 */
bool spiclientTx (void * const data, const void ** const payload, size_t * const size) {
	assert (data != NULL);
	assert (payload != NULL);
	assert (size != NULL);
	spiclient * const client = (spiclient * const) data;

#ifdef DEBUG_CONTINUOUS_SEND
	const fmacCtx * const fm = client->macData;
	/* debugging: continuously send data */
	static uint32_t seqnum = 0;
	/* must be static, otherwise it will be overwritten on the stack */
	static uint8_t foo[PAYLOAD_SIZE];
	memset (foo, 0, sizeof (foo));
	memcpy (foo, &fm->i, sizeof (fm->i));
	memcpy (&foo[sizeof (fm->i)], &seqnum, sizeof (seqnum));
	++seqnum;
	*payload = foo;
	*size = sizeof (foo);
	return true;
#else
	void * const ret = fifoPop (&client->txFifo);
	if (ret != NULL) {
		*payload = ret;
		*size = PAYLOAD_SIZE;
		return true;
	}
#endif
	return false;
}

/*	Data for this node has been received
 */
bool spiclientRx (void * const data, const void * const payload, const size_t size) {
	assert (data != NULL);
	assert (payload != NULL);

	spiclient * const client = (spiclient * const) data;

	/* dump data to RTT channel 1, used to display it on the host */
	const uint32_t ident = 0x48fac0b4;
	SEGGER_RTT_Write (1, &ident, sizeof (ident));
	SEGGER_RTT_Write (1, payload, size);

	uint8_t * const ret = fifoPushAlloc (&client->rxFifo);

	if (ret != NULL) {
		/* high-low edge signals incoming packet */
		XMC_GPIO_SetOutputLow (INTERRUPT);
		assert (size <= client->rxFifo.entrySize);
		memcpy (ret, payload, size);

		fifoPushCommit (&client->rxFifo);
		XMC_GPIO_SetOutputHigh (INTERRUPT);
		return true;
	} else {
		++client->overflowCount;
		return false;
	}
}

static void queueResponse (XMC_USIC_CH_t * const dev, const void * const data, const size_t size) {
	const uint8_t * const byte = data;
	XMC_USIC_CH_TXFIFO_PutData (dev, 0xaa);
	for (size_t i = 0; i < size; i++) {
		assert (!XMC_USIC_CH_TXFIFO_IsFull (dev));
		XMC_USIC_CH_TXFIFO_PutData (dev, byte[i]);
	}
	XMC_USIC_CH_TXFIFO_PutData (dev, 0xff);
}

void ISR () {
	spiclient * const client = staticClient;
	XMC_USIC_CH_t * const dev = client->dev;

	const uint32_t status = XMC_SPI_CH_GetStatusFlag (dev);
	//debug ("status: %x\n", status);

	/* XXX: assuming slave select and rx can never occur at the same time */
	if (status & XMC_SPI_CH_STATUS_FLAG_DX2T_EVENT_DETECTED) {
		/* slave select was asserted */
		reset (client);
		DEBUG_RX;
		XMC_SPI_CH_ClearStatusFlag (dev,
				XMC_SPI_CH_STATUS_FLAG_DX2T_EVENT_DETECTED |
				XMC_SPI_CH_STATUS_FLAG_DATA_LOST_INDICATION |
				XMC_SPI_CH_STATUS_FLAG_RECEIVE_INDICATION |
				XMC_SPI_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION);
		/* flush RBUF0/RBUF1 */
		XMC_SPI_CH_GetReceivedData (dev);
		XMC_SPI_CH_GetReceivedData (dev);
		debug ("event: cs\n");
	} else if ((status & XMC_SPI_CH_STATUS_FLAG_RECEIVE_INDICATION) ||
			(status & XMC_SPI_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION)) {
		const uint8_t rcvData = XMC_SPI_CH_GetReceivedData (dev);
		//debug ("got: %x\n", rcvData);

		if (status & XMC_SPI_CH_STATUS_FLAG_DATA_LOST_INDICATION) {
			client->mode = MODE_IGNORE;
			XMC_SPI_CH_ClearStatusFlag (dev, XMC_SPI_CH_STATUS_FLAG_DATA_LOST_INDICATION);
			debug ("lost\n");
		}

		if (client->mode == MODE_RX) {
			if (client->reqPos >= arraysize (client->req)) {
				client->mode = MODE_IGNORE;
				return;
			}

			DEBUG_RX;

			client->req[client->reqPos] = rcvData;
			++client->reqPos;
			const uint8_t command = client->req[0];
			if (command < CMD_COUNT && client->reqPos == commandRequestSize[command]+1) {
				debug ("command %x\n", command);
				/* overrideable defaults */
				client->mode = MODE_TX;
				XMC_USIC_CH_TXFIFO_Flush (dev);

				switch (command) {
					case CMD_INVALID:
						client->mode = MODE_IGNORE;
						break;

					/* read receive fifo */
					case CMD_READBUF: {
						void * const ret = fifoPop (&client->rxFifo);
						if (ret != NULL) {
							queueResponse (dev, ret, PAYLOAD_SIZE);
						} else {
							for (size_t i = 0; i < PAYLOAD_SIZE; i++) {
								XMC_USIC_CH_TXFIFO_PutData (dev, 0xff);
							}
						}
						break;
					}

					/* write transmit fifo */
					case CMD_WRITEBUF: {
						void * const ret = fifoPushAlloc (&client->txFifo);
						if (ret != NULL) {
							memcpy (ret, &client->req[1], PAYLOAD_SIZE);
							fifoPushCommit (&client->txFifo);
							client->triggerSend (client->macData);
						}
						break;
					}

					/* read register */
					case CMD_READREG: {
						const uint8_t reg = client->req[1];
						switch (reg) {
							case REG_RXPENDING: {
								const uint32_t items = fifoItems (&client->rxFifo);
								queueResponse (dev, &items, sizeof (items));
								break;
							}

							case REG_TXPENDING: {
								const uint32_t items = fifoItems (&client->txFifo);
								queueResponse (dev, &items, sizeof (items));
								break;
							}

							case REG_RXOVERFLOW:
								queueResponse (dev, &client->overflowCount,
										sizeof (client->overflowCount));
								client->overflowCount = 0;
								break;

							case REG_CONFIG: {
								const uint32_t val = 0;
							#if 0
								const uint32_t val =
										((stationGetMaxRetransmit (sta) & 0xff) << 24) |
										((stationGetBsSlots (sta) & 0xff) << 16) |
										((stationGetClients (sta) & 0xff) << 8) |
										(stationGetClientId (sta) & 0xff);
#endif
								queueResponse (dev, &val, sizeof (val));
								break;
							}

							default: {
								const uint32_t invalid = 0xdeadbeef;
								queueResponse (dev, &invalid, sizeof (invalid));
								SEGGER_RTT_printf (0, "reading invalid register %x\n", reg);
								break;
							}
						}
						break;
					}

					/* write register */
					case CMD_WRITEREG: {
						const uint8_t reg = client->req[1];
						switch (reg) {
							case REG_CONFIG: {
								uint32_t val;
								memcpy (&val, &client->req[2], sizeof (val));
								const uint8_t stationId = val & 0xff;
								const uint8_t numStations = (val >> 8) & 0xff;
								initFifos (client);
								debug ("configuring with i=%u, n=%u\n", stationId, numStations);
								assert (client->initMac != NULL);
								client->initMac (client->macData, stationId, numStations);
								break;
							}

							default:
								/* ignored */
								break;
						}
						break;
					}
				}
			}
		}
	}
}

/*	Init. Use dev as SPI slave. Note that pins at the top must match this dev.
 */
void spiclientInit (spiclient * const client, XMC_USIC_CH_t * const dev,
		const uint32_t priority) {
	assert (client != NULL);
	assert (dev != NULL);

	staticClient = client;
	client->dev = dev;

	initFifos (client);

	SEGGER_RTT_ConfigUpBuffer (1, "data", upBuffer,
			sizeof (upBuffer), SEGGER_RTT_MODE_NO_BLOCK_SKIP);

	/* interrupt config, low-active */
	const XMC_GPIO_CONFIG_t configPP = {
			.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
			.output_level = XMC_GPIO_OUTPUT_LEVEL_HIGH,
			};
	XMC_GPIO_Init (INTERRUPT, &configPP);

	/* SPI config */
	XMC_SPI_CH_CONFIG_t config = {
		.bus_mode = XMC_SPI_CH_BUS_MODE_SLAVE,
		.parity_mode = XMC_USIC_CH_PARITY_MODE_NONE,
		};

	/* init spi */
	XMC_SPI_CH_Init(dev, &config);
	XMC_SPI_CH_Start(dev);

	XMC_SPI_CH_SetInputSource(dev, XMC_SPI_CH_INPUT_DIN0, SPI_MOSI_SRC);
	XMC_SPI_CH_SetInputSource(dev, XMC_SPI_CH_INPUT_SLAVE_SCLKIN, SPI_SCLK_SRC);
	XMC_SPI_CH_SetInputSource(dev, XMC_SPI_CH_INPUT_SLAVE_SELIN, SPI_SS_SRC);
	/* low-active slave select */
	XMC_SPI_CH_EnableInputInversion (dev, XMC_SPI_CH_INPUT_SLAVE_SELIN);
	/* XXX: we want rising edge. why invert? */
	//XMC_SPI_CH_EnableInputInversion (spi, XMC_SPI_CH_INPUT_SLAVE_SCLKIN);
	/* XXX: using CPOL=0, CPHA=1 right now */

	XMC_SPI_CH_SetBitOrderMsbFirst (dev);

	const XMC_GPIO_CONFIG_t configAlt = { .mode = SPI_MISO_ALT };
	const XMC_GPIO_CONFIG_t configTri = { .mode = XMC_GPIO_MODE_INPUT_TRISTATE };
	XMC_GPIO_Init (SPI_MOSI, &configTri);
	XMC_GPIO_SetHardwareControl(SPI_MOSI, XMC_GPIO_HWCTRL_DISABLED);
	XMC_GPIO_Init (SPI_SS, &configTri);
	XMC_GPIO_Init (SPI_SCLK, &configTri);
	XMC_GPIO_Init (SPI_MISO, &configAlt);

	/* enable dx2t event on rising edge (low-active SS, but selin is inverted; there’s no API) */
	dev->DXCR[2] = (dev->DXCR[2] & ~USIC_CH_DX2CR_CM_Msk) | (0x1 << USIC_CH_DX2CR_CM_Pos);
	/* enable receive interrupts */
	uint32_t rxEvents = (XMC_SPI_CH_EVENT_ALTERNATIVE_RECEIVE | XMC_SPI_CH_EVENT_STANDARD_RECEIVE);
	XMC_SPI_CH_ClearStatusFlag (dev, XMC_SPI_CH_STATUS_FLAG_DX2T_EVENT_DETECTED);
	XMC_SPI_CH_EnableEvent (dev, rxEvents | XMC_SPI_CH_EVENT_DX2TIEN_ACTIVATED);
	/* spi client has lower priority than timer and tda */
    NVIC_SetPriority (IRQN, priority);
    NVIC_EnableIRQ (IRQN);

	debug ("done. waiting for input\n");

	XMC_SPI_CH_Receive (dev, XMC_SPI_CH_MODE_STANDARD);
	/* set up fifo, always transmit immediately */
	XMC_USIC_CH_TXFIFO_Configure (dev, 0, XMC_USIC_CH_FIFO_SIZE_16WORDS, -1);
	XMC_SPI_CH_EnableDataTransmission (dev);
}

