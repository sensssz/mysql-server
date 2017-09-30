#include "rdma_server.h"

#include <iostream>

RdmaServer::RdmaServer(int port, int backlog) :
    port_(port), backlog_(backlog), current_context_(nullptr),
    outstanding_connections_(0) {}

Status RdmaServer::Initialize() {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET; /* standard IP NET address */
  addr.sin_addr.s_addr = htonl(INADDR_ANY); /* passed address */
  addr.sin_port = htons(port_);

  ERROR_IF_ZERO(event_channel_ = rdma_create_event_channel());
  ERROR_IF_NON_ZERO(rdma_create_id(event_channel_, &cm_id_, nullptr, RDMA_PS_TCP));
  ERROR_IF_NON_ZERO(rdma_bind_addr(cm_id_, (struct sockaddr *)&addr));
  ERROR_IF_NON_ZERO(rdma_listen(cm_id_, backlog_));

  return Status::Ok();
}

int RdmaServer::port() {
  return port_;
}

Context *RdmaServer::Accept() {
  struct rdma_cm_event *event = nullptr;
  while (rdma_get_cm_event(event_channel_, &event) == 0) {
    struct rdma_cm_event event_copy;

    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);

    auto status = OnEvent(&event_copy);
    if (!status.ok()) {
      return nullptr;
    }
    if (event->event == RDMA_CM_EVENT_ESTABLISHED) {
      ++outstanding_connections_;
      std::cerr << "New connection established" << std::endl;
      return current_context_;
    } else if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
      --outstanding_connections_;
    }
  }
  return nullptr;
}

void RdmaServer::Stop() {
  std::cerr << "Stopping server..." << std::endl;
  if (cm_id_ && outstanding_connections_ > 0) {
    std::cerr << "rdma_disconnecting..." << std::endl;
    rdma_disconnect(cm_id_);
  }
  if (cm_id_) {
    std::cerr << "rdma_destroy_id..." << std::endl;
    rdma_destroy_id(cm_id_);
  }
  if (event_channel_) {
    std::cerr << "rdma_destroy_event_channel..." << std::endl;
    rdma_destroy_event_channel(event_channel_);
  }
  std::cerr << "Server shutdown complete" << std::endl;
}

Status RdmaServer::OnAddressResolved(struct rdma_cm_id *id) {
  // For client only.
  return Status::Ok();
}

Status RdmaServer::OnRouteResolved(struct rdma_cm_id *id) {
  // For client only.
  return Status::Ok();
}

Status RdmaServer::OnConnectRequest(struct rdma_cm_id *id) {
  auto status_or = BuildContext(id);
  if (!status_or.ok()) {
    return status_or.status();
  }

  current_context_ = status_or.Take();
  RETURN_IF_ERROR(PostReceive(current_context_));

  struct rdma_conn_param cm_params;
  BuildParams(&cm_params);
  ERROR_IF_NON_ZERO(rdma_accept(id, &cm_params));

  return Status::Ok();
}
