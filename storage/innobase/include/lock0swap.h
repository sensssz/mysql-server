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
@file include/lock0swap.h
The transaction lock system

Created 5/7/1996 Heikki Tuuri
*******************************************************/

#ifndef lock0swap_h
#define lock0swap_h

#include <my_thread.h>

#include "univ.i"
#include "hash0hash.h"

struct lock_sys_change_event_t{
    hash_table_t*   lock_hash;
    ulint           space;
    ulint           page_no;
    ulint           heap_no;
};

/*********************************************************************//**
Start the swap background thread. */
void
swap_thread_start();


/*********************************************************************//**
Stop the swap background thread. */
void
swap_thread_stop();

/*********************************************************************//**
Submit a lock system change event. */
void
submit_lock_sys_change (
/*==============*/
    hash_table_t*   lock_hash,  /* !< The lock hash that changes */
    ulint           space,      /* !< Space ID of the changed lock. */
    ulint           page_no,    /* !< Page number of the changed lock. */
    ulint           heap_no);   /* !< Heap number of the changed lock. */
