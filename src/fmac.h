#pragma once

#include <tda5340.h>

/* max packet length in bytes, including preamble, runin and crc, 8b10b encoded payload */
#define FMAC_MAX_PACKET_LEN (64)

/* size in bytes */
typedef bool (*fmacTxCallback) (void * const data,
		const void ** const payload, size_t * const size);
typedef bool (*fmacRxCallback) (void * const data, const void * const payload,
			const size_t size);

#include "packet.h"

typedef struct {
	volatile enum {
		FMAC_IDLE,
		/* sending a packet */
		FMAC_SEND,
		/* waiting after sending a framelet sequence */
		FMAC_WAIT_END,
	} state;

	/* framelet length (whole packet on air), payload len (no preamble, crc, …) */
	uint8_t frameletLen, payloadLen;
	/* fmac base unit, δ, in μs */
	uint32_t delta;
	/* station id, i and number of stations, n*/
	uint32_t i, n;
	const uint32_t *k;
	uint32_t kmax;
	/* current packet repetiton */
	uint8_t repetition;

	/* current framelet */
	uint8_t rxPacket[FMAC_MAX_PACKET_LEN], txPacket[FMAC_MAX_PACKET_LEN];
	bool rxPacketValid, txPacketValid;

	tda5340Ctx *tda;
	packetEncoder enc;

	/* callbacks and data */
	fmacTxCallback txcb;
	fmacRxCallback rxcb;
	void *cbdata;

	uint8_t initialized;
} fmacCtx;

void fmacIrqHandle (fmacCtx * const fm);
bool fmacSend (fmacCtx * const fm, const uint8_t * const buf, const uint8_t len);
void fmacInit (fmacCtx * const fm, const uint8_t i, const uint8_t n,
		tda5340Ctx * const tda, const uint8_t payloadSize);

inline static bool fmacCanSend (fmacCtx * const fm) {
	return fm->state == FMAC_IDLE;
}
