/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _POSIX_SYS_SOCKET_H
#define _POSIX_SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include "sys/ip_addr.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Create u32_t value from bytes */
#define LWIP_MAKEU32(a, b, c, d)    (((uint32_t)((a)&0xff) << 24) | \
                                    ((uint32_t)((b)&0xff) << 16)  | \
                                    ((uint32_t)((c)&0xff) << 8)   | \
                                    (uint32_t)((d)&0xff))

/* If your port already typedef's sa_family_t, define SA_FAMILY_T_DEFINED
   to prevent this code from redefining it. */
#if !defined(sa_family_t) && !defined(SA_FAMILY_T_DEFINED)
typedef uint8_t sa_family_t;
#endif

/* If your port already typedef's in_addr_t, define IN_ADDR_T_DEFINED
   to prevent this code from redefining it. */
#if !defined(in_addr_t) && !defined(IN_ADDR_T_DEFINED)
typedef uint32_t in_addr_t;
#endif

struct in_addr
{
    in_addr_t s_addr;
};

/* If your port already typedef's in_port_t, define IN_PORT_T_DEFINED
   to prevent this code from redefining it. */
#if !defined(in_port_t) && !defined(IN_PORT_T_DEFINED)
typedef uint16_t in_port_t;
#endif

/* members are in network byte order */
struct sockaddr_in
{
    uint8_t sin_len;
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
#define SIN_ZERO_LEN 8
    char sin_zero[SIN_ZERO_LEN];
};

struct sockaddr
{
    uint8_t sa_len;
    sa_family_t sa_family;
    char sa_data[14];
};

/* If your port already typedef's socklen_t, define SOCKLEN_T_DEFINED
   to prevent this code from redefining it. */
#if !defined(socklen_t) && !defined(SOCKLEN_T_DEFINED)
typedef uint32_t socklen_t;
#endif

struct msghdr
{
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    socklen_t msg_controllen;
    int msg_flags;
};

/* struct msghdr->msg_flags bit field values */
#define MSG_TRUNC   0x04
#define MSG_CTRUNC  0x08

/* RFC 3542, Section 20: Ancillary Data */
struct cmsghdr
{
    socklen_t cmsg_len; /* number of bytes, including header */
    int cmsg_level; /* originating protocol */
    int cmsg_type; /* protocol-specific type */
};

/* Data section follows header and possible padding, typically referred to as
      unsigned char cmsg_data[]; */

/* cmsg header/data alignment. NOTE: we align to native word size (double word
size on 16-bit arch) so structures are not placed at an unaligned address.
16-bit arch needs double word to ensure 32-bit alignment because socklen_t
could be 32 bits. If we ever have cmsg data with a 64-bit variable, alignment
will need to increase long long */
#define ALIGN_H(size) (((size) + sizeof(long) - 1U) & ~(sizeof(long)-1U))
#define ALIGN_D(size) ALIGN_H(size)

#define CMSG_FIRSTHDR(mhdr) \
          ((mhdr)->msg_controllen >= sizeof(struct cmsghdr) ? \
           (struct cmsghdr *)(mhdr)->msg_control : \
           (struct cmsghdr *)NULL)

#define CMSG_NXTHDR(mhdr, cmsg) \
        (((cmsg) == NULL) ? CMSG_FIRSTHDR(mhdr) : \
         (((u8_t *)(cmsg) + ALIGN_H((cmsg)->cmsg_len) \
                            + ALIGN_D(sizeof(struct cmsghdr)) > \
           (u8_t *)((mhdr)->msg_control) + (mhdr)->msg_controllen) ? \
          (struct cmsghdr *)NULL : \
          (struct cmsghdr *)((void*)((u8_t *)(cmsg) + \
                                      ALIGN_H((cmsg)->cmsg_len)))))

#define CMSG_DATA(cmsg) ((void *)((u8_t *)(cmsg) + ALIGN_D(sizeof(struct cmsghdr))))

#define CMSG_SPACE(length) (ALIGN_D(sizeof(struct cmsghdr)) + ALIGN_H(length))

#define CMSG_LEN(length) (ALIGN_D(sizeof(struct cmsghdr)) + length)

/* Socket protocol types (TCP/UDP/RAW) */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

/*
 * Option flags per-socket. These must match the SOF_ flags in ip.h (checked in init.c)
 */
#define SO_REUSEADDR   0x0004 /* Allow local address reuse */
#define SO_KEEPALIVE   0x0008 /* keep connections alive */
#define SO_BROADCAST   0x0020 /* permit to send and to receive broadcast messages (see IP_SOF_BROADCAST option) */


/*
 * Additional options, not kept in so_options.
 */
#define SO_DEBUG        0x0001 /* Unimplemented: turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002 /* socket has had listen() */
#define SO_DONTROUTE    0x0010 /* Unimplemented: just use interface addresses */
#define SO_USELOOPBACK  0x0040 /* Unimplemented: bypass hardware when possible */
#define SO_LINGER       0x0080 /* linger on close if data present */
#define SO_DONTLINGER   ((int)(~SO_LINGER))
#define SO_OOBINLINE    0x0100 /* Unimplemented: leave received OOB data in line */
#define SO_REUSEPORT    0x0200 /* Unimplemented: allow local address & port reuse */
#define SO_SNDBUF       0x1001 /* Unimplemented: send buffer size */
#define SO_RCVBUF       0x1002 /* receive buffer size */
#define SO_SNDLOWAT     0x1003 /* Unimplemented: send low-water mark */
#define SO_RCVLOWAT     0x1004 /* Unimplemented: receive low-water mark */
#define SO_SNDTIMEO     0x1005 /* send timeout */
#define SO_RCVTIMEO     0x1006 /* receive timeout */
#define SO_ERROR        0x1007 /* get error status and clear */
#define SO_TYPE         0x1008 /* get socket type */
#define SO_CONTIMEO     0x1009 /* Unimplemented: connect timeout */
#define SO_NO_CHECK     0x100a /* don't create UDP checksum */
#define SO_BINDTODEVICE 0x100b /* bind to device */

/*
 * Structure used for manipulating linger option.
 */
struct linger
{
    int l_onoff; /* option on/off */
    int l_linger; /* linger time in seconds */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define  SOL_SOCKET  0xfff    /* options for socket level */

#define AF_UNSPEC       0
#define AF_INET         2
#define AF_INET6        AF_UNSPEC
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6
#define PF_UNSPEC       AF_UNSPEC

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_UDPLITE 136
#define IPPROTO_RAW     255

/* Flags we can use with send and recv. */
#define MSG_PEEK       0x01    /* Peeks at an incoming message */
#define MSG_WAITALL    0x02    /* Unimplemented: Requests that the function block until the full amount of data requested can be returned */
#define MSG_OOB        0x04    /* Unimplemented: Requests out-of-band data. The significance and semantics of out-of-band data are protocol-specific */
#define MSG_DONTWAIT   0x08    /* Nonblocking i/o for this operation only */
#define MSG_MORE       0x10    /* Sender will send more */
#define MSG_NOSIGNAL   0x20    /* Uninmplemented: Requests not to send the SIGPIPE signal if an attempt to send is made on a stream-oriented socket that is no longer connected. */


/*
 * Options for level IPPROTO_IP
 */
#define IP_TOS             1
#define IP_TTL             2
#define IP_PKTINFO         8
/*
 * Options for level IPPROTO_TCP
 */
#define TCP_NODELAY    0x01    /* don't delay send to coalesce packets */
#define TCP_KEEPALIVE  0x02    /* send KEEPALIVE probes when idle for pcb->keep_idle milliseconds */
#define TCP_KEEPIDLE   0x03    /* set pcb->keep_idle  - Same as TCP_KEEPALIVE, but use seconds for get/setsockopt */
#define TCP_KEEPINTVL  0x04    /* set pcb->keep_intvl - Use seconds for get/setsockopt */
#define TCP_KEEPCNT    0x05    /* set pcb->keep_cnt   - Use number of probes sent for get/setsockopt */

/*
 * The Type of Service provides an indication of the abstract
 * parameters of the quality of service desired.  These parameters are
 * to be used to guide the selection of the actual service parameters
 * when transmitting a datagram through a particular network.  Several
 * networks offer service precedence, which somehow treats high
 * precedence traffic as more important than other traffic (generally
 * by accepting only traffic above a certain precedence at time of high
 * load).  The major choice is a three way tradeoff between low-delay,
 * high-reliability, and high-throughput.
 * The use of the Delay, Throughput, and Reliability indications may
 * increase the cost (in some sense) of the service.  In many networks
 * better performance for one of these parameters is coupled with worse
 * performance on another.  Except for very unusual cases at most two
 * of these three indications should be set.
 */
#define IPTOS_TOS_MASK          0x1E
#define IPTOS_TOS(tos)          ((tos) & IPTOS_TOS_MASK)
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
#define IPTOS_LOWCOST           0x02
#define IPTOS_MINCOST           IPTOS_LOWCOST

/*
 * The Network Control precedence designation is intended to be used
 * within a network only.  The actual use and control of that
 * designation is up to each network. The Internetwork Control
 * designation is intended for use by gateway control originators only.
 * If the actual use of these precedence designations is of concern to
 * a particular network, it is the responsibility of that network to
 * control the access to, and use of, those precedence designations.
 */
#define IPTOS_PREC_MASK                 0xe0
#define IPTOS_PREC(tos)                ((tos) & IPTOS_PREC_MASK)
#define IPTOS_PREC_NETCONTROL           0xe0
#define IPTOS_PREC_INTERNETCONTROL      0xc0
#define IPTOS_PREC_CRITIC_ECP           0xa0
#define IPTOS_PREC_FLASHOVERRIDE        0x80
#define IPTOS_PREC_FLASH                0x60
#define IPTOS_PREC_IMMEDIATE            0x40
#define IPTOS_PREC_PRIORITY             0x20
#define IPTOS_PREC_ROUTINE              0x00

#ifndef SHUT_RD
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#endif

#define ip4_addr_get_u32(src_ipaddr) ((src_ipaddr)->addr)
#define htons(x) ((uint16_t)((((x) & (uint16_t)0x00ffU) << 8) | (((x) & (uint16_t)0xff00U) >> 8)))
#define ntohs(x) htons(x)

int socket(int domain, int type, int protocol);
int bind(int socket, const struct sockaddr *address, socklen_t address_len);
int accept(int socket, struct sockaddr *address, socklen_t *address_len);
int shutdown(int s, int how);
int connect(int socket, const struct sockaddr *address, socklen_t address_len);
int listen(int socket, int backlog);
int recv(int socket, void *mem, size_t len, int flags);
int send(int socket, const void *data, size_t size, int flags);
int recvfrom(int socket, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int sendto(int socket, const void *data, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);

#define inet_addr(cp) sys_ipaddr_addr(cp)
#define inet_aton(cp, addr) sys_ip4addr_aton(cp, (ip4_addr_t *)addr)
#define inet_ntoa(addr) sys_ip4addr_ntoa((const ip4_addr_t *)&(addr))

#define inet_ntop(af, src, dst, size) \
    (((af) == AF_INET) ? sys_ip4addr_ntoa_r((const ip4_addr_t *)(src), (dst), (size)) : NULL)
#define inet_pton(af, src, dst) \
    (((af) == AF_INET) ? sys_ip4addr_aton((src), (ip4_addr_t *)(dst)) : 0)
#define htonl(x)    ((((x)&0x000000ffUL) << 24) | \
                    (((x)&0x0000ff00UL) << 8)   | \
                    (((x)&0x00ff0000UL) >> 8)   | \
                    (((x)&0xff000000UL) >> 24))

#ifdef __cplusplus
}
#endif
#endif