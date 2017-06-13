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
