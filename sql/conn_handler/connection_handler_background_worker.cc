/*
   Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "connection_handler_impl.h"

#include "channel_info.h"                // Channel_info
#include "connection_handler_manager.h"  // Connection_handler_manager
#include "mysqld.h"                      // max_connections
#include "mysqld_error.h"                // ER_*
#include "mysqld_thd_manager.h"          // Global_THD_manager
#include "sql_audit.h"                   // mysql_audit_release
#include "sql_class.h"                   // THD
#include "sql_connect.h"                 // close_connection
#include "sql_parse.h"                   // do_command
#include "sql_thd_internal_api.h"        // thd_set_thread_stack
#include "log.h"                         // Error_log_throttle

ulong num_workers;

static void *process_client_requests(void *)
{
	my_thread_init();
	Global_THD_manager *manager = Global_THD_manager::get_instance();
	while (!abort_loop)
	{
		THD *thd = NULL;
		if (!manager->try_get_thd(thd)) {
			std::this_thread::sleep_for(std::chrono::microseconds(10));
			continue;
		}
		thd->store_globals();
		thd_set_thread_stack(thd, (char*) &thd);
#ifdef HAVE_PSI_THREAD_INTERFACE
		/*
		 Reusing existing pthread:
		 Create new instrumentation for the new THD job,
		 and attach it to this running pthread.
		 */
		PSI_thread *psi= PSI_THREAD_CALL(new_thread)
			(key_thread_one_connection, thd, thd->thread_id());
		PSI_THREAD_CALL(set_thread_os_id)(psi);
		PSI_THREAD_CALL(set_thread)(psi);
		/* Save it within THD, so it can be inspected */
		thd->set_psi(psi);
#endif /* HAVE_PSI_THREAD_INTERFACE */
		mysql_thread_set_psi_id(thd->thread_id());
		mysql_thread_set_psi_THD(thd);
		mysql_socket_set_thread_owner(thd->get_protocol_classic()->get_vio()->mysql_socket);
		while (thd_connection_alive(thd))
		{
			int do_res = do_command(thd);
			if (do_res == 1)
			{
				// End of transaction, put it back
				manager->put_thd(thd);
				break;
			}
			else if (do_res == 2)
			{
				end_connection(thd);
				close_connection(thd, 0, false, false);

				thd->get_stmt_da()->reset_diagnostics_area();
				thd->release_resources();

				manager->remove_thd(thd);
				Connection_handler_manager::dec_connection_count();

#ifdef HAVE_PSI_THREAD_INTERFACE
				/*
				 Delete the instrumentation for the job that just completed.
				 */
				thd->set_psi(NULL);
				PSI_THREAD_CALL(delete_current_thread)();
#endif /* HAVE_PSI_THREAD_INTERFACE */

				delete thd;
				break;
			}
		}
	}
	my_thread_end();
	my_thread_exit(0);
	return NULL;
}


Background_worker_connection_handler::Background_worker_connection_handler()
{
	my_thread_attr_t attr;
	my_thread_attr_init(&attr);
	for (ulong i = 0; i < num_workers; i++)
	{
		my_thread_handle id;
		(void) my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_DETACHED);
		mysql_thread_create(key_thread_one_connection, &id, &attr,
												process_client_requests, NULL);
	}
	my_thread_attr_destroy(&attr);
}

/**
  Construct and initialize a THD object for a new connection.

  @param channel_info  Channel_info object representing the new connection.
                       Will be destroyed by this function.

  @retval NULL   Initialization failed.
  @retval !NULL  Pointer to new THD object for the new connection.
*/

static THD* init_new_thd(Channel_info *channel_info)
{
  THD *thd= channel_info->create_thd();
  if (thd == NULL)
  {
    channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
    delete channel_info;
    return NULL;
  }

  thd->set_new_thread_id();

  thd->start_utime= thd->thr_create_utime= my_micro_time();
  delete channel_info;

  /*
    handle_one_connection() is normally the only way a thread would
    start and would always be on the very high end of the stack ,
    therefore, the thread stack always starts at the address of the
    first local variable of handle_one_connection, which is thd. We
    need to know the start of the stack so that we could check for
    stack overruns.
  */
  thd_set_thread_stack(thd, (char*) &thd); 
  if (thd->store_globals())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES);
    thd->release_resources();
    delete thd;
    return NULL;
  }

  return thd;
}

static void create_thd(Channel_info *channel_info)
{
	Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
	Connection_handler_manager *handler_manager= Connection_handler_manager::get_instance();

	if (my_thread_init())
	{
		connection_errors_internal++;
		channel_info->send_error_and_close_channel(ER_OUT_OF_RESOURCES, 0, false);
		handler_manager->inc_aborted_connects();
		Connection_handler_manager::dec_connection_count();
		delete channel_info;
		return;
	}

	THD *thd= init_new_thd(channel_info);
	if (thd == NULL)
	{
		connection_errors_internal++;
		handler_manager->inc_aborted_connects();
		Connection_handler_manager::dec_connection_count();
		delete channel_info;
		return;
	}

	if (thd_prepare_connection(thd))
	{
		handler_manager->inc_aborted_connects();
		close_connection(thd, 0, false, false);
		thd->get_stmt_da()->reset_diagnostics_area();
		thd->release_resources();
		Connection_handler_manager::dec_connection_count();
		// Clean up errors now, before possibly waiting for a new connection.
		ERR_remove_state(0);

#ifdef HAVE_PSI_THREAD_INTERFACE
		/*
		 Delete the instrumentation for the job that just completed.
		 */
		thd->set_psi(NULL);
		PSI_THREAD_CALL(delete_current_thread)();
#endif /* HAVE_PSI_THREAD_INTERFACE */

		delete thd;
	}
	else
	{
		if (thd_manager->add_thd(thd)) {
			thd_manager->put_thd(thd);
		}
	}
}

bool Background_worker_connection_handler::add_connection(Channel_info* channel_info)
{
  DBUG_ENTER("Background_worker_connection_handler::add_connection");

	create_thd(channel_info);

  DBUG_PRINT("info",("THD created"));
  DBUG_RETURN(false);
}
