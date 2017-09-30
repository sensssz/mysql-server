#ifndef RDMA_RDMA_SERVER_H_
#define RDMA_RDMA_SERVER_H_

#include "rdma_communicator.h"
#include "status.h"

#include <string>

class RdmaServer : public RdmaCommunicator {
public:
  RdmaServer(int port, int backlog);
  Status Initialize();
  int port();
  Context *Accept();
  void Stop();

protected:
  virtual Status OnAddressResolved(struct rdma_cm_id *id) override;
  virtual Status OnRouteResolved(struct rdma_cm_id *id) override;
  virtual Status OnConnectRequest(struct rdma_cm_id *id) override;

private:
  int port_;
  int backlog_;
  Context *current_context_;
  int outstanding_connections_;
};

#endif // RDMA_RDMA_SERVER_H_
