/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_
#define BMI_HEADER_LENGTH 24
#define MESSAGE_START_THRESHOLD 10
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "sockio.h"
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "proxy2.h"
#include <poll.h>
#include "logging.h"

#include "dump.h"
#include "llist.h"
//#include "scheduler_SFQD.h"
#include <pthread.h>
#include "performance.h"
struct hashtable *h;
PINT_llist_p ip_sock;
extern char *directions[2];

extern struct socket_pool  s_pool;
extern int logging;
extern struct heap *heap_queue;
extern int passed_completions;
extern int PVFS_IO_READ;
extern int PVFS_IO_WRITE;

#endif /* MESSAGE_H_ */
