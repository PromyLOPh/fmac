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
void packetIdentityInit (packetEncoder * const enc);

