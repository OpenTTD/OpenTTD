#ifndef _getnameinfo_h
#define _getnameinfo_h
/*
 * Reconstructed from KAME getnameinfo.c (in lib/)
 */

    /* getnameinfo flags */
#define	NI_NOFQDN	0x0001
#define	NI_NUMERICHOST	0x0002	/* return numeric form of address */
#define	NI_NAMEREQD	0x0004	/* request DNS name */
#define	NI_NUMERICSERV	0x0008
#define	NI_DGRAM	0x0010

#ifdef	__cplusplus
extern "C" {
#endif
/* RFC 2553 / Posix resolver */
int getnameinfo(const struct sockaddr *sa,
                              socklen_t salen,
                              char *host,
                              size_t hostlen,
                              char *serv,
                              size_t servlen,
                              int flags );
#ifdef	__cplusplus
}
#endif

#endif /* _getnameinfo_h */
