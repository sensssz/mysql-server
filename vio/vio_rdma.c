#include "my_global.h"
#include "vio_rdma.h"
#include "context.h"

static const int kMaxBufferSize = 1000;

static my_bool PostSend(Context *context, size_t size) {
  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  // We need to do at least one signaled send per kQueueDepth sends.
  int send_flags = 0;
  int num_unsignaled_sends = ++context->unsignaled_sends;
  if (num_unsignaled_sends == context->queue_depth - 10) {
    send_flags = IBV_SEND_SIGNALED;
    context->unsignaled_sends = 0;
  }

  memset(&wr, 0, sizeof(wr));
  wr.opcode = IBV_WR_SEND;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.send_flags = send_flags;

  sge.addr = (uint64_t) context->send_region;
  sge.length = size;
  sge.lkey = context->send_mr->lkey;

  while (!context->connected) {
    // Left empry.
  }
  if (ibv_post_send(context->queue_pair, &wr, &bad_wr) != 0) {
    return FALSE;
  }
  return TRUE;
}

static my_bool PostReceive(Context *context) {
  struct ibv_recv_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  wr.next = NULL;
  wr.sg_list = &sge;
  wr.num_sge = 1;

  sge.addr = (uintptr_t) context->recv_region;
  sge.length = kMaxBufferSize;
  sge.lkey = context->recv_mr->lkey;
  if (ibv_post_recv(context->queue_pair, &wr, &bad_wr) != 0) {
    return FALSE;
  }
  return TRUE;
}

size_t vio_read_rdma(Vio *vio, uchar * buf, size_t size) {
  if (vio->context == NULL) {
    return 0;
  }
  return SpscBufferRead(vio->context->buffer, (char *) buf, size);
}

size_t vio_write_rdma(Vio *vio, const uchar *buf, size_t size) {
  if (vio->context == NULL) {
    return 0;
  }
  *((size_t *) vio->context->send_region) = size;
  memcpy(vio->context->send_region + sizeof(size_t), buf, size);
  if (!PostReceive(vio->context)) {
    return 0;
  }
  if (PostSend(vio->context, size)) {
    return size;
  }
  return 0;
}

my_bool vio_is_connected_rdma(Vio *vio) {
  return vio->context->connected;
}

int vio_shutdown_rdma(Vio * vio) {
  rdma_disconnect(vio->context->id);
  vio->context = NULL;
  return 0;
}

my_bool has_data_rdma(Vio *vio) {
  return SpscBufferHasData(vio->context->buffer);
}
