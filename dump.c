/*
 * dump.c
 *
 *  Created on: Jul 6, 2010
 *      Author: yiqi
 */


#include "dump.h"
#include "proxy2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
extern char* log_prefix;
//each function makes sure the headers are dumped in one call only

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

	offset+=((num_nested_req+1)*80);
	/*for (i=0;i<num_nested_req+1;i++)
	{
		///io/desc/pint-req-encode.h 104, 56
		//proto/pvfs2-req-proto.h
		//hexdump(buffer+offset,80,D_CALL);
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
*/

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
				return 0;
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
	}

}
