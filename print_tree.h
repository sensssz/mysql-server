#ifndef print_tree_h
#define print_tree_h
#include <cstdio>
#include <vector>
#include <cstdlib>
#include "lock0lock.h"
#include "lock0priv.h"

#ifdef UNIV_NONINL
#include "lock0lock.ic"
#include "lock0priv.ic"
#endif


void print_tree(trx_t*, int);
void print_tree(lock_t*, int);

void print_tree(trx_t* trx, int depth)
{
    lock_t* lock;
    for (int i = 0; i < depth; ++i)
        fprintf(stderr, "\t");
    fprintf(stderr, "%p (%ld)\n", trx, trx->finish_time);
    for (lock = UT_LIST_GET_FIRST(trx->lock.trx_locks);
            lock != NULL;
            lock = UT_LIST_GET_NEXT(trx_locks, lock)) {
        if (lock_get_type_low(lock) == LOCK_REC
                && !lock_get_wait(lock)){
            print_tree(lock, depth + 1);
        }
    }
}

void print_tree(lock_t* in_lock, int depth)
{
    lock_t*     lock;
    ulint       space;
    ulint       page_no;
    ulint       heap_no;
    ulint       nbits;
    long        release_time;
    long        new_release_time;
    triplet     rec;
    hash_table_t*   lock_hash;
    
    space = in_lock->un_member.rec_lock.space;
    page_no = in_lock->un_member.rec_lock.page_no;
    rec.space = space;
    rec.page_no = page_no;
    lock_hash = lock_hash_get(in_lock->type_mode);
    nbits = lock_rec_get_n_bits(in_lock);
    
    for (heap_no = 0; heap_no < nbits; heap_no++) {
        if (!lock_rec_get_nth_bit(in_lock, heap_no)) {
            continue;
        }
        rec.heap_no = heap_no;
        for (int i = 0; i < depth; ++i)
            fprintf(stderr, "\t");
        fprintf(stderr, "[%ld, %ld, %ld] (%ld)\n", rec.space, rec.page_no, rec.heap_no, rec_release_time[rec]);
        for (lock = lock_rec_get_first(lock_hash, rec.space, rec.page_no, rec.heap_no);
             lock != NULL;
             lock = lock_rec_get_next(rec.heap_no, lock)) {
            if (in_lock->trx != lock->trx
                && lock_get_wait(lock)) {
                print_tree(lock_t* in_lock, depth+1);
            }
        }
    }
}



#endif
