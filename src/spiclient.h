#pragma once
#include <xmc_spi.h>

#include "fifo.h"

/* attention: item start must be aligned to 4 bytes */
#define SPICLIENT_RX_ITEM_SIZE (12)
#define SPICLIENT_TX_ITEM_SIZE (12)

typedef void (*spiclientInitMac) (void * data, const uint8_t i, const uint8_t n);
typedef void (*spiclientTriggerSend) (void * data);

typedef struct {
	XMC_USIC_CH_t *dev;
	fifo rxFifo, txFifo;
	/* backing memory for fifos */
	uint8_t rxData[SPICLIENT_RX_ITEM_SIZE*3], txData[SPICLIENT_TX_ITEM_SIZE*3];
	/* performance counters */
	uint32_t overflowCount;

	/* glue for MAC */
	spiclientInitMac initMac;
	spiclientTriggerSend triggerSend;
	void *macData;
} spiclient;

void spiclientInit (spiclient * const client, XMC_USIC_CH_t * const dev,
		const uint32_t priority);
bool spiclientRx (void * const data, const void * const payload, const size_t size);
bool spiclientTx (void * const data, const void ** const payload, size_t * const size);

