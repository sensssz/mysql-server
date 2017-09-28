#ifndef VIO_RDMA_H_
#define VIO_RDMA_H_

#include "my_global.h"
#include "vio_priv.h"

size_t vio_read_rdma(Vio *vio, uchar * buf, size_t size);
size_t vio_write_rdma(Vio *vio, const uchar * buf, size_t size);
my_bool vio_is_connected_rdma(Vio *vio);
int vio_shutdown_rdma(Vio *vio);
my_bool has_data_rdma(Vio *vio);

#endif // VIO_RDMA_H_
