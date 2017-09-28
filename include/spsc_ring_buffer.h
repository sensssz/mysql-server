#ifndef SPSC_RING_BUFFER_H_
#define SPSC_RING_BUFFER_H_

#include "my_global.h"
#include "my_atomic.h"

typedef struct SpscRingBuffer {
  volatile int64_t read_loc;
  volatile int64_t write_loc;
  uint64_t buf_size;
  char *  buffer;
} SpscRingBuffer;

SpscRingBuffer *CreateSpscRingBuffer();
void DestroySpscRingBuffer(SpscRingBuffer *buffer);
my_bool SpscBufferHasData(SpscRingBuffer *buffer);
size_t SpscBufferRead(SpscRingBuffer *buffer, char *out_buffer, size_t size);
void SpscBufferWrite(SpscRingBuffer *buffer, const char *data, size_t size);
my_bool SpscBufferWriteAsync(SpscRingBuffer *buffer, const char *data, size_t size);

#endif // SPSC_RING_BUFFER_H_
