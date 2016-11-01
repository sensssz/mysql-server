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
#include "dict0types.h"
#include "que0types.h"
#include "lock0types.h"
#include "lock0priv.h"
#include "lock0lock.h"
#include "lock0swap.h"
#include "hash0hash.h"

#include <vector>
#include <deque>

#define lock_sys_change_mutex_enter() mutex_enter(&lock_sys_change->mutex)
#define lock_sys_change_mutex_exit() mutex_exit(&lock_sys_change->mutex)

const int NUM_SWAPS = 3;

typedef std::deque<lock_sys_change_event_t> EventQueue;

struct lock_sys_change_t {
    EventQueue  event_queue;
    LockMutex   mutex;
    os_event_t  cond;
};

static
void
process_lock_sys_change_event(
    lock_sys_change_event_t event);

static
long
total_release_time();

static
void
swap_locks_if_beneficial(
    lock_sys_change_event_t event,
    lock_t* lock1,
    lock_t* lock2);

static lock_sys_change_t  *lock_sys_change;

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
lock_sys_change_create()
{
    lock_sys_change = static_cast<lock_sys_change_t*>(ut_zalloc_nokey(sizeof(lock_sys_change_t)));
    mutex_create(LATCH_ID_lock_sys_change, &lock_sys_change->mutex);
    lock_sys_change->cond = os_event_create(0);
}

static
void
lock_sys_change_stop()
{
    os_event_destroy(lock_sys_change->cond);
    mutex_destroy(&lock_sys_change->mutex);
    lock_sys_change->event_queue.clear();
    lock_sys_change = NULL;
}

/*********************************************************************//**
Start the swap background thread. */
void
swap_thread_start()
{
    thread_shutdown = false;
    lock_sys_change_create();
    my_thread_create(&swap_thread, NULL, handle_lock_sys_change_events, NULL);
}


/*********************************************************************//**
Stop the swap background thread. */
void
swap_thread_stop()
{
    thread_shutdown = true;
    my_thread_join(&swap_thread, NULL);
    lock_sys_change_stop();
}

/*********************************************************************//**
Swap thread worker. */
extern "C"
void*
handle_lock_sys_change_events(
    void* args)
{
    my_thread_init();
    
    while (!thread_shutdown) {
        lock_sys_change_mutex_enter();
        while (lock_sys_change->event_queue.empty()) {
            os_event_wait(lock_sys_change->cond);
        }
        lock_sys_change_event_t event = lock_sys_change->event_queue.pop_front();
        lock_sys_change_mutex_exit();
        process_lock_sys_change_event(event);
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
    lock_sys_change_mutex_enter();
    lock_sys_change->event_queue.push_back(std::move(event));
    if (!os_event_is_set(lock_sys_change->cond)) {
        os_event_set(lock_sys_change->cond);
    }
    lock_sys_change_mutex_exit();
}

static
lock_t *
lock_rec_get_first(
    hash_table_t*   lock_hash,
    ulint   space,
    ulint   page_no,
    ulint   heap_no)
{
    lock_t *lock = lock_rec_get_first_on_page_addr(lock_hash, space, page_no);
    if (lock != NULL && !lock_rec_get_nth_bit(lock, heap_no)) {
        lock = lock_rec_get_next(heap_no, lock);
    }
    return lock;
}


static
void
process_lock_sys_change_event(
    lock_sys_change_event_t event)
{
    int         index;
    int         num_swaps;
    lock_t*     lock;
    lock_t*     lock1;
    lock_t*     lock2;
    std::vector<lock_t*>    locks_on_rec;
    
    lock_mutex_enter();
    trx_sys_mutex_enter();
    for (lock = lock_rec_get_first(event.lock_hash, event.space, event.page_no, event.heap_no);
         lock != NULL;
         lock = lock_rec_get_next(event.heap_no, lock)) {
        locks_on_rec.push_back(lock);
    }
    index = locks_on_rec.size() - 2;
    num_swaps = 0;
    for (index >= 0) {
        if (num_swaps == NUM_SWAPS) {
            break;
        }
        lock1 = locks_on_rec[index];
        lock2 = locks_on_rec[index + 1];
        if (swap_locks_if_beneficial(event, lock1, lock2)) {
            locks_on_rec[index] = lock2;
            locks_on_rec[index + 1] = lock1;
            ++num_swaps;
        }
    }
    trx_sys_mutex_exit();
    lock_mutex_exit();
}


static
long
total_release_time()
{
    long    total_release_time;
    trx_t*  trx;
    
    ut_ad(trx_sys_mutex_own());
    
    total_release_time = 0;
    for (trx = UT_LIST_GET_FIRST(trx_sys->mysql_trx_list);
         trx != NULL;
         trx = UT_LIST_GET_NEXT(trx_list, trx)) {
        total_release_time += trx->finish_time;
    }
    
    return total_release_time;
}


static
bool
swap_locks_if_beneficial(
    lock_sys_change_event_t event,
    lock_t* lock1,
    lock_t* lock2)
{
    long        original_release_time;
    long        new_release_time;
    lock_t*     lock;
    lock_t**    lock1_prev;
    lock_t**    lock2_prev;
    lock_t*     lock1_next;
    lock_t*     lock2_next;
    ulint       rec_fold;
    hash_cell_t*    cell;

    ut_ad(lock_mutex_own());

    original_release_time = total_release_time();
    
    rec_fold = lock_rec_fold(event.space, event.page_no);
    cell = hash_get_nth_cell(event.lock_hash, hash_calc_hash(rec_fold, event.lock_hash));

    lock = lock_rec_get_first(event.lock_hash, event.space, event.page_no, event.heap_no);
    if (lock == cell->node) {
        lock1_prev = &cell->node;
    }
    
    for (; lock != NULL;
         lock = lock_rec_get_next(event.heap_no, lock)) {
        if (lock->hash == lock1) {
            lock1_prev = &lock->hash;
        }
        if (lock->hash == lock2) {
            lock2_prev = &lock->hash;
        }
    }
    lock1_next = lock1->hash;
    lock2_next = lock2->hash;
    
    if (lock1->hash == lock2) {
        *lock1_prev = lock2;
        lock1->hash = lock2_next;
        lock2->hash = lock1;
    } else {
        lock1->hash = lock2_next;
        lock2->hash = lock1_next;
        *lock1_prev = lock2;
        *lock2_prev = lock1;
    }
    
    update_trx_finish_time(lock1->trx, 1);
    update_trx_finish_time(lock2->trx, -1);
    
    new_release_time = total_release_time();
    if (new_release_time < original_release_time) {
        return true;
    }
    
    if (lock2->hash == lock1) {
        *lock1_prev = lock1;
        lock2->hash = lock2_next;
        lock1->hash = lock2;
    } else {
        lock1->hash = lock1_next;
        lock2->hash = lock2_next;
        *lock1_prev = lock1;
        *lock2_prev = lock2;
    }
    
    update_trx_finish_time(lock1->trx, -1);
    update_trx_finish_time(lock2->trx, 1);

    return false;
}

