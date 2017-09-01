#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef enum {
	PACKET_DECODE_OK,
	PACKET_DECODE_FAIL, /* generic failure */
	PACKET_DECODE_LINECODE_FAIL,
	PACKET_DECODE_CHECKSUM_FAIL,
	PACKET_DECODE_ECC_FAIL,
} packetDecodeStatus;

typedef packetDecodeStatus (*packetEncoderDec) (const uint8_t * const src,
		const size_t srcLen, uint8_t * const dest, const size_t destLen);
typedef size_t (*packetEncoderEnc) (const uint8_t * const src,
		const size_t srcLen, uint8_t * const dest, const size_t destLen);
typedef size_t (*packetEncoderLen) (const size_t payloadLen);

typedef struct {
	packetEncoderEnc encode;
	packetEncoderDec decode;
	/* tx len for payload in _bytes_ */
	packetEncoderLen txlen;
	/* rx len for payload in _bits_ */
	packetEncoderLen rxlen;
} packetEncoder;

void packet8b10bInit (packetEncoder * const enc);

