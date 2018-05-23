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

#include <xmc_scu.h>

#include <SEGGER_RTT.h>

/*	Override newlib’s default assert function, because it uses stdio
 */
void __assert_func (const char *file, int line, const char *func, const char *exp) {
	__disable_irq ();
	SEGGER_RTT_printf (0, "%s:%lu:%s: %s\n", file, line, func, exp);
	while(1);
}

void hexdump (const void * const data, const size_t size) {
	const uint8_t * const dataref = data;
	for (size_t i = 0; i < size; i++) {
		if (i % 16 == 0) {
			SEGGER_RTT_Write (0, "\n", 1);
		}
		SEGGER_RTT_printf (0, "%02x ", dataref[i]);
	}
	SEGGER_RTT_Write (0, "\n", 1);
}
