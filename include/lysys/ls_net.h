#ifndef _LS_NET_H_
#define _LS_NET_H_

#include "ls_defs.h"

#define LS_NET_STREAM 1
#define LS_NET_DGRAM 2

#define LS_NET_PROTO_TCP 1
#define LS_NET_PROTO_UDP 2

#define LS_AF_UNSPEC 0
#define LS_AF_INET 2
#define LS_AF_INET6 23

#define LS_NET_SHUT_RECV 0
#define LS_NET_SHUT_SEND 1
#define LS_NET_SHUT_BOTH 2

#define LS_NET_MAXCONN 0x7fffffff

ls_handle ls_net_connect(const char *host, unsigned short port, int type, int protocol, int addr_family);

ls_handle ls_net_listen(const char *host, unsigned short port, int type, int protocol, int addr_family, int backlog);

ls_handle ls_net_accept(ls_handle sock);

//! \brief Shutdown a connection
//! 
//! Causes the socket to stop receiving, sending, or both. The
//! handle is still valid after this call, but no more data can
//! be sent or received, depending on the value of how.
//! 
//! \param sock The socket handle
//! \param how How to shutdown the connection, one of LS_NET_SHUT_RECV,
//! LS_NET_SHUT_SEND, or LS_NET_SHUT_BOTH
//! 
//! \return 0 on success, -1 on error
int ls_net_shutdown(ls_handle sock, int how);

//! \brief Get the remote host of a socket
//! 
//! \param sock The socket handle
//! 
//! \return The remote host of the socket. NULL is a valid return value,
//! so check ls_get_errno() to determine if an error occurred.
const char *ls_net_gethost(ls_handle sock);

//! \brief Get the remote port of a socket
//! 
//! \param sock The socket handle
//! 
//! \return The remote port of the socket. 0 is a valid return value,
//! so check ls_get_errno() to determine if an error occurred.
unsigned short ls_net_getport(ls_handle sock);

size_t ls_net_recv(ls_handle sock, void *buffer, size_t size);

size_t ls_net_send(ls_handle sock, const void *buffer, size_t size);

#endif // _LS_NET_H_
