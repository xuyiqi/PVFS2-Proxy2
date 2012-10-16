/*
 * socket_pool.h
 *
 *  Created on: Oct 4, 2010
 *      Author: yiqi
 */

#ifndef SOCKET_POOL_H_
#define SOCKET_POOL_H_

struct request_state * add_request_to_socket(int index, int tag);
struct request_state * find_request(int index, int tag, int rank);
struct request_state * create_counter_rs(struct request_state * rs, int counter_index);

#endif /* SOCKET_POOL_H_ */
