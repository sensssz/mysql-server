/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file lock/lock0swap.cc
The transaction lock system

Created 5/7/1996 Heikki Tuuri
*******************************************************/

#define LOCK_MODULE_IMPLEMENTATION

#include <my_thread.h>

#include "univ.i"
#include "trx0types.h"
#include "mtr0types.h"
#include "rem0types.h"
#include "dict0types.h"
#include "que0types.h"
#include "lock0types.h"
#include "lock0lock.h"
#include "lock0swap.h"
#include "hash0hash.h"

#include <deque>

#define lock_change_sys_mutex_enter() mutex_enter(&lock_change_sys->mutex)
#define lock_change_sys_mutex_exit() mutex_exit(&lock_change_sys->mutex)

typedef std::deque<lock_sys_change_event_t> EventQueue;

struct lock_change_sys_t {
    EventQueue  event_queue;
    LockMutex   mutex;
    os_event_t  cond;
};

static lock_change_sys_t  *lock_change_sys;

static my_thread_handle swap_thread;
static bool thread_shutdown;

/*********************************************************************//**
Process lock sys change events. */
extern "C"
void*
handle_lock_sys_change_events(
    void* args);

static
void
lock_change_sys_create()
{
    lock_change_sys = static_cast<lock_change_sys_t*>(ut_zalloc_nokey(sizeof(lock_change_sys_t)));
    mutex_create(LATCH_ID_LOCK_CHANGE_SYS, &lock_change_sys->mutex);
    lock_change_sys->cond = os_event_create(0);
}

static
void
lock_change_sys_stop()
{
    os_event_destroy(lock_change_sys->cond);
    mutex_destroy(&lock_change_sys->mutex);
    lock_change_sys->event_queue.clear();
    lock_change_sys = NULL;
}

/*********************************************************************//**
Start the swap background thread. */
void
swap_thread_start()
{
    thread_shutdown = false;
    lock_change_sys_create();
    my_thread_create(&swap_thread, NULL, handle_lock_sys_change_events, NULL);
}


/*********************************************************************//**
Stop the swap background thread. */
void
swap_thread_stop()
{
    thread_shutdown = true;
    my_thread_join(&swap_thread, NULL);
    lock_change_sys_stop();
}

/*********************************************************************//**
Stop the swap background thread. */
extern "C"
void*
handle_lock_sys_change_events(
    void* args)
{
    my_thread_init();
    
    while (!thread_shutdown) {
        lock_change_sys_mutex_enter();
        while (lock_change_sys->event_queue.empty()) {
            os_event_wait(lock_change_sys->cond);
        }
        lock_sys_change_event_t event = lock_change_sys->event_queue.pop_front();
        lock_change_sys_mutex_exit();
    }
    
    return NULL;
}

/*********************************************************************//**
Submit a lock system change event. */
void
submit_lock_sys_change(
/*==============*/
    hash_table_t*   lock_hash,  /* !< The lock hash that changes. */
    ulint           space,      /* !< Space ID of the changed lock. */
    ulint           page_no,    /* !< Page number of the changed lock. */
    ulint           heap_no)    /* !< Heap number of the changed lock. */
{
    lock_sys_change_event_t event;
    event.lock_hash = lock_hash;
    event.space = space;
    event.page_no = page_no;
    event.heap_no = heap_no;
    lock_change_sys_mutex_enter();
    lock_change_sys->event_queue.push_back(std::move(event));
    if (!os_event_is_set(lock_change_sys->cond)) {
        os_event_set(lock_change_sys->cond);
    }
    lock_change_sys_mutex_exit();
}
