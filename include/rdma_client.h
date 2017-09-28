#ifndef RDMA_RDMA_CLIENT_H_
#define RDMA_RDMA_CLIENT_H_

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

Context *RdmaConnect(const char *host, int port);
void RdmaDisconnect(Context *context);

#ifdef __cplusplus
}
#endif

#endif // RDMA_RDMA_CLIENT_H_
