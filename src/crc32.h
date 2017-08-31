#pragma once

/* crc32 calculation, either using a hardware accelerator found in xmc4500 or a
 * public domain c implementation */
uint32_t crc32Calc (const uint32_t * const data, const size_t len);
unsigned int crc32IncorrectBit (const uint32_t crc);
void crc32Init (const unsigned int msgLen);

