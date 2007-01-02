/* $Id$ */

#ifndef THREAD_H
#define THREAD_H

typedef struct OTTDThread OTTDThread;

typedef void* (*OTTDThreadFunc)(void*);

OTTDThread* OTTDCreateThread(OTTDThreadFunc, void*);
void*       OTTDJoinThread(OTTDThread*);
void        OTTDExitThread(void);

#endif /* THREAD_H */
