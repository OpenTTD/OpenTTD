/* $Id$ */

#ifndef THREAD_H
#define THREAD_H

/*
 * DO NOT USE THREADS if you don't know what race conditions, mutexes,
 * semaphores, atomic operations, etc. are or how to properly handle them.
 * Ask somebody who has a clue.
 */

typedef struct Thread Thread;

typedef void (*ThreadFunc)(void*);

Thread* OTTDCreateThread(ThreadFunc, void*);
void    OTTDJoinThread(Thread*);

#endif
