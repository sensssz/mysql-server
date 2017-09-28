#include "rdma_communicator.h"
#include "status.h"

namespace sqpkv {

// 16MB
const int kMaxBufferSize = 1.6e+7;
const int kQueueDepth = 2048;

RdmaCommunicator::RdmaCommunicator() : cm_id_(nullptr), event_channel_(nullptr) {}

Status RdmaCommunicator::OnConnection(struct rdma_cm_id *id) {
  reinterpret_cast<Context *>(id->context)->connected = true;
  return Status::Ok();
}

Status RdmaCommunicator::OnDisconnect(struct rdma_cm_id *id) {
  DestroyConnection(id->context);
  return Status::Ok();
}

Status RdmaCommunicator::OnEvent(struct rdma_cm_event *event) {
  switch (event->event) {
  case RDMA_CM_EVENT_ADDR_RESOLVED:
    return OnAddressResolved(event->id);
  case RDMA_CM_EVENT_ROUTE_RESOLVED:
    return OnRouteResolved(event->id);
  case RDMA_CM_EVENT_CONNECT_REQUEST:
    return OnConnectRequest(event->id);
  case RDMA_CM_EVENT_ESTABLISHED:
    return OnConnection(event->id);
  case RDMA_CM_EVENT_DISCONNECTED:
    return OnDisconnect(event->id);
  default:
    return Status::Err();
  }
}

void RdmaCommunicator::OnWorkCompletion(Context *context, struct ibv_wc *wc) {
  if (wc->status != IBV_WC_SUCCESS) {
    std::cerr << "OnWorkCompletion: status is not success: " << ibv_wc_status_str(wc->status) << std::endl;
    return;
  }
  if (wc->opcode & IBV_WC_RECV) {
    size_t size = *(reinterpret_cast<size_t *>(context->recv_region));
    SpscBufferWrite(context->buffer, context->recv_region + sizeof(size_t), size);
  }
}

void *RdmaCommunicator::PollCompletionQueue(void *arg) {
  Context *context = (Context *) arg;
  struct ibv_cq *cq = context->completion_queue;
  struct ibv_cq *ev_cq;
  struct ibv_wc wc;
  Context *queue_context;

  while (true) {
    RETURN_IF_NON_ZERO(ibv_get_cq_event(context->completion_channel, &ev_cq, reinterpret_cast<void **>(&queue_context)));
    ibv_ack_cq_events(ev_cq, 1);
    RETURN_IF_NON_ZERO(ibv_req_notify_cq(ev_cq, 0));

    while (ibv_poll_cq(cq, 1, &wc) > 0) {
      OnWorkCompletion(context, &wc);
    }
  }

  return nullptr;
}

Status RdmaCommunicator::PostReceive(Context *context) {
  struct ibv_recv_wr wr, *bad_wr = nullptr;
  struct ibv_sge sge;

  wr.next = nullptr;
  wr.sg_list = &sge;
  wr.num_sge = 1;

  sge.addr = reinterpret_cast<uintptr_t>(context->recv_region);
  sge.length = kMaxBufferSize;
  sge.lkey = context->recv_mr->lkey;
  ERROR_IF_NON_ZERO(ibv_post_recv(context->queue_pair, &wr, &bad_wr));
  return Status::Ok();
}

Status RdmaCommunicator::PostSend(Context *context, size_t size) {
  struct ibv_send_wr wr, *bad_wr = nullptr;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  // We need to do at least one signaled send per kQueueDepth sends.
  int send_flags = 0;
  int num_unsignaled_sends = ++context->unsignaled_sends;
  if (num_unsignaled_sends == kQueueDepth - 10) {
    send_flags = IBV_SEND_SIGNALED;
    context->unsignaled_sends = 0;
  }

  wr.opcode = IBV_WR_SEND;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.send_flags = send_flags;

  sge.addr = reinterpret_cast<uintptr_t>(context->send_region);
  sge.length = size;
  sge.lkey = context->send_mr->lkey;

  while (!context->connected) {
    // Left empry.
  }
  ERROR_IF_NON_ZERO(ibv_post_send(context->queue_pair, &wr, &bad_wr));
  return Status::Ok();
}

Status RdmaCommunicator::InitContext(Context *context, struct rdma_cm_id *id) {
  context->connected = false;
  context->id = id;
  context->device_context = id->verbs;
  ERROR_IF_ZERO(context->protection_domain = ibv_alloc_pd(context->device_context));
  ERROR_IF_ZERO(context->completion_channel = ibv_create_comp_channel(context->device_context));
  ERROR_IF_ZERO(context->completion_queue =
    ibv_create_cq(context->device_context, 64, nullptr, context->completion_channel, 0));
  ERROR_IF_NON_ZERO(ibv_req_notify_cq(context->completion_queue, 0));
  my_thread_create(&context->cq_poller_thread, NULL, RdmaCommunicator::PollCompletionQueue, context);

  struct ibv_qp_init_attr queue_pair_attr;
  BuildQueuePairAttr(context, &queue_pair_attr);
  ERROR_IF_NON_ZERO(rdma_create_qp(id, context->protection_domain, &queue_pair_attr));
  context->queue_pair = id->qp;

  id->context = context;

  RETURN_IF_ERROR(RegisterMemoryRegion(context));
  context->unsignaled_sends = 0;
  return Status::Ok();
}

Status RdmaCommunicator::PostInitContext(Context *context) {
  return Status::Ok();
}

StatusOr<Context> RdmaCommunicator::BuildContext(struct rdma_cm_id *id) {
  auto context = std::make_unique<Context>();
  auto status = InitContext(context.get(), id);
  if (!status.ok()) {
    return std::move(status);
  }
  RETURN_IF_ERROR(PostInitContext(context.get()));
  return std::move(context);
}

void RdmaCommunicator::BuildQueuePairAttr(Context *context, struct ibv_qp_init_attr* attributes) {
  memset(attributes, 0, sizeof(*attributes));

  attributes->send_cq = context->completion_queue;
  attributes->recv_cq = context->completion_queue;
  attributes->qp_type = IBV_QPT_RC;
  attributes->sq_sig_all = 0;

  attributes->cap.max_send_wr = kQueueDepth;
  attributes->cap.max_recv_wr = kQueueDepth;
  attributes->cap.max_send_sge = 1;
  attributes->cap.max_recv_sge = 1;
}

void RdmaCommunicator::BuildParams(struct rdma_conn_param *params) {
  memset(params, 0, sizeof(*params));

  params->initiator_depth = params->responder_resources = 7;
  params->rnr_retry_count = 7; /* infinite retry */
}

Status RdmaCommunicator::RegisterMemoryRegion(Context *context) {
  context->recv_region = reinterpret_cast<char *>(malloc(kMaxBufferSize * sizeof(char)));
  context->send_region = reinterpret_cast<char *>(malloc(kMaxBufferSize * sizeof(char)));

  ERROR_IF_ZERO(context->recv_mr = ibv_reg_mr(
    context->protection_domain,
    context->recv_region,
    kMaxBufferSize,
    IBV_ACCESS_LOCAL_WRITE));

  ERROR_IF_ZERO(context->send_mr = ibv_reg_mr(
    context->protection_domain,
    context->send_region,
    kMaxBufferSize,
    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ));

  return Status::Ok();
}

void RdmaCommunicator::DestroyConnection(void *context_void) {
  Context *context = reinterpret_cast<Context *>(context_void);
  delete context;
}

} // namespace sqpkv
