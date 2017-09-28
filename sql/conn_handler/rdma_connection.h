#ifndef CONN_HANDLER_RDMA_CONNECTION_H_
#define CONN_HANDLER_RDMA_CONNECTION_H_

#include "my_global.h"
#include "channel_info.h"
#include "rdma_server.h"

class Rdma_socket_listener {
public:
  Rdma_socket_listener(int port, int backlog);
  /**
    Set up a listener.

    @retval false listener listener has been setup successfully to listen for connect events
            true  failure in setting up the listener.
  */
  bool setup_listener();

  /**
    The body of the event loop that listen for connection events from clients.

    @retval Channel_info   Channel_info object abstracting the connected client
                            details for processing this connection.
  */
  Channel_info* listen_for_connection_event();

  /**
    Close the listener.
  */
  void close_listener();

private:
  RdmaServer server_;
};

#endif // CONN_HANDLER_RDMA_CONNECTION_H_
