#ifndef __sysinclude_h__
#define __sysinclude_h__

/* First of all, pull in answers from autoconfiguration system. */
#define PACKAGE "VIRKOM64"

#define __STDC_FORMAT_MACROS // for PRIx64,PRIu64,PRId64
#include <inttypes.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
//#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif /* HAVE_GETOPT_H */

#include <stdarg.h>

/* We need this for mmap(2). */
#if !defined(MAP_FAILED)
# define MAP_FAILED ((caddr_t)-1L)
#endif

/* Random OS-specific wart-massaging. */
#if defined(__ultrix__)
extern "C" {
# include <sys/ioctl.h>
	extern void bzero(char *b1, int length);
	extern int getopt(int argc, char **argv, char *optstring);
	extern char *optarg;
	extern int optind, opterr, optopt;
	extern char *strdup(const char *s);
	extern int getpagesize(void);
	extern int gettimeofday(struct timeval *tp, struct timezone *tzp);
	int ioctl(int d, int request, void *argp);
	extern caddr_t mmap(caddr_t addr, size_t len, int prot, int flags, int fd,
		off_t off);
	extern caddr_t munmap(caddr_t addr, size_t len);
	extern long random(void);
	extern int select(int nfds, fd_set *readfds, fd_set *writefds,
		fd_set *exceptfds, struct timeval *timeout);
	extern int strcasecmp(char *s1, char *s2);
}
#endif

typedef uint64_t	uint64;
typedef int64_t		int64;
typedef uint32_t	uint32;
typedef int32_t		int32;
typedef uint16_t	uint16;
typedef int16_t		int16;
typedef uint8_t		uint8;
typedef int8_t		int8;

#endif /* __sysinclude_h__ */
