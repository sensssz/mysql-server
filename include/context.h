#ifndef RDMA_CONTEXT_H_
#define RDMA_CONTEXT_H_

#include "my_global.h"
#include "my_thread.h"
#include "spsc_ring_buffer.h"

#include <rdma/rdma_cma.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Context {
  struct rdma_cm_id *id;

  struct ibv_qp *queue_pair;
  struct ibv_context *device_context;
  struct ibv_pd *protection_domain;
  struct ibv_cq *completion_queue;
  struct ibv_comp_channel *completion_channel;

  my_bool connected;

  char *recv_region;
  struct ibv_mr *recv_mr;
  char *send_region;
  struct ibv_mr *send_mr;

  int queue_depth;
  int unsignaled_sends;

  SpscRingBuffer *buffer;

  my_thread_handle cq_poller_thread;
} Context;

void DestroyContext(Context *context);

#ifdef __cplusplus
}
#endif

#endif // RDMA_CONTEXT_H_
