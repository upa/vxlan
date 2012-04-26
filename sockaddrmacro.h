#ifndef _SOCKADDRMACRO_H_
#define _SOCKADDRMACRO_H_

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>


#define EXTRACT_INADDR(sa) \
        (((struct sockaddr_in *)(&(sa)))->sin_addr)

#define EXTRACT_IN6ADDR(sa) \
        (((struct sockaddr_in6 *)(&(sa)))->sin6_addr)

#define EXTRACT_FAMILY(sa) \
        (((struct sockaddr *)(&(sa)))->sa_family)

#define EXTRACT_SALEN(sa) \
	(EXTRACT_FAMILY(sa) == AF_INET) ? sizeof (struct sockaddr_in) : 	\
	(EXTRACT_FAMILY(sa) == AF_INET6) ? sizeof (struct sockaddr_in6) : 	\
	-1


#define EXTRACT_PORT(sa) (((struct sockaddr_in *)&(sa))->sin_port)

#define INET_STOP(sa, c) \
        do {                                                            \
		switch (EXTRACT_FAMILY(sa)) {                           \
		case AF_INET6 :                                         \
			inet_ntop (AF_INET6, &EXTRACT_IN6ADDR(sa),      \
				   c, sizeof (c));			\
			break;                                          \
		case AF_INET :                                          \
			inet_ntop (AF_INET, &EXTRACT_INADDR(sa),        \
				   c, sizeof (c));			\
			break;                                          \
		}                                                       \
        } while (0)                                                     \
 

#define PRINT_ADDR(desc, sa)                                            \
        do {                                                            \
		char addrbuf[128] = "";                                 \
		switch (EXTRACT_FAMILY(sa)) {                           \
		case AF_INET6 :                                         \
			inet_ntop (AF_INET6, &EXTRACT_IN6ADDR(sa),      \
				   addrbuf, sizeof (addrbuf));          \
			printf ("%s %s\n", desc, addrbuf);              \
			break;                                          \
		case AF_INET :                                          \
			inet_ntop (AF_INET, &EXTRACT_INADDR(sa),        \
				   addrbuf, sizeof (addrbuf));          \
			printf ("%s %s\n", desc, addrbuf);              \
			break;                                          \
		default :                                               \
			printf ("%s invalid family %d\n", desc,         \
				EXTRACT_FAMILY(sa));                    \
		}                                                       \
        } while (0)                                                     \

#define FILL_SOCKADDR(af, sa, in)                                       \
        do {                                                            \
		switch (af) {                                           \
		case AF_INET :                                          \
			((struct sockaddr_in *)(sa))->sin_family = AF_INET; \
			memcpy (&(((struct sockaddr_in *)(sa))->sin_addr), \
				&(in),                                  \
				sizeof (struct in_addr));               \
									\
			break;                                          \
									\
		case AF_INET6:                                          \
			((struct sockaddr_in6 *)(sa))->sin6_family = AF_INET6; \
			memcpy (&(((struct sockaddr_in6 *)(sa))->sin6_addr), \
				&(in),                                  \
				sizeof (struct in6_addr));              \
			break;                                          \
		}                                                       \
        } while (0)


#define COMPARE_SOCKADDR(s1, s2)                                        \
        ((struct sockaddr *)s1)->sa_family !=                           \
                (((struct sockaddr *)s2)->sa_family) ? -1 :             \
                (((struct sockaddr *)s1)->sa_family == AF_INET) ?       \
                COMPARE_SOCKADDR_IN (s1, s2) :COMPARE_SOCKADDR_IN6 (s1, s2) \

#define COMPARE_SOCKADDR_IN(sa1, sa2)                                   \
        COMPARE_SADDR_IN (((struct sockaddr_in *)sa1)->sin_addr,        \
			  ((struct sockaddr_in *)sa2)->sin_addr)

#define COMPARE_SOCKADDR_IN6(sa61, sa62)                                \
        COMPARE_SADDR_IN6(((struct sockaddr_in6 *)sa61)->sin6_addr,     \
			  ((struct sockaddr_in6 *)sa62)->sin6_addr)

#define COMPARE_SADDR_IN(in1, in2)              \
        (in1.s_addr == in2.s_addr) ? 1 : -1

#define COMPARE_SADDR_IN6(in61, in62)                           \
        (in61.s6_addr32[0] != in62.s6_addr32[0]) ? -1 :         \
        (in61.s6_addr32[1] != in62.s6_addr32[1]) ? -1 :         \
        (in61.s6_addr32[2] != in62.s6_addr32[2]) ? -1 :         \
        (in61.s6_addr32[3] != in62.s6_addr32[3]) ? -1 : 1       \

#define COMPARE_INET6_PREFIX(a, b)					\
        (a.s6_addr32[0] == b.s6_addr32[0] && a.s6_addr32[1] == b.s6_addr32[1])

#define MEMCMP_SOCKADDR(a, b)					\
	((EXTRACT_FAMILY ((a)) == AF_INET) ?			\
	 memcmp (&(a), &(b), sizeof (struct sockaddr_in)) :	\
	 memcmp (&(a), &(b), sizeof (struct sockaddr_in6))) 


#endif /* _SOCKADDRMACRO_H_ */
