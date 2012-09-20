/*
 * signals.h
 *
 * Signal handling
 */

#ifndef SIGNALS_H
#define SIGNALS_H

extern void install_signal_handler(int signo, void (*handler)(int));
extern void ignore_signal(int signo);

#endif /* SIGNALS_H */
