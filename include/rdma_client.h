#ifndef RDMA_RDMA_CLIENT_H_
#define RDMA_RDMA_CLIENT_H_

#include "rdma_communicator.h"

class RdmaClient : public RdmaCommunicator {
public:
  RdmaClient(std::string hostname, int port);
  StatusOr<Context *> Connect();
  char *GetRemoteBuffer();
  Status SendToServer(size_t size);
  void Disconnect();
  Status CancelOustanding();

protected:
  virtual Status OnAddressResolved(struct rdma_cm_id *id) override;
  virtual Status OnRouteResolved(struct rdma_cm_id *id) override;
  virtual Status OnConnectRequest(struct rdma_cm_id *id) override;

private:
  int port_;
  std::string hostname_;
  Context *context_;
};

#endif // RDMA_RDMA_CLIENT_H_
