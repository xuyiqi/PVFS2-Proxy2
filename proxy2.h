/*
 * proxy1.h
 *
 *  Created on: Mar 22, 2010
 *      Author: yiqi
 */
#include <sys/socket.h>
#include <assert.h>
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

extern char *ops[40];
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

enum mode{WRITE=0,READ=1,ACCEPT=2};
struct proxy_option
{
	int port;
	int buffer_size;
	int logging;
};

struct socket_state
{
	int socket;
	enum node source; //data's origin socket
	enum node target; //data's end socket
	int counter_part; //corresponding socket on another node
	int counter_index;
	enum mode mode; //the socket's working mode
	int config_tag; //indicating that a get_config request is identified
	int job_size;
	int completed_size;
	int ready_to_receive;
	char* ip;
	int port;
	int current_tag;
	int last_tag;
	int pvfs_io_type;
	enum PVFS_server_op op;
	int needs_output;
	unsigned long long read_size;
	unsigned long long read_offset;
	int locked;
	int check_response_completion;
	int last_completion;
	int has_block_item;
	struct generic_queue_item * current_item;
	int app_index;
	int weight;
	int deadline;
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
	char * buffer;
	int buffer_size;
	int buffer_head;
	int buffer_tail;
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
		enum mode mode, enum node target, enum node source, char* client_ip, char* server_ip, int client_port, int server_port);
void create_socket_pool(int capacity);
void increase_socket_pool();
void remove_socket(int socket);
void print_socket_pool();
int recv_all(int fd, unsigned char buff[], int* expected_size, int flag);

#endif /* PROXY1_H_ */
