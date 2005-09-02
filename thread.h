/* $Id$ */

#ifndef THREAD_H
#define THREAD_H

typedef struct Thread Thread;

typedef void* (*ThreadFunc)(void*);

Thread* OTTDCreateThread(ThreadFunc, void*);
void*   OTTDJoinThread(Thread*);

#endif /* THREAD_H */
