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

#include <8b10b.h>
#include <SEGGER_RTT.h>

#include "packet.h"
#include "config.h"
#include "crc32.h"
#include "fmac.h"

/* packet specifics, XXX length is still hardcoded in a lot of places */
/* 8 bit runin, 16 bit tsi */
#define PREAMBLE (3)
/* after the actual message, N zeros are sent, otherwise the receiver fails to
 * detect the last bit if it is logic one. make sure to change the code below
 * when modifying this value */
#define TRAILING_ZEROS (8)
#define TRAILING_ZEROS_BYTES ((TRAILING_ZEROS-1)/8+1)

/* ===== 8b10b ===== */

static size_t packet8b10bEncode (const uint8_t * const src, const size_t srcLen,
		uint8_t * const dest, const size_t destLen) {
	assert (src != NULL);
	assert (srcLen > 0);
	assert (dest != NULL);

	dest[0] = 0xaa;
	dest[1] = 0x9a;
	dest[2] = 0x69;

	const uint32_t crc32 = crc32Calc ((const uint32_t * const) src, srcLen);

	/* XXX: we could save this memcpy if eightbtenbEncode handles multiple
	 * calls well */
	uint8_t raw[FMAC_MAX_PACKET_LEN];
	const size_t rawSize = srcLen + sizeof (crc32);
	assert (rawSize <= sizeof (raw));
	memcpy (raw, src, srcLen);
	memcpy (&raw[srcLen], &crc32, sizeof (crc32));

	const size_t encodedSizeBits = rawSize*10;
	assert (encodedSizeBits%8 == 0);
	const size_t encodedSizeBytes = encodedSizeBits/8;
	eightbtenbCtx linecode;
	eightbtenbInit (&linecode);
	eightbtenbSetDest (&linecode, &dest[PREAMBLE]);
	eightbtenbEncode (&linecode, raw, rawSize);

	memset (&dest[PREAMBLE+encodedSizeBytes], 0, TRAILING_ZEROS_BYTES);

	const size_t s = PREAMBLE*8+encodedSizeBits+TRAILING_ZEROS;

	return s;
}

static packetDecodeStatus packet8b10bDecode (const uint8_t * const src,
		const size_t srcBits, uint8_t * const dest, const size_t destLen) {
	/* 8b10b decoder expects full symbols, packet must be large enough to carry
	 * crc */
	if (srcBits % 10 != 0 || srcBits <= 40) {
		SEGGER_RTT_printf (0, "packet len fail, %u\n", srcBits);
		return PACKET_DECODE_LINECODE_FAIL;
	}

	eightbtenbCtx linecode;
	eightbtenbInit (&linecode);
	eightbtenbSetDest (&linecode, dest);
	if (!eightbtenbDecode (&linecode, src, srcBits)) {
		SEGGER_RTT_printf (0, "8b10b fail\n");
		return PACKET_DECODE_LINECODE_FAIL;
	}

	/* check crc32 at end of packet */
	uint32_t crc32 = crc32Calc ((uint32_t *) dest, srcBits/8-4);
	if (crc32 != 0) {
		const unsigned int incorrect = crc32IncorrectBit (crc32);
		if (incorrect != -1) {
			SEGGER_RTT_printf (0, "crc mismatch %x, bit %u incorrect\n", crc32, incorrect);
			/* correct that bit */
			dest[incorrect/8] ^= (1<<(incorrect%8));
			/* try again, XXX: is this required or can we just assume the
			 * packet is now correct? */
			crc32 = crc32Calc ((uint32_t *) dest, srcBits/8-4);
			if (crc32 != 0) {
				SEGGER_RTT_printf (0, "uncorrectable 1 bit crc error\n");
				return PACKET_DECODE_ECC_FAIL;
			}
		} else {
			SEGGER_RTT_printf (0, "uncorrectable n bit crc error\n");
			return PACKET_DECODE_CHECKSUM_FAIL;
		}
	}

	return PACKET_DECODE_OK;
}

static size_t packet8b10bTxLen (const size_t payloadLen) {
	return PREAMBLE+(payloadLen+4)*10/8+TRAILING_ZEROS_BYTES;
}

static size_t packet8b10bRxLen (const size_t payloadLen) {
	return (payloadLen+4)*10;
}

void packet8b10bInit (packetEncoder * const enc) {
	enc->encode = packet8b10bEncode;
	enc->decode = packet8b10bDecode;
	enc->txlen = packet8b10bTxLen;
	enc->rxlen = packet8b10bRxLen;
}

/* ===== identity ===== */

static size_t identityEncode (const uint8_t * const src, const size_t srcLen,
		uint8_t * const dest, const size_t destLen) {
	assert (src != NULL);
	assert (srcLen > 0);
	assert (dest != NULL);

	dest[0] = 0xaa;
	dest[1] = 0x9a;
	dest[2] = 0x69;

	const uint32_t crc32 = crc32Calc ((const uint32_t * const) src, srcLen);

	memcpy (&dest[PREAMBLE], src, srcLen);
	memcpy (&dest[PREAMBLE+srcLen], &crc32, sizeof (crc32));
	memset (&dest[PREAMBLE+srcLen+sizeof (crc32)], 0, TRAILING_ZEROS_BYTES);

	const size_t s = PREAMBLE*8+srcLen*8+sizeof (crc32)*8+TRAILING_ZEROS;

	return s;
}

static packetDecodeStatus identityDecode (const uint8_t * const src,
		const size_t srcBits, uint8_t * const dest, const size_t destLen) {
	/* expects full bytes */
	if (srcBits % 8 != 0 || srcBits <= 32) {
		SEGGER_RTT_printf (0, "packet len fail, %u\n", srcBits);
		return PACKET_DECODE_LINECODE_FAIL;
	}

	/* check crc32 at end of packet */
	uint32_t crc32 = crc32Calc ((uint32_t *) dest, srcBits/8-4);
	if (crc32 != 0) {
		const unsigned int incorrect = crc32IncorrectBit (crc32);
		if (incorrect != -1) {
			SEGGER_RTT_printf (0, "crc mismatch %x, bit %u incorrect\n", crc32, incorrect);
			/* correct that bit */
			dest[incorrect/8] ^= (1<<(incorrect%8));
			/* try again, XXX: is this required or can we just assume the
			 * packet is now correct? */
			crc32 = crc32Calc ((uint32_t *) dest, srcBits/8-4);
			if (crc32 != 0) {
				SEGGER_RTT_printf (0, "uncorrectable 1 bit crc error\n");
				return PACKET_DECODE_ECC_FAIL;
			}
		} else {
			SEGGER_RTT_printf (0, "uncorrectable n bit crc error\n");
			return PACKET_DECODE_CHECKSUM_FAIL;
		}
	}

	assert (srcBits/8-4 <= destLen);
	memcpy (dest, src, srcBits/8-4);

	return PACKET_DECODE_OK;
}

static size_t identityTxLen (const size_t payloadLen) {
	return PREAMBLE+payloadLen+4+TRAILING_ZEROS_BYTES;
}

static size_t identityRxLen (const size_t payloadLen) {
	return (payloadLen+4)*8;
}

void packetIdentityInit (packetEncoder * const enc) {
	enc->encode = identityEncode;
	enc->decode = identityDecode;
	enc->txlen = identityTxLen;
	enc->rxlen = identityRxLen;
}
