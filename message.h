/* vPFS: Virtualized Parallel File System:
 * Performance Virtualization of Parallel File Systems

 * Copyright (C) 2009-2012 Yiqi Xu Florida International University
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See COPYING in top-level directory.
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
