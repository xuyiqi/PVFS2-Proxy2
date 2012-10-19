/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
 */

#ifndef SOCKET_POOL_H_
#define SOCKET_POOL_H_

struct request_state * add_request_to_socket(int index, int tag);
struct request_state * find_request(int index, int tag, int rank);
struct request_state * create_counter_rs(struct request_state * rs, int counter_index);
struct request_state * find_last_request(int index, int tag);
#endif /* SOCKET_POOL_H_ */
