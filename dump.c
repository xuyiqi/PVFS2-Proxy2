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

#include "dump.h"
#include "proxy2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
extern char* log_prefix;

/* internal attribute masks, common to all obj types */
#define PVFS_ATTR_COMMON_UID   (1 << 0)
#define PVFS_ATTR_COMMON_GID   (1 << 1)
#define PVFS_ATTR_COMMON_PERM  (1 << 2)
#define PVFS_ATTR_COMMON_ATIME (1 << 3)
#define PVFS_ATTR_COMMON_CTIME (1 << 4)
#define PVFS_ATTR_COMMON_MTIME (1 << 5)
#define PVFS_ATTR_COMMON_TYPE  (1 << 6)
#define PVFS_ATTR_COMMON_ATIME_SET (1 << 7)
#define PVFS_ATTR_COMMON_MTIME_SET (1 << 8)
#define PVFS_ATTR_COMMON_ALL                       \
(PVFS_ATTR_COMMON_UID   | PVFS_ATTR_COMMON_GID   | \
 PVFS_ATTR_COMMON_PERM  | PVFS_ATTR_COMMON_ATIME | \
 PVFS_ATTR_COMMON_CTIME | PVFS_ATTR_COMMON_MTIME | \
 PVFS_ATTR_COMMON_TYPE)

/* internal attribute masks for metadata objects */
#define PVFS_ATTR_META_DIST    (1 << 10)
#define PVFS_ATTR_META_DFILES  (1 << 11)
#define PVFS_ATTR_META_ALL \
(PVFS_ATTR_META_DIST | PVFS_ATTR_META_DFILES)

#define PVFS_ATTR_META_UNSTUFFED (1 << 12)

/* internal attribute masks for datafile objects */
#define PVFS_ATTR_DATA_SIZE            (1 << 15)
#define PVFS_ATTR_DATA_ALL   PVFS_ATTR_DATA_SIZE

/* internal attribute masks for symlink objects */
#define PVFS_ATTR_SYMLNK_TARGET            (1 << 18)
#define PVFS_ATTR_SYMLNK_ALL PVFS_ATTR_SYMLNK_TARGET

/* internal attribute masks for directory objects */
#define PVFS_ATTR_DIR_DIRENT_COUNT         (1 << 19)
#define PVFS_ATTR_DIR_HINT                  (1 << 20)
#define PVFS_ATTR_DIR_ALL \
(PVFS_ATTR_DIR_DIRENT_COUNT | PVFS_ATTR_DIR_HINT)

/* attributes that do not change once set */
#define PVFS_STATIC_ATTR_MASK \
(PVFS_ATTR_COMMON_TYPE|PVFS_ATTR_META_DIST|PVFS_ATTR_META_DFILES|PVFS_ATTR_META_UNSTUFFED)

//each function makes sure the headers are dumped in one call only

typedef enum
{
    PVFS_TYPE_NONE =              0,
    PVFS_TYPE_METAFILE =    (1 << 0),
    PVFS_TYPE_DATAFILE =    (1 << 1),
    PVFS_TYPE_DIRECTORY =   (1 << 2),
    PVFS_TYPE_SYMLINK =     (1 << 3),
    PVFS_TYPE_DIRDATA =     (1 << 4),
    PVFS_TYPE_INTERNAL =    (1 << 5)   /* for the server's private use */
} PVFS_ds_type;

enum PVFS_sys_layout_algorithm
{
    /* order the datafiles according to the server list */
    PVFS_SYS_LAYOUT_NONE = 1,

    /* choose the first datafile randomly, and then round-robin in-order */
    PVFS_SYS_LAYOUT_ROUND_ROBIN = 2,

    /* choose each datafile randomly */
    PVFS_SYS_LAYOUT_RANDOM = 3,

    /* order the datafiles based on the list specified */
    PVFS_SYS_LAYOUT_LIST = 4
};
#define PVFS_SYS_LAYOUT_DEFAULT NULL

/* permission bits */
#define PVFS_O_EXECUTE (1 << 0)
#define PVFS_O_WRITE   (1 << 1)
#define PVFS_O_READ    (1 << 2)
#define PVFS_G_EXECUTE (1 << 3)
#define PVFS_G_WRITE   (1 << 4)
#define PVFS_G_READ    (1 << 5)
#define PVFS_U_EXECUTE (1 << 6)
#define PVFS_U_WRITE   (1 << 7)
#define PVFS_U_READ    (1 << 8)
/* no PVFS_U_VTX (sticky bit) */
#define PVFS_G_SGID    (1 << 10)
#define PVFS_U_SUID    (1 << 11)

/* valid permission mask */
#define PVFS_PERM_VALID \
(PVFS_O_EXECUTE | PVFS_O_WRITE | PVFS_O_READ | PVFS_G_EXECUTE | \
 PVFS_G_WRITE | PVFS_G_READ | PVFS_U_EXECUTE | PVFS_U_WRITE | \
 PVFS_U_READ | PVFS_G_SGID | PVFS_U_SUID)

#define PVFS_USER_ALL  (PVFS_U_EXECUTE|PVFS_U_WRITE|PVFS_U_READ)
#define PVFS_GROUP_ALL (PVFS_G_EXECUTE|PVFS_G_WRITE|PVFS_G_READ)
#define PVFS_OTHER_ALL (PVFS_O_EXECUTE|PVFS_O_WRITE|PVFS_O_READ)

#define PVFS_ALL_EXECUTE (PVFS_U_EXECUTE|PVFS_G_EXECUTE|PVFS_O_EXECUTE)
#define PVFS_ALL_WRITE   (PVFS_U_WRITE|PVFS_G_WRITE|PVFS_O_WRITE)
#define PVFS_ALL_READ    (PVFS_U_READ|PVFS_G_READ|PVFS_O_READ)


void dump_IO(char* buffer, int size)//data flow
{
	//got an IO buffer of size /size/
	//Dprintf(D_CALL,"size:%lli\n", size);
}
int dump_IO_Request2(char* buffer, int offset, int small)
{
	int length;//=8;//target_datafile_handle

	//long long target_data_handle = output_param(buffer, offset, length , "target_data_handle",NULL,0);
	//Dprintf(D_CALL,"target_data_handle:%lli\n", target_data_handle);
	//offset+=length;
	//length=4;//fs_id

	//long long fs_id = output_param(buffer, offset, length , "fs_id",NULL,0);
	//Dprintf(D_CALL,"fs_id:%lli\n", fs_id);
	//offset+=length;
	//length=4;//padding
	//offset+=length;
	length=4;//io_type
	if (small==0)
	{
		offset+=16;//for efficiency
	}
	else
	{
		offset+=12;
	}

	long long io_type = output_param(buffer, offset, length , "io_type",NULL,0);
	return io_type;
}
struct dist* dump_IO_Request(unsigned char* buffer, int IO_size, int offset, int small)
{

	int length;//=8;//target_datafile_handle

	struct dist * dist = (struct dist*) malloc(sizeof(struct dist));

	//long long target_data_handle = output_param(buffer, offset, length , "target_data_handle",NULL,0);
	//Dprintf(D_CALL,"target_data_handle:%lli\n", target_data_handle);
	//offset+=length;
	//length=4;//fs_id

	//long long fs_id = output_param(buffer, offset, length , "fs_id",NULL,0);
	//Dprintf(D_CALL,"fs_id:%lli\n", fs_id);
	//offset+=length;
	//length=4;//padding
	//offset+=length;
	//length=4;//io_type
	//long long io_type = output_param(buffer, offset, length , "io_type",NULL,0);
	//Dprintf(D_CALL,"io_type:%lli\n", io_type);
	//offset+=length;
	//length=4;//flow_type
	//long long flow_type = output_param(buffer, offset, length , "flow_type",NULL,0);
	//Dprintf(D_CALL,"flow_type:%lli\n", flow_type);
	//offset+=length;
	if (small==1)
	{
		offset+=16;
	}
	else
	{
		offset+=24;//for efficiency

	}
	length=4;//relative_server_num
	long long relative_server_num = output_param(buffer, offset, length , "relative_server_num",NULL,0);

	dist->current_server_number=relative_server_num;

	//Dprintf(D_CALL,"relative_server_num:%lli\n", relative_server_num);
	offset+=length;
	length=4;//total_server_num
	long long total_server_num = output_param(buffer, offset, length , "total_server_num",NULL,0);

	dist->total_server_number=total_server_num;

	//Dprintf(D_CALL,"total_server_num:%lli\n", total_server_num);
	offset+=length;
	length=4;//string length
	int string_length = output_param(buffer, offset, length , "string length",NULL,0);
	//Dprintf(D_CALL,"string length:%i\n", string_length);
	offset+=length;
	//find source code
	length=string_length;//dist_name
	char * dist_name =(char*)malloc(length+1);
	memcpy(dist_name, buffer+offset ,length);
	//hexdump(buffer+offset,length,D_CALL);

	int total_length=string_length+4;
	dist_name[length]='\0';
	int mul=total_length-total_length/8*8;
	int p1=0;
	if (mul!=0)
	{
		p1=8-mul;
	}

	offset+=length;
	offset+=p1;//string padding reserved?
	//Dprintf(D_CALL,"padding %i after dist", p1);
	int padding=0;
	//Dprintf(D_CALL,"dist:%s\n", dist_name);
	dist->dist_name=dist_name;
	if (!strcmp(dist_name, "basic_dist"))
	{
		//none
	} else if (!strcmp(dist_name, "varstrip_dist"))
	{
		length=strlen(buffer+offset);

		char * variable_stripe =(char*)malloc(length);
		memcpy(variable_stripe, buffer+offset ,length);

		padding=8-length%8;
		padding %=8;
		offset+=length;

		free(variable_stripe);

	} else if (!strcmp(dist_name, "simple_stripe"))
	{
		length=8;
		long long stripe_size = output_param(buffer, offset, length , "stripe_size",NULL,0);
		dist->stripe_size=stripe_size;
		//Dprintf(D_CALL,"stripe_size:%lli\n", stripe_size);
		offset+=length;
	} else if (!strcmp(dist_name, "twod_stripe"))
	{
		length=4;
		long long num_of_groups = output_param(buffer, offset, length , "num_of_groups",NULL,0);
		//Dprintf(D_CALL,"num_groups:%lli\n", num_of_groups);
		offset+=length;
		length=8;
		long long stripe_size = output_param(buffer, offset, length , "stripe_size",NULL,0);
		//Dprintf(D_CALL,"stripe_size:%lli\n", stripe_size);
		offset+=length;
		padding=4;
	}
	else
	{
		fprintf(stderr, "dist_name cannot be recognized:%s\n", dist_name);
		exit(-1);
	}
	offset+=padding;
	length=4;//num_nested_req
	long long num_nested_req = output_param(buffer, offset, length , "num_nested_req",NULL,0);
	//fprintf(stderr,"nested_req:%lli\n", num_nested_req);
	offset+=length;
	offset+=4;//padding
	int i;



	//long long internal_offset = output_param(buffer, offset, 8 , "offset",NULL,0);
	//long long internal_offset2 = output_param(buffer, offset+80, 8 , "offset",NULL,0);

	//fprintf(stderr,"    offset:%lli,%lli\n", internal_offset,internal_offset2);

	//offset+=((num_nested_req+1)*80);
	for (i=0;i<num_nested_req+1;i++)
	{
		///io/desc/pint-req-encode.h 104, 56
		//proto/pvfs2-req-proto.h

		length=8;
		long long internal_offset = output_param(buffer, offset, length , "offset",NULL,0);
		fprintf(stderr,"    offset:%lli\n", internal_offset);
		offset+=length;
		length=4;
		long long num_ereqs = output_param(buffer, offset, length , "num_ereqs",NULL,0);
		fprintf(stderr,"    ereqs:%lli\n", num_ereqs);
		offset+=length;
		length=4;
		long long num_blocks = output_param(buffer, offset, length , "num_blocks",NULL,0);
		fprintf(stderr,"    blocks:%lli\n", num_blocks);
		offset+=length;
		length=8;
		long long stride = output_param(buffer, offset, length , "stride",NULL,0);
		fprintf(stderr,"    stride:%lli\n", stride);
		offset+=length;
		length=8;
		long long upper_bound = output_param(buffer, offset, length , "upper_bound",NULL,0);
		fprintf(stderr,"    upper_bound:%lli\n", upper_bound);
		offset+=length;
		length=8;
		long long lower_bound = output_param(buffer, offset, length , "lower_bound",NULL,0);
		fprintf(stderr,"    lower_bound:%lli\n", lower_bound);
		offset+=length;
		length=8;
		long long aggregate_size = output_param(buffer, offset, length , "aggregate_size",NULL,0);
		fprintf(stderr,"    aggregate_size:%lli\n", aggregate_size);
		offset+=length;
		length=4;
		long long num_contig_chunks = output_param(buffer, offset, length , "num_contig_chunks",NULL,0);
		fprintf(stderr,"    contig_chunks:%lli\n", num_contig_chunks);
		offset+=length;
		length=4;
		long long depth = output_param(buffer, offset, length , "depth",NULL,0);
		fprintf(stderr,"    depth:%lli\n", depth);
		offset+=length;
		length=4;
		long long num_nested_req = output_param(buffer, offset, length , "num_nested_req",NULL,0);
		fprintf(stderr,"    num_nested_req:%lli\n", num_nested_req);
		offset+=length;
		length=4;

		long long committed = output_param(buffer, offset, length , "committed",NULL,0);
		fprintf(stderr,"    committed:%lli\n", committed);
		offset+=length;
		length=4;

		long long ref_count = output_param(buffer, offset, length , "ref_count",NULL,0);
		fprintf(stderr,"    ref_count:%lli\n", ref_count);
		offset+=length;
		offset+=4;//padding
		length=4;

		long long ereq = output_param(buffer, offset, length , "ereq",NULL,0);
		fprintf(stderr,"    ereq:%lli\n", ereq);
		offset+=length;
		length=4;

		long long sreq = output_param(buffer, offset, length , "sreq",NULL,0);
		fprintf(stderr,"    sreq:%lli\n", sreq);
		offset+=length;

	}


	length=8;

	long long file_req_offset = output_param(buffer, offset, length , "file_req_offset",NULL,0);

	file_req_offset=*( (long long *) (buffer+offset));

	offset+=length;
	length=8;

	unsigned long long aggregate_size = output_param(buffer, offset, length , "aggregate_size",NULL,0);
	//Dprintf(D_CALL,"aggregate_size:%lli\n", aggregate_size);
	//fprintf(stderr,"%s file offset: %lli, size, %lli\n",log_prefix,file_req_offset, aggregate_size);

	dist->aggregate_size=aggregate_size;
	dist->data_file_offset=file_req_offset;

	if (small==1)
	{
		offset+=length;
		length=8;
		long long total_bytes = output_param(buffer, offset, length , "aggregate_size",NULL,0);
		dist->small_total=total_bytes;
	}

	return dist;


}

void dump_IO_Response(char* buffer, int length, int offset)
{
	//bstream size of /8/
	//long long complete_size=output_param(buffer, offset, 8 , "IO_Response",NULL,0);
	//Dprintf(D_CALL,"response:%lli\n", complete_size);

}

long long dump_Write_Completion(char* buffer, int length, int offset)
{
	//got a completion of size buffer (8)
	long long complete_size=output_param(buffer, offset, 8 , "Complete_size",NULL,0);
	//Dprintf(D_CALL,"completion:%lli\n", complete_size);
	return complete_size;
}


long long dump_SMALL_IO_Completion(char* buffer, int length, int offset)
{
	//got a completion of size buffer (8)
	offset+=16;//iotype=4,pad=4,bstreamsize=8
	long long complete_size=output_param(buffer, offset, 8 , "Complete_size",NULL,0);
	//fprintf(stderr,"completion of small io:%lli\n", complete_size);
	return complete_size;
}

void dump_address (char* from, char* to)
{
	//Dprintf(D_CALL,"From: %s; To: %s\n", from, to);
}

/*this function actually executes a get_io_type task*/

int dump_header2(char* buffer,int small)
{

	int offset=0;
	int length=4;//mnr
	long long mnr;

/*	offset+=length;
	length=4;//mode
	offset+=length;
	length=8;//tag

	offset+=length;
	length=8;//size

	offset+=length;
	length=4;//version

	offset+=length;
	length=4;//encoding

	offset+=length;
	length=4;//operation


	offset+=length;
	length=4;//padding 4

	offset+=length;
	length=4;//userid

	offset+=length;
	length=4;//credentials 8,user 4, group 4

	offset+=length;*/

	offset+=48;//for efficiency
	length=4;//hint_count 4
	long long hint_count = output_param(buffer, offset, length, "hint count", NULL,0);
	//Dprintf(D_CALL,"hint_count at offset %i:%lli\n", offset,hint_count);
	int i=0;
	for (i=0;i<hint_count;i++)
	{
		offset+=length;
		length=4;//hint_type 4
		long long hint_type = output_param(buffer, offset, length, "hint type", NULL,0);
		//Dprintf(D_CALL,"hint_type:%lli\n", hint_type);
		if (hint_type==PINT_HINT_HANDLE)
		{
			offset+=length;
			length=8;//hint_length 8

			//long long hint_handle = output_param(buffer, offset, length, "hint handle", NULL,0);
			//Dprintf(D_CALL,"handle:%lli\n", hint_handle);
		}
		else
		{
			//src/common/misc/pint-hint.c, line 36
			offset+=length;
			length=4;//hint_length 4

			//long long hint_other = output_param(buffer, offset, length, "hint other", NULL,0);
			//Dprintf(D_CALL,"hint_other:%lli\n", hint_other);
		}
		//hint type 4
		//hint depending on the hint type (if ==handle[3]), handle[8]
	}

	offset+=length;
	//Dprintf(D_CALL, "pvfs header done, offset is %i\n",offset);
	return dump_IO_Request2(buffer, offset,small);//hints, distribution, etc

}

void dump_string(char* buffer, int *offset)
{
	int length=4;
	int s_size = output_param(buffer, *offset, length, "string size", NULL, 0);
	*offset+=length;
	char* temp_string = (char*)malloc(s_size+1);
	temp_string[s_size]=0;
	memcpy(temp_string, buffer+*offset, s_size);
	//fprintf(stderr,"STRING LENGTH IS %i:\n'%s'\n",s_size, temp_string);
	*offset+=s_size;
	int left_over = (length + s_size) % 8;
	int padding = 0;
	if ( left_over > 0)
	{ padding = 8 - left_over;}
	//fprintf(stderr,"PADDING is %i bytes long\n", padding);
	*offset+=padding;
	free(temp_string);

}
struct attr_plus
{
	int create_ds_type;
	int attr_mask;
};

struct attr_plus dump_attribute(char* buffer, int* offset)
{
	int length;
	length=4;
	int create_uid = output_param(buffer, *offset, length, "create uid", NULL,0);
	*offset+=length;
	//fprintf(stderr,"create uid: %i\n", create_uid);
	length=4;
	int create_gid = output_param(buffer, *offset, length, "create gid", NULL,0);
	*offset+=length;
	//fprintf(stderr,"create gid: %i\n", create_gid);
	length=4;
	int create_permissions = output_param(buffer, *offset, length, "create permissions", NULL,0);
	*offset+=(length+4);//padding
	//fprintf(stderr,"create permissions: %i\n", create_permissions);
	length=8;
	long long create_atime = output_param(buffer, *offset, length, "create atime", NULL,0);
	*offset+=length;
	length=8;
	long long create_mtime = output_param(buffer, *offset, length, "create mtime", NULL,0);
	*offset+=length;
	length=8;
	long long create_ctime = output_param(buffer, *offset, length, "create ctime", NULL,0);
	*offset+=length;
	//fprintf(stderr,"create atime, mtime, ctime: %lli, %lli, %lli\n",
		//	create_atime, create_mtime, create_ctime);

	length=4;
	int create_attr_mask = output_param(buffer, *offset, length, "create attr mask", NULL,0);
	*offset+=length;
	fprintf(stderr,"create attr mask: %X\n", create_attr_mask);

	length=4;
	int create_ds_type = output_param(buffer, *offset, length, "create ds type", NULL,0);
	*offset+=length;

	fprintf(stderr,"create ds type: %i\n", create_ds_type);
	struct attr_plus ret;
	ret.create_ds_type=create_ds_type;
	ret.attr_mask=create_attr_mask;
	return ret;
}

void dump_attribute_plus(char* buffer, int* offset, struct attr_plus plus)
{

	int length;
	int create_ds_type = plus.create_ds_type;
	int create_attr_mask = plus.attr_mask;
	//fprintf(stderr,"! %X & %X == %X \n", create_attr_mask, PVFS_ATTR_META_UNSTUFFED, !(create_attr_mask & PVFS_ATTR_META_UNSTUFFED));
	//fprintf(stderr,"create_ds_type = %i\n", create_ds_type);
	/*
	//additional attributes depends on the following conditions:*/
	if ( create_ds_type == PVFS_TYPE_METAFILE && ! (create_attr_mask & PVFS_ATTR_META_UNSTUFFED))
	{
		length=4;
		int create_stuffed_size = output_param(buffer, *offset, length, "stuffed size", NULL,0);
		*offset+=(length+4);//padding
		//fprintf(stderr,"stuffed size is %lli\n", create_stuffed_size);
	}
	if ( create_attr_mask & PVFS_ATTR_META_DIST)
	{
		//fprintf(stderr,"dumping meta_dist string\n");
		dump_string(buffer, offset);
		length=8;

		long long create_stripe_size = output_param(buffer, *offset, length, "stripe size", NULL,0);
		//fprintf(stderr,"stripe size %lli\n", create_stripe_size);
		*offset+=length;
	}
	if ( create_attr_mask & PVFS_ATTR_META_DFILES )
	{
		length=4;
		int create_dfile_count = output_param(buffer, *offset, length, "dfile count", NULL,0);
		*offset+=(length+4);//padding
		//fprintf(stderr,"create dfile count is %i \n", create_dfile_count);
		int j;
		for (j=0; j< create_dfile_count; j++)
		{
			length=8;
			long long create_handle = output_param(buffer, *offset, length, "handle", NULL,0);
			//fprintf(stderr, "handle %lli\n", create_handle);
			*offset+=length;
		}
		length=8;
		long long create_hint = output_param(buffer, *offset, length, "hint", NULL,0);
		*offset+=length;
		//fprintf(stderr,"create hint %lli\n", create_hint);
	}
	if ( create_attr_mask & PVFS_ATTR_DATA_SIZE)
	{
		length=8;
		long long create_data_size = output_param(buffer, *offset, length, "data size", NULL,0);
		*offset+=length;
		//fprintf(stderr,"create data size %lli\n", create_data_size);

	}
	if ( create_attr_mask & PVFS_ATTR_SYMLNK_TARGET)
	{
		length=4;
		long long create_path_length = output_param(buffer, *offset, length, "path length", NULL,0);
		*offset+=(length+4);//padding
		//fprintf(stderr,"path length %lli\n", create_path_length);
		//fprintf(stderr,"dumping symlink string\n");
		dump_string(buffer, offset);

	}
	if ( create_attr_mask &
			(PVFS_ATTR_DIR_DIRENT_COUNT |   PVFS_ATTR_DIR_HINT) )
	{
		length=8;
		long long create_dirent_count = output_param(buffer, *offset, length, "dirent count", NULL,0);
		*offset+=length;
		//fprintf(stderr,"dirent_count %lli\n", create_dirent_count);
		length=4;
		int create_dist_name_len = output_param(buffer, *offset, length, "dist name len", NULL,0);
		*offset+=(length+4);//padding
		//fprintf(stderr,"name len %lli\n", create_dist_name_len);
		//fprintf(stderr,"dumping dirent string1\n");
		dump_string(buffer, offset);
		length=4;
		int create_dist_param_len = output_param(buffer, *offset, length, "dist param len", NULL,0);
		*offset+=(length+4);//padding
		//fprintf(stderr,"dumping dirent string2\n");
		dump_string(buffer, offset);
		length=4;
		int create_dfile_count = output_param(buffer, *offset, length, "data file count", NULL,0);
		*offset+=length;

	}



}

struct meta* dump_meta_header(char* buffer, enum msg_type type, char* source)
{
	//Dprintf(D_CALL,"=======source:%s======\n", source );
	int offset=0;
	int length;/*//=4;//mnr
	//long long mnr;

	//mnr=output_param(buffer, offset, length , "magic number",NULL,0);
	//Dprintf(D_CALL,"mnr:%lli\n", mnr);
	offset+=length;
	length=4;//mode

	//long long mode=output_param(buffer, offset, length , "mode",NULL,0);
	//Dprintf(D_CALL,"mode:%lli\n", mode);
	offset+=length;
	length=8;//tag

	//long long tag =output_param(buffer, offset, length , "tag",NULL,0);
	//Dprintf(D_CALL,"tag:%lli\n", tag);
	offset+=length;*/
	length=8;//size

	offset+=16;

	long long size=output_param(buffer, offset, length , "size",NULL,0);

	offset+=length;
	length=4;//version

	//long long version=output_param(buffer, offset, length , "version",NULL,0);
	//Dprintf(D_CALL,"version:%lli\n", version);
	offset+=length;
	length=4;//encoding

	//long long encoding=output_param(buffer, offset, length , "encoding",NULL,0);
	//Dprintf(D_CALL,"encoding:%lli\n", encoding);
	offset+=length;
	length=4;//operation

	long long operation = output_param(buffer, offset, length, "pvfs_operation", ops,40);
	//Dprintf(D_CALL,"operation:%lli\n", operation);

	offset+=length;
/*

	length=4;//padding 4
	offset+=length;
	length=4;
	//long long user_id = output_param(buffer, offset, length, "user id", NULL,0);
	offset+=length;
	length=4;//credentials 8,user 4, group 4
	//long long group_id = output_param(buffer, offset, length, "group id", NULL,0);
	//Dprintf(D_CALL,"group_id:%lli\n", group_id);
	offset+=length;*/
	offset+=12;
	length=4;//hint_count 4
	long long hint_count = output_param(buffer, offset, length, "hint count", NULL,0);
	//fprintf(stderr,"meta hint_count:%lli\n", hint_count);
	int i=0;
	for (i=0;i<hint_count;i++)
	{
		offset+=length;
		length=4;//hint_type 4
		long long hint_type = output_param(buffer, offset, length, "hint type", NULL,0);
		//fprintf(stderr,"meta hint_type:%lli\n", hint_type);
		switch (hint_type)
		{
		case PINT_HINT_HANDLE:
			offset+=length;
			length=8;//hint_length 8
			long long hint_handle = output_param(buffer, offset, length, "hint handle", NULL,0);
			//fprintf(stderr,"meta hint_handle:%lli\n", hint_handle);
			break;
		case PINT_HINT_UNKNOWN:
		case PINT_HINT_REQUEST_ID:
		case PINT_HINT_CLIENT_ID:

		case PINT_HINT_OP_ID:
		case PINT_HINT_RANK:
		case PINT_HINT_SERVER_ID:
		default:
			//pvfs2/src/common/misc/pint-hint.c, line 36
			offset+=length;
			length=4;//hint_length 4
			long long hint_other = output_param(buffer, offset, length, "hint other", NULL,0);
			//fprintf(stderr,"meta hint_other:%lli\n", hint_other);
			break;
		//hint type 4
		//hint depending on the hint type (if ==handle[3]), handle[8]
		}
	}

	offset+=length;

	struct meta * meta = (struct meta *)malloc(sizeof (struct meta));
	memset(meta, 0, sizeof (struct meta));
	switch (operation){
	case PVFS_SERV_GETATTR:
		length=8;
		long long attr_handle = output_param(buffer, offset, length, "attr handle", NULL,0);
		offset+=length;
		length=4;
		int attr_fs_id = output_param(buffer, offset, length, "fs id", NULL,0);
		meta->handle=attr_handle;
		offset+=length;
		length=4;
		int attr_mask = output_param(buffer, offset, length, "attr mask", NULL,0);
		meta->mask = attr_mask;
		//fprintf(stderr, "HANDLE %#08llX, FS ID %#08lX, MASK %#08lX\n", attr_handle, attr_fs_id, attr_mask);
		break;
		//handle[8]
		//fs_id[4]
		//attribute_mask[4]

	case PVFS_SERV_READDIR:
		length=8;
		long long dir_handle = output_param(buffer, offset, length, "dir handle", NULL,0);
		offset+=length;
		length=4;
		int dir_fs_id = output_param(buffer, offset, length, "fs id", NULL,0);
		offset+=length;
		length=4;
		int dir_entry_count = output_param(buffer, offset, length, "dir entry count", NULL,0);
		offset+=length;
		length=8;
		int ds_position = output_param(buffer, offset, length, "ds position", NULL,0);
		//fprintf(stderr, "HANDLE %#08llX, FS ID %#08lX, ENTRY COUNT %#08lX, DS POSITION %#08lX\n",
			//	dir_handle, dir_fs_id, dir_entry_count, ds_position);

		//dir_handle[8]
		//fs_id[4]
		//dir_entry_count[4]
		//ds_position[8]
		//response: ds_position[8] dir_version[8], padding[4], dir_entry_count[4], dir_entry_array[8*] (name[8*], handle[8])
		break;
	case PVFS_SERV_LISTATTR:
		length=4;
		long long listattr_fs_id = output_param(buffer, offset, length, "fs id", NULL,0);
		offset+=length;
		length=4;
		int listattr_attr_mask = output_param(buffer, offset, length, "attribute mask", NULL,0);
		offset+=length;
		offset+=4;//padding
		length=4;
		int listattr_handle_num = output_param(buffer, offset, length, "handle num", NULL,0);
		offset+=length;
		//fprintf(stderr, "FS ID %#08lX, ATTR MASK %#08lX, # HANDLES %i\n",
			//	listattr_fs_id, listattr_attr_mask, listattr_handle_num);

		int i;
		//fprintf(stderr,"HANDLES:");
		for (i =0; i< listattr_handle_num; i++)
		{
			length=8;
			int handle = output_param(buffer, offset, length, "handle", NULL,0);
			//fprintf(stderr, "%#08llX ", handle);
			offset+=length;
		}
		//fprintf(stderr,"\n");


		/*
		fs_id[4]
		Attribute_mask[4]
		Padding[4]
		handle_num[4]
		handles[8*]

		response:
		padding[4]
		object_num[4]
		PVFS_errors[4*]
		The error code array for all the target objects. If there is an error when querying the attribute, the error code is non-zero, otherwise it is zero.
		Attributes[(48+)*]
		 */
		break;
	case PVFS_SERV_CREATE:

		length=4;
		int create_fs_id = output_param(buffer, offset, length, "create fs id", NULL,0);
		offset+=(length+4);//padding

		//fprintf(stderr,"fs id: %i\n",create_fs_id);
		//attributes

		struct attr_plus ap = dump_attribute(buffer, &offset);
		dump_attribute_plus(buffer, &offset, ap);
		length=4;
		int create_num_dfiles_req = output_param(buffer, offset, length, "num dfiles req", NULL,0);
		offset+=length;
		//fprintf(stderr,"ndfiles req %i\n", create_num_dfiles_req);
		length=4;
		int create_layout_algo = output_param(buffer, offset, length, "layout algo", NULL,0);
		offset+=(length+4);
		//fprintf(stderr,"layout algo %i\n", create_layout_algo);
		length=4;
		int create_server_list_count = output_param(buffer, offset, length, "server list", NULL,0);
		offset+=(length+4);
		//fprintf(stderr,"server list count %i\n", create_server_list_count);
		if (create_layout_algo == PVFS_SYS_LAYOUT_LIST)
		{
			//fprintf(stderr, "dumping layout string\n");
			dump_string(buffer, &offset);
		}

/*
		response:**********************************
		Metafile_handle[8]
		Value
			8-byte integer
		Meaning
			The metafile handle for the created file.
		padding[4]
		datafile_count[4]
		Value
			integer
		Meaning
			The number of datafiles.
		datafile_handles[8*]
		Value
			8-byte integers
		Meaning
		The array of datafile handles for the created file.
		 */
		break;
	case PVFS_SERV_STATFS:
		length=4;
		int statfs_fs_id = output_param(buffer, offset, length, "statfs fs id", NULL,0);
		offset+=(length+4);//padding

		//fs_id[4]

		/* response: PVFS_statfs[88]
		Padding[4]
		fs_id[4]
		bytes_available[8]
		bytes_total[8]
		ram_total_bytes[8]
		ram_free_bytes[8]
		Load_1[8]
		Load_5[8]
		Load_15[8]
		uptime_seconds[8]
		handles_available_count[8]
		handles_total_count[8]
		 	*/
		break;
	case PVFS_SERV_CRDIRENT:
		dump_string(buffer, &offset);
		length=8;

		long long new_handle = output_param(buffer, offset, length, "new handle", NULL,0);
		offset+=length;
		length=8;
		long long crd_parent_dir_handle = output_param(buffer, offset, length, "parent_dir_handle", NULL,0);
		offset+=length;
		length=4;
		int crd_fs_id = output_param(buffer, offset, length, "fs id", NULL,0);

		//fprintf(stderr, "NEW HANDLE %#08lX, PARENT DIR HANDLE %#08lX, FS ID %#08lX\n",
		//		new_handle, crd_parent_dir_handle, crd_fs_id);
		//name[8*]
		//new_handle[8]
		//parent_dir_handle[8]
		//fs_id[4]
		break;
	case PVFS_SERV_RMDIRENT:
		dump_string(buffer, &offset);
		length=8;
		long long rmd_parent_dir_handle = output_param(buffer, offset, length, "parent_dir_handle", NULL,0);
		offset+=length;
		length=4;
		int rmd_fs_id = output_param(buffer, offset, length, "fs id", NULL,0);

		//fprintf(stderr, "PARENT DIR HANDLE %#08lX, FS ID %#08lX\n",
		//		rmd_parent_dir_handle, rmd_fs_id);
		//string_entry[8*]
		//parent_dir_handle[8]
		//fs_id[4]
		break;
	case PVFS_SERV_LOOKUP_PATH:
		//fprintf(stderr,"dumping lookup path string\n");
		dump_string(buffer, &offset);
		length=4;
		int lookup_fs_id = output_param(buffer,
				offset, length, "lookup fs id", NULL,0);
		offset+=(length+4);
		length=8;
		long long lookup_parent_dir_handle = output_param(buffer,
				offset, length, "lookup parent dir handle", NULL,0);
		offset+=length;
		length=4;
		int lookup_attr_mask = output_param(buffer,
				offset, length, "look up attr mask", NULL,0);
		//file_name[8*]
		//fd_id[4]
		//padding[4]
		//parent_dir_handle[8]
		//attribute_mask[4]

		//response is more complicated
		break;
	case PVFS_SERV_MKDIR:
		length=4;
		int mkdir_fs_id = output_param(buffer,
				offset, length, "mkdir fs id", NULL,0);
		//fprintf(stderr, "mkdir fs id %i\n", mkdir_fs_id);
		offset+=(length+4);//padding
		struct attr_plus p = dump_attribute(buffer,&offset);
		dump_attribute_plus(buffer, &offset, p);

		offset+=4;//padding
		length=4;
		int mkdir_extent_count = output_param(buffer,
				offset, length, "mkdir extent count", NULL,0);
		offset+=length;
		//fprintf(stderr,"mkdir extent count %i\n",mkdir_extent_count);
		/*int k;
		for (k=0;k<mkdir_extent_count;k++)
		{
			length=8;
			long long mkdir_start = output_param(buffer,
					offset, length, "mkdir extent start", NULL,0);
			//fprintf(stderr,"mkdir extent start %lli, ",mkdir_start);
			offset+=length;
			length=8;
			long long mkdir_end = output_param(buffer,
					offset, length, "mkdir extent end", NULL,0);
			offset+=length;
			//fprintf(stderr, "end %lli\n", mkdir_end);
		}*/

		//fs_id[4]
		//padding[4]
		//attribute[48+]
		//call a function
		//handle_extent_array[8+16*]

		break;
	case PVFS_SERV_GETCONFIG:
		//resp: fs_config_buf_size[4] padding[4] fs_colnfig_buf[8*]
		break;
	//case PVFS_SERV_BATCH_CREATE:
		/*
		fs_id[4]
		object_type[4]
		Value <pvfs2-types.h>
			enum:
			NONE		0
			METAFILE	1<<0
			DATAFILE	1<<1
			DIRECTORY	1<<2
			SYMLINK	1<<3
			DIRDATA	1<<4
			INTERNAL	1<<5
		Meaning
			The type of the objects to be created.
		obj_count[4]
		Value
			integer
		Meaning
			The amount of objects to be created.
		Padding[4]
		handle_extent_array[8+16*]
		Please refer to PVFS_SERV_MKDIR (11) :: handle_extent_array.

		response *******************

		Padding[4]
		obj_count[4]
		handles[8*]
		*/
	//case PVFS_SERV_BATCH_REMOVE:
	case PVFS_SERV_REMOVE:
		//handle[8], fs_id[4]
		length=8;
		long long remove_handle = output_param(buffer, offset, length, "remove handle", NULL,0);
		offset+=length;
		length=4;
		int remove_fs_id = output_param(buffer, offset, length, "fs id", NULL,0);
		meta->handle = remove_handle;
		meta->fsid = remove_fs_id;
		//fprintf(stderr, "HANDLE %#08llX, FS ID %#08lX\n", remove_handle, remove_fs_id);
		break;

	default:
		fprintf(stderr, "Error, not supported operation in scheduler %i, %s\n", operation, ops[operation]);
		exit(-1);
		break;

	}

	return meta;

}

struct dist* dump_header(char* buffer, enum msg_type type, char* source)
{


	//Dprintf(D_CALL,"=======source:%s======\n", source );
	int offset=0;
	int length;/*//=4;//mnr
	//long long mnr;

	//mnr=output_param(buffer, offset, length , "magic number",NULL,0);
	//Dprintf(D_CALL,"mnr:%lli\n", mnr);
	offset+=length;
	length=4;//mode

	//long long mode=output_param(buffer, offset, length , "mode",NULL,0);
	//Dprintf(D_CALL,"mode:%lli\n", mode);
	offset+=length;
	length=8;//tag

	//long long tag =output_param(buffer, offset, length , "tag",NULL,0);
	//Dprintf(D_CALL,"tag:%lli\n", tag);
	offset+=length;*/
	length=8;//size

	offset+=16;

	long long size=output_param(buffer, offset, length , "size",NULL,0);
	//Dprintf(D_CALL,"size:%lli\n", size);
	offset+=length;
	length=4;//version

	//fprintf(stderr,"type:%i\n",type);

	if (type==IO)
	{
	//	if (mnr==0xcabf)
		{
				//dump_IO(buffer, size);
				return NULL;
		}
	}

	//long long version=output_param(buffer, offset, length , "version",NULL,0);
	//Dprintf(D_CALL,"version:%lli\n", version);
	offset+=length;
	length=4;//encoding

	//long long encoding=output_param(buffer, offset, length , "encoding",NULL,0);
	//Dprintf(D_CALL,"encoding:%lli\n", encoding);
	offset+=length;
	length=4;//operation

	long long operation = output_param(buffer, offset, length, "pvfs_operation", ops,40);
	//Dprintf(D_CALL,"operation:%lli\n", operation);

	if (type==RESPONSE)
	{
		offset+=length;
		length=4;//RESULT_STATUS
		//long long result = output_param(buffer, offset, length, "result", NULL,0);
		//Dprintf(D_CALL,"result:%lli\n", result);
		if (//mnr==0xcabf &&
				operation== PVFS_SERV_IO)
		{
			//delete?
			//offset+=length;
			//dump_IO_Response(buffer, size,offset);
		}
		if (//mnr==0xcabf &&
				operation== PVFS_SERV_WRITE_COMPLETION)
		{
			offset+=length;
			struct dist * dist = (struct dist*) malloc(sizeof(struct dist));

			dist->aggregate_size=dump_Write_Completion(buffer, size,offset);
			return dist;
			//return this
		}

		if (operation==PVFS_SERV_SMALL_IO)
		{
			offset+=length;
			struct dist * dist = (struct dist*) malloc(sizeof(struct dist));

			dist->aggregate_size=dump_SMALL_IO_Completion(buffer, size,offset);
			return dist;
		}
		return NULL;
	}
	if (type==REQUEST)
	{
		//fprintf(stderr,"type is request\n");
		if (//mnr==0xcabf &&
				operation== PVFS_SERV_IO || operation== PVFS_SERV_SMALL_IO)
		{
			int small=0;
			if (operation==PVFS_SERV_SMALL_IO)
			{
				small=1;
			}
			//fprintf(stderr,"operation is IO\n");
			offset+=length;
/*			length=4;//padding 4

			offset+=length;
			length=4;
			//long long user_id = output_param(buffer, offset, length, "user id", NULL,0);
			//fprintf(stderr,"user_id:%lli\n", user_id);
			//Dprintf(D_CALL,"user_id:%lli\n", user_id);
			offset+=length;
			length=4;//credentials 8,user 4, group 4
			//long long group_id = output_param(buffer, offset, length, "group id", NULL,0);
			//Dprintf(D_CALL,"group_id:%lli\n", group_id);
			offset+=length;*/
			offset+=12;
			length=4;//hint_count 4
			long long hint_count = output_param(buffer, offset, length, "hint count", NULL,0);
			//Dprintf(D_CALL,"hint_count:%lli\n", hint_count);
			int i=0;
			for (i=0;i<hint_count;i++)
			{
				offset+=length;
				length=4;//hint_count 4
				long long hint_type = output_param(buffer, offset, length, "hint type", NULL,0);
				//Dprintf(D_CALL,"  hint_type:%lli\n", hint_type);
				if (hint_type==PINT_HINT_HANDLE)
				{
					offset+=length;
					length=8;//hint_length 8

					//long long hint_handle = output_param(buffer, offset, length, "hint handle", NULL,0);
					//Dprintf(D_CALL,"  hint_handle:%lli\n", hint_handle);
				}
				else
				{
					//src/common/misc/pint-hint.c, line 36
					offset+=length;
					length=4;//hint_length 4

					//long long hint_other = output_param(buffer, offset, length, "hint other", NULL,0);
					//Dprintf(D_CALL,"  hint_other:%lli\n", hint_other);
				}
				//hint type 4
				//hint depending on the hint type (if ==handle[3]), handle[8]
			}


			offset+=length;
			return dump_IO_Request(buffer, size, offset,small);//hints, distribution, etc
		}
		return NULL;
	}
	return NULL;

}
