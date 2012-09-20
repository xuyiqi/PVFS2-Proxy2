/*
 * logging	This module handles the logging of requests.
 *
 * TODO:	Merge the two "XXX_log() calls.
 *
 * Authors:	Donald J. Becker, <becker@super.org>
 *		Rick Sladkey, <jrs@world.std.com>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Olaf Kirch, <okir@monad.swb.de>
 *
 *		This software maybe be used for any purpose provided
 *		the above copyright notice is retained.  It is supplied
 *		as is, with no warranty expressed or implied.
 *
 * Update:	Added hexdump.
 *		Ming, June 2005.
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "logging.h"
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#else
#define LOG_FILE  "/var/tmp/%s.log"
#endif

#define LOG_SIZE 1024

int  logging = 0;    /* enable/disable DEBUG logs  */
static int  dbg_mask = 0;//D_GENERAL; /* What will be logged    */
static char log_name[LOG_SIZE];   /* name of this program   */
static FILE *log_fp = (FILE *)NULL; /* fp for the log file    */
static char* logfile;

void
log_open(char *progname, int foreground)
{
	fprintf(stderr,"0\n");
#ifdef HAVE_SYSLOG_H
  openlog(progname, LOG_PID | LOG_NDELAY, LOG_DAEMON );
  if (foreground)
  {
fprintf(stderr,"here?\n");
    log_fp = stderr;
  }
#else
  if (foreground) {
	  fprintf(stderr,"here2?\n");
    log_fp = stderr;
  } else {
    char path[LOG_SIZE];

    if(strlen(progname)>(LOG_SIZE-strlen(LOG_FILE)))
    {
    	fprintf(stderr,"file name too long\n");
      return;

    }
    sprintf(path, LOG_FILE, progname);
    logfile = path;
    if ((log_fp = fopen(path, "w")) == NULL)
    {
    	fprintf(stderr,"cannot open %s in append mode\n", path);
      return;
    }
      fprintf(stderr, "%s\n", path);
  }
#endif
  if (log_fp != NULL)
    setbuf(log_fp, (char *) NULL);
  else
  {
	  fprintf(stderr,"log file open failed\n");
  }
  if(strlen(progname)>(LOG_SIZE-16)) {
    strcpy(log_name,"");
    fprintf(stderr,"program name too long?\n");
    return;
  }
  sprintf(log_name, "%s[%d]", progname, (int)getpid());
  fprintf(stderr, "final name %s\n",log_name);
}

void
log_close(void)
{
  if (log_fp) {
    fclose(log_fp);
    log_fp = 0;
  }
}


void
background_logging(void)
{
  if (log_fp == stderr)
    log_fp = NULL;
}

void
toggle_logging(int sig)
{
  Dprintf(D_GENERAL, "turned off logging\n");
  logging = 1 - logging;
  Dprintf(D_GENERAL, "turned on logging\n");
}

void
enable_logging(char *kind)
{
  if ('a' == *kind && !strcmp(kind, "auth"))
    dbg_mask |= D_AUTH;
  else if ('a' == *kind && !strcmp(kind, "all"))
    dbg_mask |= D_ALL;
  else if ('c' == *kind && !strcmp(kind, "call"))
    dbg_mask |= D_CALL;
  else if ('d' == *kind && !strcmp(kind, "devtab"))
    dbg_mask |= D_DEVTAB;
  else if ('c' == *kind && !strcmp(kind, "cache"))
    dbg_mask |= D_CACHE;
  else if ('g' == *kind && !strcmp(kind, "general"))
    dbg_mask |= D_GENERAL;
  else if ('r' == *kind && !strcmp(kind, "rmtab"))
    dbg_mask |= D_RMTAB;
  else if ('s' == *kind && !strcmp(kind, "stale"))
    dbg_mask |= D_AUTH | D_CALL | D_CACHE;
  else if ('u' == *kind && !strcmp(kind, "ugid"))
    dbg_mask |= D_UGID;
  else
    fprintf (stderr, "Invalid debug facility: %s\n", kind);
  logging = 1;
  fprintf(stderr, "Final dbg_mask:#%08x\n", dbg_mask);
}

int
logging_enabled(int level)
{
  return (logging && (level & dbg_mask));
}


/* Write something to the system logfile. */
void
Dprintf(int kind, const char *fmt, ...)
{
  char buff[2048];
  va_list args;
  time_t now;
  struct tm *tm;
  int ret=1;

  if (!(kind & (L_FATAL | L_ERROR | L_WARNING | L_NOTICE))
      && !(logging && (kind & dbg_mask)))
  {
	  ret=1;
	  return;
  }
  else
  {
	  ret=0;
  }

  va_start(args, fmt);
#ifdef HAVE_VPRINTF
  vsnprintf(buff, sizeof(buff)-1, fmt, args);
#else
  /* Figure out how to use _doprnt here. */
#endif
  va_end(args);

#ifdef HAVE_SYSLOG_H
  if (kind & (L_FATAL | L_ERROR)) {
    (void) syslog(LOG_ERR, "%s", buff);
  } else if (kind & L_WARNING) {
    (void) syslog(LOG_WARNING, "%s", buff);
  } else if (kind & L_NOTICE) {
    (void) syslog(LOG_NOTICE, "%s", buff);
  } else if (log_fp == NULL) {
    (void) syslog(LOG_DEBUG, "%s", buff);
  }
#endif
  if (log_fp != (FILE *) NULL) {
    (void) time(&now);
    tm = localtime(&now);


    //int w_bytes =
	fprintf(stderr, "%s %02d/%02d/%02d %02d:%02d:%02d %s",
log_name, tm->tm_mon + 1, tm->tm_mday, tm->tm_year,
tm->tm_hour, tm->tm_min, tm->tm_sec, buff);
    		fprintf(log_fp, "%s %02d/%02d/%02d %02d:%02d:%02d %s",
      log_name, tm->tm_mon + 1, tm->tm_mday, tm->tm_year,
      tm->tm_hour, tm->tm_min, tm->tm_sec, buff);
    //fprintf(stderr,"%i bytes written\n",w_bytes);
    fflush(log_fp);
    //fprintf(stderr , "[FILE] %s %02d/%02d/%02d %02d:%02d:%02d %s",
      //log_name, tm->tm_mon + 1, tm->tm_mday, tm->tm_year,
      //tm->tm_hour, tm->tm_min, tm->tm_sec, buff);
    if (strchr(buff, '\n') == NULL)
      fputc('\n', log_fp);
  }
  else
  {
	    (void) time(&now);
	    tm = localtime(&now);
	    //fprintf(stderr, "[RETURNED] %s %02d/%02d/%02d %02d:%02d:%02d %s",
	    //  log_name, tm->tm_mon + 1, tm->tm_mday, tm->tm_year,
	    //  tm->tm_hour, tm->tm_min, tm->tm_sec, buff);
  }

  if (kind & L_FATAL)
  {
	  fprintf(stderr, "FATAL message encountered, exiting\n");
    exit(-1);
  }
}



int
hexdump(char *data, int data_len, int flag)
{
  int i, j;
  char buf[1000];
  char temp[10];

  if(data == NULL) {
    Dprintf(flag, "null\n");
    return -1;
  }

  for(i=0; i<data_len; i++) {
    j = sprintf(temp, "%x", data[i]);
    if(j < 2) {
      buf[(i%16)*3] = '0';
      strcpy(buf+(i%16)*3+1, temp+j-1);
    }
    else strcpy(buf+(i%16)*3, temp+j-2);
    strcpy(buf+(i%16)*3+2, " ");
    if(i%16 == 15) Dprintf(flag, "%s", buf);
  }

  if(i%16 != 0) Dprintf(flag, "%s", buf);

  return 1;
}
