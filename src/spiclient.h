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
#include <xmc_spi.h>

#include "fifo.h"
#include "fmac.h"

/* attention: item start must be aligned to 4 bytes */
#define SPICLIENT_RX_ITEM_SIZE FMAC_MAX_PACKET_LEN
#define SPICLIENT_TX_ITEM_SIZE FMAC_MAX_PACKET_LEN

typedef void (*spiclientInitMac) (void * data, const uint8_t i, const uint8_t n,
		const uint8_t payloadSize);
typedef void (*spiclientTriggerSend) (void * data);

typedef struct {
	XMC_USIC_CH_t *dev;
	fifo rxFifo, txFifo;
	uint8_t payloadSize;
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

