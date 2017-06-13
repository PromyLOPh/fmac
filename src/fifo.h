#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint8_t *dataStart, *dataEnd;
	/* read/write pointers */
	uint8_t *read, *write;
	size_t entrySize;
	bool pushLocked;
} fifo;

void fifoInit (fifo * const fifo, void * const data, const size_t len,
		const size_t entrySize);
void *fifoPushAlloc (fifo * const fifo);
void fifoPushCommit (fifo * const fifo);
void *fifoPop (fifo * const fifo);
size_t fifoItems (const fifo * const fifo);

