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

#include <sys/socket.h>
#include <assert.h>
#include "llist.h"
#ifndef PROXY1_H_
#define PROXY1_H_
//#define LOGGING
#define ERROR
#include <sys/param.h>

#ifdef MSG_NOSIGNAL //socket message reading/writing mask copied from BMI/tcp
#define DEFAULT_MSG_FLAGS MSG_NOSIGNAL
#else
#define DEFAULT_MSG_FLAGS 0
#endif

//extern struct queue_item * current_item;

#define default_port "3335"
#define POLL_DELAY -1
extern int  active_port;
#define BACKLOG 256*100

extern char *ops[41];
extern char *modes[3];
extern char *encodings[3];
extern int out_buffer_size;
extern int poll_delay;
extern int scheduler_on;
extern char* dump_file;
extern int timer_stop;
extern int first_receive;
extern int counter_start;
enum PVFS_server_op
{
    PVFS_SERV_INVALID = 0,
    PVFS_SERV_CREATE = 1,
    PVFS_SERV_REMOVE = 2,
    PVFS_SERV_IO = 3,
    PVFS_SERV_GETATTR = 4,
    PVFS_SERV_SETATTR = 5,
    PVFS_SERV_LOOKUP_PATH = 6,
    PVFS_SERV_CRDIRENT = 7,
    PVFS_SERV_RMDIRENT = 8,
    PVFS_SERV_CHDIRENT = 9,
    PVFS_SERV_TRUNCATE = 10,
    PVFS_SERV_MKDIR = 11,
    PVFS_SERV_READDIR = 12,
    PVFS_SERV_GETCONFIG = 13,
    PVFS_SERV_WRITE_COMPLETION = 14,
    PVFS_SERV_FLUSH = 15,
    PVFS_SERV_MGMT_SETPARAM = 16,
    PVFS_SERV_MGMT_NOOP = 17,
    PVFS_SERV_STATFS = 18,
    PVFS_SERV_PERF_UPDATE = 19,  /* not a real protocol request */
    PVFS_SERV_MGMT_PERF_MON = 20,
    PVFS_SERV_MGMT_ITERATE_HANDLES = 21,
    PVFS_SERV_MGMT_DSPACE_INFO_LIST = 22,
    PVFS_SERV_MGMT_EVENT_MON = 23,
    PVFS_SERV_MGMT_REMOVE_OBJECT = 24,
    PVFS_SERV_MGMT_REMOVE_DIRENT = 25,
    PVFS_SERV_MGMT_GET_DIRDATA_HANDLE = 26,
    PVFS_SERV_JOB_TIMER = 27,    /* not a real protocol request */
    PVFS_SERV_PROTO_ERROR = 28,
    PVFS_SERV_GETEATTR = 29,
    PVFS_SERV_SETEATTR = 30,
    PVFS_SERV_DELEATTR = 31,
    PVFS_SERV_LISTEATTR = 32,
    PVFS_SERV_SMALL_IO = 33,
    PVFS_SERV_LISTATTR = 34,
    PVFS_SERV_BATCH_CREATE = 35,
    PVFS_SERV_BATCH_REMOVE = 36,
    PVFS_SERV_PRECREATE_POOL_REFILLER = 37, /* not a real protocol request */
    PVFS_SERV_UNSTUFF = 38,
    /* leave this entry last */
    PVFS_SERV_NUM_OPS,
    PVFS_DATA_FLOW
};

enum endian
{
	PVFS_BIG_ENDIAN = 1,
	PVFS_LITTLE_ENDIAN =2
};
enum node{SERVER=0,CLIENT=1};

//enum mode{WRITE=0,READ=1,ACCEPT=2};
#define SOCKET_WRITE	1<<1
#define SOCKET_READ		1<<2
#define SOCKET_ACCEPT	1<<3

struct proxy_option
{
	int port;
	int buffer_size;
	int logging;
};

struct request_state
{

	//these two may be obsolete because we don't have a direction now

	int config_tag; //indicating that a get_config request is identified

	int job_size;//extracted from message header
	int completed_size;//actual payload following the header received


	int current_tag;
	int last_tag;
	int pvfs_io_type;
	enum PVFS_server_op op;
	int needs_output;//obsolete

	unsigned long long read_size;
	unsigned long long read_offset;

	char * buffer;
	struct timeval receive_time;
	int buffer_size;
	int buffer_head;
	int buffer_tail;

	int locked;//obsolete
	int has_block_item;//obsolete too
	int check_response_completion; //what's the use?
	int last_completion;

	struct generic_queue_item * current_item;
	struct request_state * original_request;//for responses

	int last_flow;
	int meta_response;
};

struct socket_state
{
	//add a list to the state, saving per-request data to the elements of the list
	//rather than occupying/indicating the state of the socket

	PINT_llist_p req_state_data;//stores request_state struct

	int socket;

	int counter_socket; //corresponding socket on peer node
	int counter_socket_index; //corresponding to  socket index,

	int accept_mode;
	int incomplete_mode;//partial small message peeked, but not complete(req or resp)

	char* ip;
	int port;

	enum node source; //data's origin socket
	enum node target; //data's end socket
	int app_index;
	int weight;
	int deadline;



	//per app/ip data stays here

	struct timeval last_work_time_r;
	struct timeval last_work_time_w;
	int exp_smth_value_r;
	int exp_smth_value_w;
	int last_exp_r_app;
	int last_exp_w_app;
	int last_exp_r_machine;
	int last_exp_w_machine;
	int last_exp2_r_app;
	int last_exp2_w_app;
	int last_exp2_r_machine;
	int last_exp2_w_machine;

	struct request_state * current_send_item;
	struct request_state * current_receive_item;



};
#define IN_BUFFER_SIZE OUT_BUFFER_SIZE//only one buffer, so the buffer sizes are set to the same
#define OUT_BUFFER_SIZE 70*1024  //70KB buffer
struct socket_pool
{
	struct socket_state * socket_state_list;
	struct pollfd* poll_list;
        
    int pool_size;
	int pool_capacity;
};
extern struct socket_pool  s_pool;
int add_socket(int socket, int counter_part,
		int accept_mode, enum node target, enum node source,
		char* client_ip, char* server_ip, int client_port, int server_port);
void create_socket_pool(int capacity);
void increase_socket_pool();
void remove_socket(int socket);
void print_socket_pool();
int recv_all(int fd, unsigned char buff[], int* expected_size, int flag);

#endif /* PROXY1_H_ */
