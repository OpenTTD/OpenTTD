/* $Id$ */

#ifndef THREAD_H
#define THREAD_H

typedef struct OTTDThread OTTDThread;

typedef void* (*OTTDThreadFunc)(void*);

OTTDThread* OTTDCreateThread(OTTDThreadFunc, void*);
void*       OTTDJoinThread(OTTDThread*);
void        OTTDExitThread();

#endif /* THREAD_H */
