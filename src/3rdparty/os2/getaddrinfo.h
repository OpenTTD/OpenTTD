#ifndef _getaddrinfo_h
#define _getaddrinfo_h

/*
 *  Shamelessly duplicated from the fetchmail public sources
 *  for use by the Squid Project under GNU Public License.
 *
 * Update/Maintenance History:
 *
 *    15-Aug-2007 : Copied from fetchmail 6.3.8
 *			- added protection around libray headers
 *
 *    16-Aug-2007 : Altered configure checks
 *                  Un-hacked slightly to use system gethostbyname()
 *
 *  Original License and code follows.
 */

/*
 *  This file is part of libESMTP, a library for submission of RFC 2822
 *  formatted electronic mail messages using the SMTP protocol described
 *  in RFC 2821.
 *
 *  Copyright (C) 2001,2002  Brian Stafford  <brian@stafford.uklinux.net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Structure and prototypes taken from RFC 2553 */

/* SG 23/09/2007:
On Windows the following definitions are already available, may be that
this could be needed on some other platform */
typedef int socklen_t;

struct addrinfo {
    int ai_flags;  	  	/* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
    int ai_family; 	  	/* PF_xxx */
    int ai_socktype;	 	/* SOCK_xxx */
    int ai_protocol;	 	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
    socklen_t ai_addrlen;	/* length of ai_addr */
    char *ai_canonname;		/* canonical name for nodename */
    struct sockaddr *ai_addr;	/* binary address */
    struct addrinfo *ai_next;	/* next structure in linked list */
};

/* Supposed to be defined in <netdb.h> */
#define AI_ADDRCONFIG  0
#define AI_PASSIVE     1       /* Socket address is intended for `bind'.  */
#define AI_CANONNAME   2       /* Request for canonical name.  */
#define AI_NUMERICHOST 4       /* Don't use name resolution.  */

/* Supposed to be defined in <netdb.h> */
#define EAI_ADDRFAMILY 1   /* address family for nodename not supported */
#define EAI_AGAIN      2   /* temporary failure in name resolution */
#define EAI_BADFLAGS   3   /* invalid value for ai_flags */
#define EAI_FAIL       4   /* non-recoverable failure in name resolution */
#define EAI_FAMILY     5   /* ai_family not supported */
#define EAI_MEMORY     6   /* memory allocation failure */
#define EAI_NODATA     7   /* no address associated with nodename */
#define EAI_NONAME     8   /* nodename nor servname provided, or not known */
#define EAI_SERVICE    9   /* servname not supported for ai_socktype */
#define EAI_SOCKTYPE   10  /* ai_socktype not supported */

#ifndef EAI_SYSTEM
/* Not defined on mingw32. */
#define EAI_SYSTEM     11  /* System error returned in `errno'.  */
#endif
#ifndef EAI_OVERFLOW
/* Not defined on mingw32. */
#define EAI_OVERFLOW   12 /* Argument buffer overflow.  */
#endif

#ifdef	__cplusplus
extern "C" {
#endif
/* RFC 2553 / Posix resolver */
int getaddrinfo (const char *nodename, const char *servname,
                               const struct addrinfo *hints, struct addrinfo **res);
/* Free addrinfo structure and associated storage */
void freeaddrinfo (struct addrinfo *ai);

/* Convert error return from getaddrinfo() to string */
const char *gai_strerror (int code);
#ifdef	__cplusplus
}
#endif

#endif /* _getaddrinfo_h */
