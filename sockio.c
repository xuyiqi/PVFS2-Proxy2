/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
 *
 *  This file is partially copied from PVFS2 source code - BMI/tcp implementation.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "sockio.h"
#include <stdlib.h>
#include "proxy2.h"
#include "logging.h"
int BMI_sockio_set_tcpopt(int s,
	       int optname,
	       int val)
{
    if (setsockopt(s, IPPROTO_TCP, optname, &val, sizeof(val)) == -1)
	return (-1);
    else
	return (val);
}

int BMI_sockio_set_sockopt(int s,
		int optname,
		int val)
{

    if (setsockopt(s, SOL_SOCKET, optname, &val, sizeof(val)) == -1)
	return (-1);
    else
	return (val);
}

/* NOTE: this function returns BMI error codes */
int BMI_sockio_connect_sock(int sockd,
		 const char *name,
		 int service,
		 char* ip,
		 int* port
)
{
    struct sockaddr saddr;
    int ret;

    if ((ret = BMI_sockio_init_sock(&saddr, name, service)) != 0)
	return (ret);
  connect_sock_restart:
    if (connect(sockd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
    {
	if (errno == EINTR)
	    goto connect_sock_restart;
        return(-errno);
    }
	unsigned int addr=((struct sockaddr_in *)&saddr)->sin_addr.s_addr;
	int ssize = sprintf(ip,"%u.%u.%u.%u",(addr<<24)>>24,	(addr<<16)>>24,(addr<<8)>>24, addr>>24
			);
	int l = sizeof (saddr);
	getsockname(sockd, &saddr, &l);
	*port=ntohs(((struct sockaddr_in *)&saddr)->sin_port);
	//fprintf(stderr,"returning %i\n");
    return (sockd);
}
/* routines to get and set socket options */
int BMI_sockio_get_sockopt(int s,
		int optname)
{
    int val;
    socklen_t len = sizeof(val);

    if (getsockopt(s, SOL_SOCKET, optname, &val, &len) == -1)
	return (-1);
    else
	return (val);
}

int BMI_sockio_init_sock(struct sockaddr *saddrp,
			 const char *name,
			 int service)
{
    int ret;
    struct in_addr addr;

    bzero((char *) saddrp, sizeof(struct sockaddr_in));
    if (name == NULL || strcasecmp(name, "localhost")==0)
    {
	ret = inet_aton("127.0.0.1", &addr);
    }
    else
    {
	ret = inet_aton(name, &addr);
    }

    if (ret == 0) return -1;

    ((struct sockaddr_in *) saddrp)->sin_family = AF_INET;
    ((struct sockaddr_in *) saddrp)->sin_port = htons((u_short) service);
    memcpy((char *) &(((struct sockaddr_in *) saddrp)->sin_addr), &addr,
	  sizeof(addr));

    return 0;
}


int BMI_sockio_new_sock()
{
    return(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
}

/* NOTE: this function returns BMI error codes */

int BMI_sockio_bind_sock_specific(int sockd,
              const char *name,
	      int service)
{
    struct sockaddr saddr;
    int ret;

    if ((ret = BMI_sockio_init_sock(&saddr, name, service)) != 0)
	return (ret); /* converted to PVFS error code below */

  bind_sock_restart:
    if (bind(sockd, &saddr, sizeof(saddr)) < 0)
    {
	if (errno == EINTR)
	    goto bind_sock_restart;
        return(-errno);
    }
    return (sockd);
}

int BMI_sockio_bind_sock(int sockd,
	      int service)
{
    struct sockaddr_in saddr;

    bzero((char *) &saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((u_short) service);
    saddr.sin_addr.s_addr = INADDR_ANY;
  bind_sock_restart:
    if (bind(sockd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
    {
	if (errno == EINTR)
	    goto bind_sock_restart;
	return (-1);
    }
    return (sockd);
}

int BMI_sockio_nbpeek(int s, void* buf, int len)
{
    int ret;
    assert(fcntl(s, F_GETFL, 0) & O_NONBLOCK);

  nbpeek_restart:
    ret = recv(s, buf, len, (MSG_PEEK|DEFAULT_MSG_FLAGS));
    if(ret == 0)
    {
        errno = EPIPE;
        return (-1);
    }
    else if (ret == -1 && errno == EWOULDBLOCK)
    {
        return(0);
    }
    else if (ret == -1 && errno == EINTR)
    {
        goto nbpeek_restart;
    }
    else if (ret == -1)
    {
        return (-1);
    }

    return(ret);
}

