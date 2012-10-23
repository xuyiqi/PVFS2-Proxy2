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

#ifndef SCHEDULER_MAIN_H_
#define SCHEDULER_MAIN_H_
#define SCHEDULER_COUNT 8
#define SCHEDULER_DEFAULT SFQD_SCHEDULER
extern char* chosen_scheduler;
extern int io_purecost;
#include "performance.h"
#include "dictionary.h"
#include "proxy2.h"
extern FILE* depthtrack;
struct socket_info
{

	int request_socket;//index in the socket pool
	int data_port; //internal forwarding parameter
	char* data_ip;
	int data_socket;
	int socket_tag; //internal forwarding parameter (tag used by pvfs)
	int app_index;//differentiates streams, now differentiated by ip
	int stream_id;//for identifying previously finished time.
	int task_size;
	int complete_size;
	int got_size;
	char* buffer; //request message buffer
	int io_type;
	int buffer_size;
	int unlock_index;
	int state_index;
	struct request_state* rs;

};

struct pvfs_info
{
	enum PVFS_server_op op;
	int handle;
	int mask;
	int fsid;
	int io_type;
	int tag;
	int current_data_size;
	long long aggregate_size;
	int current_server;
	int total_server;
	int req_size;
	long long req_offset;
	int strip_size;
	long long data_file_size;
};
struct complete_message
{
	struct generic_queue_item * current_item;
	int complete_size;
	void * proxy_message;
};

struct generic_queue_item
{
	long int item_id;
	struct socket_info * socket_data; //when returned, the socket information is used
	void * embedded_queue_item;//four tags, embedded here.
};

enum dequeue_event{NEW_IO, COMPLETE_IO, DIFF_CHANGE, NEW_META};
struct dequeue_reason
{
	enum dequeue_event event;
	int complete_size;
	int last_app_index;
	struct generic_queue_item * item;
};


struct scheduler_method
{
    const char *method_name;
    int work_conserving;
    int (* sch_initialize) ();
    int (* sch_finalize) ();
    int (* sch_enqueue) (struct socket_info *, struct pvfs_info*);//socket_info can be further generalized to channel_info for protocols other than TCP
    struct generic_queue_item * (* sch_dequeue) (struct dequeue_reason r);
    int (* sch_load_data_from_config) (dictionary * );
    int (* sch_update_on_request_completion)(void* arg);
    void (* sch_get_scheduler_info) ();
    int (* sch_remove_item_by_socket)(int socket);
    int (* sch_is_idle)();
    int (* sch_current_size)(struct request_state *rs, long long actual);
    int (* sch_add_ttl_throughput)(int this_amount, int app_index);
    int (* sch_calculate_diff)(int app_index);
    int sch_self_dispatch;
    int sch_accept_meta;
    //when sockets are disconnected, the unfinished items in the queue should be removed
    //- next time they'll occupy the seats in the service queue and eventually block the I/O


};
extern struct scheduler_method * static_methods[];
extern int scheduler_index;

#include "scheduler_SFQD.h"
#include "scheduler_DSFQ.h"
#include "scheduler_vDSFQ.h"
#include "scheduler_SFQD2.h"//one queue for large, io, one queue for small io
#include "scheduler_SFQD3.h"//one queue for each application for non-work conserving
#include "scheduler_vDSFQ2.h"
#include "scheduler_2LSFQD.h"
#include "scheduler_SFQD_Full.h"
#endif /* SCHEDULER_MAIN_H_ */
