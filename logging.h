/*
 * logging.h	Definitions for the logging functions.
 *
 * Authors:	Donald J. Becker, <becker@super.org>
 *		Rick Sladkey, <jrs@world.std.com>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This software maybe be used for any purpose provided
 *		the above copyright notice is retained.  It is supplied
 *		as is, with no warranty expressed or implied.
 *
 * Update:	Added hexdump.
 *		Ming, June 2005.
 */

#ifndef LOGGING_H
#define LOGGING_H
//#define HAVE_SYSLOG_H
#define HAVE_VPRINTF
/* Logging/Debug levels */
#define L_ERROR		0x0001
#define L_WARNING	0x0002
#define L_NOTICE	0x0004
#define L_FATAL		0x0008
#define D_GENERAL	0x0100
#define D_CALL		0x0200
#define D_CACHE	0x0400
#define D_AUTH		0x0800
#define D_UGID		0x1000
#define D_RMTAB		0x2000
#define D_DEVTAB	0x8000
#define D_ALL		0xFFFF

/* Global Function prototypes. */
extern void log_open(char *progname, int foreground);
extern void log_close(void);
extern void enable_logging(char *kind);
extern int  logging_enabled(int level);
extern void toggle_logging(int sig);
extern void background_logging(void);
extern void Dprintf(int level, const char *fmt, ...);
//extern void log_call(struct svc_req *rqstp, char *name,	char *arg);
extern int hexdump(char *data, int data_len, int flag);
#endif /* LOGGING_H */

/* End of logging.h. */
