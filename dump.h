/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
 */

#ifndef DUMP_H_
#define DUMP_H_

#include "logging.h"
enum msg_type {REQUEST,RESPONSE,IO};

enum PINT_hint_type
{
    PINT_HINT_UNKNOWN = 0,
    PINT_HINT_REQUEST_ID,
    PINT_HINT_CLIENT_ID,
    PINT_HINT_HANDLE,
    PINT_HINT_OP_ID,
    PINT_HINT_RANK,
    PINT_HINT_SERVER_ID
};
struct meta
{
	unsigned long long handle;
	unsigned long long mask;
	int fsid;

};
struct dist
{
	int current_server_number;
	int total_server_number;
	unsigned long long aggregate_size;
	int this_server_size;
	int stripe_size;
	unsigned long long data_file_offset;
	char* dist_name;
	int small_total;
};

struct meta * dump_meta_header (char* buffer, enum msg_type type, char* source_ip);
struct dist  * dump_header(char* buffer, enum msg_type type, char* source);
#endif /* DUMP_H_ */
