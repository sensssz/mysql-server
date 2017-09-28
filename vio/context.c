#include "context.h"

#include <stdlib.h>

#include <rdma/rdma_cma.h>

void DestroyContext(Context *context) {
  rdma_destroy_qp(context->id);
  ibv_dereg_mr(context->recv_mr);
  ibv_dereg_mr(context->send_mr);
  rdma_destroy_id(context->id);
  rdma_destroy_event_channel(context->event_channel);

  my_thread_cancel(&context->cq_poller_thread);

  DestroySpscRingBuffer(context->buffer);
  free(context->buffer);
  free(context->recv_region);
  free(context->send_region);
}
