#include "rdma_connection.h"
#include "violite.h"
#include "sql_class.h"                  // THD

///////////////////////////////////////////////////////////////////////////
// Channel_info_rdma implementation
///////////////////////////////////////////////////////////////////////////

/**
  This class abstracts the info. about local socket mode of communication with
  the server.
*/
class Channel_info_rdma : public Channel_info
{
public:
  Channel_info_rdma(Context *context) : context_(context) {}

  virtual THD* create_thd()
  {
    THD* thd= Channel_info::create_thd();

    if (thd != NULL)
    {
      thd->security_context()->set_host_ptr(my_localhost, strlen(my_localhost));
    }
    return thd;
  }

  virtual void send_error_and_close_channel(uint errorcode,
                                            int error,
                                            bool senderror)
  {
    Channel_info::send_error_and_close_channel(errorcode, error, senderror);
  }

protected:
  virtual Vio* create_and_init_vio() const
  {
    return rdma_vio_new(context_);
  }

private:
  Context *context_;
};

Rdma_socket_listener::Rdma_socket_listener(int port, int backlog) : server_(port, backlog) {}

bool Rdma_socket_listener::setup_listener() {
  return server_.Initialize().err();
}

/**
  The body of the event loop that listen for connection events from clients.

  @retval Channel_info   Channel_info object abstracting the connected client
                          details for processing this connection.
*/
Channel_info* Rdma_socket_listener::listen_for_connection_event() {
  Context *context = server_.Accept();
  if (context != nullptr) {
    return new Channel_info_rdma(server_.Accept());
  } else {
    return nullptr;
  }
}

/**
  Close the listener.
*/
void Rdma_socket_listener::close_listener() {
  server_.Stop();
}
