#include "spsc_ring_buffer.h"

#include <stdlib.h>
#include <strings.h>

static const size_t kDefaultBufferSize = 1e+7;

static size_t ReadValue(char *buffer, size_t loc, size_t *value) {
  *value = *(size_t *)(buffer + loc);
  return loc + sizeof(size_t);
}

static size_t WriteValue(char *buffer, size_t loc, const size_t value) {
  *(size_t *)(buffer + loc) = value;
  return loc + sizeof(size_t);
}

static size_t WrapIfNecessary(size_t buf_size, size_t loc, size_t rw_size) {
  size_t size_left = buf_size - loc - 1;
  if (size_left < rw_size) {
    return 0;
  }
  return loc;
}

SpscRingBuffer *CreateSpscRingBuffer() {
  SpscRingBuffer *buffer = (SpscRingBuffer *) malloc(sizeof(SpscRingBuffer));
  buffer->buf_size = kDefaultBufferSize;
  buffer->buffer = (char *) malloc(buffer->buf_size * sizeof(char));
  buffer->read_loc = buffer->write_loc = 0;
  return buffer;
}

void DestroySpscRingBuffer(SpscRingBuffer *buffer) {
  free(buffer->buffer);
  buffer->buf_size = 0;
  buffer->buffer = NULL;
  buffer->read_loc = buffer->write_loc = 0;
}

my_bool SpscBufferHasData(SpscRingBuffer *buffer) {
  return my_atomic_load64(&buffer->read_loc) != my_atomic_load64(&buffer->write_loc);
}

size_t SpscBufferRead(SpscRingBuffer *buffer, char *out_buffer, size_t size) {
  size_t read_loc;
  size_t data_size;
  while (!SpscBufferHasData(buffer)) {
    // Left empty.
  }
  read_loc = (size_t) my_atomic_load64(&buffer->read_loc);
  read_loc = WrapIfNecessary(buffer->buf_size, read_loc, sizeof(data_size));
  read_loc = ReadValue(buffer->buffer, read_loc, &data_size);
  if (data_size > size) {
    return 0;
  }
  read_loc = WrapIfNecessary(buffer->buf_size, read_loc, data_size);
  memcpy(out_buffer, buffer->buffer + read_loc, data_size);
  my_atomic_store64(&buffer->read_loc, (int64) read_loc);
  return data_size;
}

void SpscBufferWrite(SpscRingBuffer *buffer, const char *data, size_t size) {
  int64 write_loc = my_atomic_load64(&buffer->write_loc);
  int64 read_loc_required = (write_loc + sizeof(size) + size) % buffer->buf_size;
  while (SpscBufferDataSize(buffer) + size > buffer->buf_size) {
    // Left empty.
  }
  write_loc = WrapIfNecessary(buffer->buf_size, write_loc, sizeof(size));
  write_loc = WriteValue(buffer->buffer, write_loc, size);
  write_loc = WrapIfNecessary(buffer->buf_size, write_loc, size);
  memcpy(buffer->buffer + write_loc, data, size);
  my_atomic_store64(&buffer->write_loc, write_loc + size);
}

my_bool SpscBufferWriteAsync(SpscRingBuffer *buffer, const char *data, size_t size) {
  int64 write_loc = my_atomic_load64(&buffer->write_loc);
  int64 read_loc_required = (write_loc + sizeof(size) + size) % buffer->buf_size;
  while (SpscBufferDataSize(buffer) + size > buffer->buf_size) {
    return FALSE;
  }
  write_loc = WrapIfNecessary(buffer->buf_size, write_loc, sizeof(size));
  write_loc = WriteValue(buffer->buffer, write_loc, size);
  write_loc = WrapIfNecessary(buffer->buf_size, write_loc, size);
  memcpy(buffer->buffer + write_loc, data, size);
  my_atomic_store64(&buffer->write_loc, write_loc + size);
  return TRUE;
}

size_t SpscBufferDataSize(SpscRingBuffer *buffer) {
  size_t read_loc = (size_t) my_atomic_load64(&buffer->read_loc);
  size_t write_loc = my_atomic_load64(&buffer->write_loc);
  if (write_loc < read_loc) {
    return (buffer->buf_size - read_loc) + write_loc;
  }
  return write_loc - read_loc;
}
