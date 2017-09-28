#include "my_global.h"
#include "vio_rdma.h"
#include "context.h"

static my_bool PostSend(Context *context, size_t size) {
  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  // We need to do at least one signaled send per kQueueDepth sends.
  int send_flags = 0;
  int num_unsignaled_sends = ++context->unsignaled_sends;
  if (num_unsignaled_sends == context->queue_depth - 10) {
    send_flags = IBV_SEND_SIGNALED;
    context->unsignaled_sends = 0;
  }

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

size_t vio_read_rdma(Vio *vio, uchar * buf, size_t size) {
  return SpscBufferRead(vio->context->buffer, buf, size);
}

size_t vio_write_rdma(Vio *vio, const uchar *buf, size_t size) {
  memcpy(vio->context->send_region, buf, size);
  if (PostSend(vio->context, size)) {
    return size;
  }
  return 0;
}

my_bool vio_is_connected_rdma(Vio *vio) {
  return vio->context->connected;
}

int vio_shutdown_rdma(Vio * vio) {
  DestroyContext(vio->context);
  vio->context = NULL;
}

my_bool has_data_rdma(Vio *vio) {
  return SpscBufferHasData(vio->context->buffer);
}
