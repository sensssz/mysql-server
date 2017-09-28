/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* Consider this file as an extension to config.h, just that it contains
 * static text. The intention is to reduce the number of #ifdefs in the rest
 * of the source files without having to put all of them in AH_BOTTOM
 * in configure.ac.
 */
#ifndef CONFIG_STATIC_H
#define CONFIG_STATIC_H 1

#ifdef WIN32
#define SOCKETPAIR_AF AF_INET
#define get_socket_error() WSAGetLastError()
extern void initialize_sockets(void);
#else
#define closesocket(a) close(a)
#define SOCKET int
#define SOCKETPAIR_AF AF_UNIX
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define get_socket_error() errno
#define initialize_sockets()
#endif

#include <dlfcn.h>

#include <sys/types.h>

#ifdef HAVE_LINK_H
#include <link.h>
#endif

#include <stdbool.h>

#include <inttypes.h>

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
/* todo: we should move this file out of win32, because it could be used
 * on all platforms without it's own sysexits.h */
#include <win32/sysexits.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/un.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <sys/uio.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>


/* some POSIX systems need the following definition
 * to get mlockall flags out of sys/mman.h.  */
#ifndef _P1003_1B_VISIBLE
#define _P1003_1B_VISIBLE
#endif
/* need this to get IOV_MAX on some platforms. */
#ifndef __need_IOV_MAX
#define __need_IOV_MAX
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

/* FreeBSD 4.x doesn't have IOV_MAX exposed. */
#ifndef IOV_MAX
#if defined(__FreeBSD__) || defined(__APPLE__)
# define IOV_MAX 1024
#endif
#endif

#if defined(ENABLE_SASL) || defined(ENABLE_ISASL)
#define SASL_ENABLED
#endif

#endif
