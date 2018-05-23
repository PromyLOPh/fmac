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

/*	Fixed size fifo with fixed size entries
 */

#include <assert.h>

#include "fifo.h"

void fifoInit (fifo * const fifo, void * const data, const size_t len,
		const size_t entrySize) {
	assert (len % entrySize == 0);

	fifo->entrySize = entrySize;
	fifo->dataStart = data;
	fifo->read = data;
	fifo->write = data;
	fifo->dataEnd = data + len;
}

static uint8_t *advanceWrite (const fifo * const fifo) {
	uint8_t *write = fifo->write;
	write += fifo->entrySize;
	if (write >= fifo->dataEnd) {
		write = fifo->dataStart;
	}
	return write;
}

/*	Return area that can be used for new item
 */
void *fifoPushAlloc (fifo * const fifo) {
	uint8_t *newwrite = advanceWrite (fifo);
	if (newwrite == fifo->read) {
		/* fifo full */
		return NULL;
	}
	fifo->pushLocked = true;
	return fifo->write;
}

void fifoPushCommit (fifo * const fifo) {
	assert (fifo->pushLocked);
	fifo->pushLocked = false;
	fifo->write = advanceWrite (fifo);
}

/*	Pop item from fifo and return address
 */
void *fifoPop (fifo * const fifo) {
	uint8_t *read = fifo->read;
	void * const ret = read;
	if (read == fifo->write) {
		/* fifo empty */
		return NULL;
	}
	read += fifo->entrySize;
	if (read >= fifo->dataEnd) {
		read = fifo->dataStart;
	}
	fifo->read = read;
	return ret;
}

/*	Items currently in fifo
 */
size_t fifoItems (const fifo * const fifo) {
	const uintptr_t read = (uintptr_t) fifo->read, write = (uintptr_t) fifo->write;
	if (read > write) {
		const size_t size = (uintptr_t) fifo->dataEnd - (uintptr_t) fifo->dataStart;
		return (size - (read-write))/fifo->entrySize;
	} else {
		return (write-read)/fifo->entrySize;
	}
}

#ifdef _TEST
/* tests */
#include <check.h>
#include <stdlib.h>

START_TEST (testAll) {
	fifo f;
	uint8_t fdata[128];

	for (unsigned int j = 0; j < 6; j++) {
		const unsigned int itemsize = 1<<j;

		memset (fdata, 0, sizeof (fdata));
		fifoInit (&f, fdata, sizeof (fdata), itemsize);

		const unsigned int maxitems = sizeof (fdata)/itemsize-1;
		for (uint8_t i = 0; i < maxitems; i++) {
			uint8_t * const v = fifoPushAlloc (&f);
			fail_unless (v != NULL);
			memset (v, i, itemsize);
			fail_unless (fifoItems (&f) == i);
			fifoPushCommit (&f);
			fail_unless (fifoItems (&f) == i+1);
		}
		fail_unless (fifoPushAlloc (&f) == NULL);
		for (uint8_t i = 0; i < maxitems; i++) {
			const uint8_t * const v = fifoPop (&f);
			fail_unless (v != NULL);
			fail_unless (memcmp (v, &i, sizeof (i)) == 0, "expected %u, got %u", i, *v);
		}
		fail_unless (fifoPop (&f) == NULL, "fifo should be empty now");
	}
} END_TEST

Suite *test() {
	Suite *s = suite_create ("fifo");

	/* add generic tests */
	TCase *tc_core = tcase_create ("generic");
	tcase_add_test (tc_core, testAll);
	suite_add_tcase (s, tc_core);

	return s;
}

/*	test suite runner
 */
int main (int argc, char **argv) {
	int numberFailed;
	SRunner *sr = srunner_create (test ());

	srunner_run_all (sr, CK_ENV);
	numberFailed = srunner_ntests_failed (sr);
	srunner_free (sr);

	return (numberFailed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif

