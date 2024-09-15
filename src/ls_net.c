#include <lysys/ls_net.h>

#include <lysys/ls_core.h>
#include <lysys/ls_file.h>

#include "ls_handle.h"

#include <stdio.h>
#include <string.h>

#if LS_WINDOWS
#include <WinSock2.h>
#include <WS2tcpip.h>
#else

#endif // LS_WINDOWS

#include "ls_native.h"

typedef struct ls_socket
{
	// Must be first member
#if LS_WINDOWS
	SOCKET socket;
#else
#endif // LS_WINDOWS

	char *host;
	unsigned short port;
	int can_recv, can_send;
} ls_socket_t;

typedef struct ls_server
{
	unsigned short port;

#if LS_WINDOWS
	SOCKET socket;
#else
#endif // LS_WINDOWS
} ls_server_t;

static void ls_socket_dtor(ls_socket_t *sock)
{
#if LS_WINDOWS
	ls_free(sock->host);
	(void)shutdown(sock->socket, SD_BOTH);
	(void)closesocket(sock->socket);
#else
#endif // LS_WINDOWS
}

static const struct ls_class SocketClass = {
	.type = LS_SOCKET,
	.cb = sizeof(ls_socket_t),
	.dtor = (ls_dtor_t)&ls_socket_dtor,
	.wait = NULL
};

static void ls_server_dtor(ls_server_t *server)
{
#if LS_WINDOWS
	closesocket(server->socket);
#else
#endif // LS_WINDOWS
}

static const struct ls_class ServerClass = {
	.type = LS_SERVER,
	.cb = sizeof(ls_server_t),
	.dtor = (ls_dtor_t)&ls_server_dtor,
	.wait = NULL
};

#if LS_WINDOWS

static int _wsa_initialized = 0;

static void ls_wsa_cleanup(void)
{
	WSACleanup();
}

static int ls_check_wsa()
{
	WSADATA wsaData;

	if (!_wsa_initialized)
	{
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return ls_set_errno(LS_NOT_SUPPORTED);

		_wsa_initialized = 1;
	
		atexit(ls_wsa_cleanup);
	}

	return 0;
}

static int ls_parse_sockaddr(const char *host, unsigned short port, int af, PSOCKADDR_STORAGE pAddr)
{
	PADDRINFOA pInfo, pPtr;
	char service[16];

	if (!host)
	{
		switch (af)
		{
		case AF_INET:
			host = "0.0.0.0";
			break;
		case AF_INET6:
			host = "::";
			break;
		default:
			return ls_set_errno(LS_INVALID_ARGUMENT);
		}
	}

	snprintf(service, sizeof(service), "%hu", port);

	if (getaddrinfo(host, service, NULL, &pInfo) != 0)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	for (pPtr = pInfo; pPtr; pPtr = pPtr->ai_next)
	{
		switch (pPtr->ai_family)
		{
		default:
		case AF_UNSPEC:
			break;
		case AF_INET:
		case AF_IPX:
		case AF_APPLETALK:
		case AF_NETBIOS:
		case AF_INET6:
		case AF_IRDA:
		case AF_BTH:
			if (af != LS_AF_UNSPEC && pPtr->ai_family != af)
				continue; // skip this address

			memcpy(pAddr, pPtr->ai_addr, pPtr->ai_addrlen);
			freeaddrinfo(pInfo);
			return 0;
		}
	}

	freeaddrinfo(pInfo);
	return ls_set_errno(LS_NOT_FOUND);
}

#endif // LS_WINDOWS

ls_handle ls_net_connect(const char *host, unsigned short port, int type, int protocol, int addr_family)
{
#if LS_WINDOWS
	ls_socket_t *sock;
	SOCKADDR_STORAGE addr;
	int rc;

	switch (type)
	{
	default:
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	case LS_NET_STREAM:
		type = SOCK_STREAM;
		break;
	case LS_NET_DGRAM:
		type = SOCK_DGRAM;
		break;
	}

	switch (protocol)
	{
	default:
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	case LS_NET_PROTO_TCP:
		protocol = IPPROTO_TCP;
		break;
	case LS_NET_PROTO_UDP:
		protocol = IPPROTO_UDP;
		break;
	}

	if (ls_check_wsa() != 0)
		return NULL;

	sock = ls_handle_create(&SocketClass, 0);
	if (!sock)
		return NULL;

	rc = ls_parse_sockaddr(host, port, addr_family, &addr);
	if (rc == -1)
	{
		ls_handle_dealloc(sock);
		return NULL;
	}

	sock->socket = socket(addr.ss_family, type, protocol);
	if (sock->socket == INVALID_SOCKET)
	{
		ls_set_errno(LS_IO_ERROR);
		ls_handle_dealloc(sock);
		return NULL;
	}

	if (connect(sock->socket, (PSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		ls_set_errno(LS_IO_ERROR);
		closesocket(sock->socket);
		ls_handle_dealloc(sock);
		return NULL;
	}

	sock->host = ls_strdup(host);
	if (!sock->host)
	{
		closesocket(sock->socket);
		ls_handle_dealloc(sock);
		return NULL;
	}

	sock->port = port;

	sock->can_recv = 1;
	sock->can_send = 1;

	return sock;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

ls_handle ls_net_listen(const char *host, unsigned short port, int type, int protocol, int addr_family, int backlog)
{
#if LS_WINDOWS
	ls_server_t *sock;
	SOCKADDR_STORAGE addr;
	int rc;

	switch (type)
	{
	default:
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	case LS_NET_STREAM:
		type = SOCK_STREAM;
		break;
	case LS_NET_DGRAM:
		type = SOCK_DGRAM;
		break;
	}

	switch (protocol)
	{
	default:
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	case LS_NET_PROTO_TCP:
		protocol = IPPROTO_TCP;
		break;
	case LS_NET_PROTO_UDP:
		protocol = IPPROTO_UDP;
		break;
	}

	if (ls_check_wsa() != 0)
		return NULL;

	sock = ls_handle_create(&ServerClass, 0);
	if (!sock)
		return NULL;

	rc = ls_parse_sockaddr(host, port, addr_family, &addr);
	if (rc == -1)
	{
		ls_handle_dealloc(sock);
		return NULL;
	}
	
	sock->socket = socket(addr.ss_family, type, protocol);
	if (sock->socket == INVALID_SOCKET)
	{
		ls_handle_dealloc(sock);
		ls_set_errno(LS_IO_ERROR);
		return NULL;
	}

	if (bind(sock->socket, (PSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		closesocket(sock->socket);
		ls_handle_dealloc(sock);
		ls_set_errno(LS_IO_ERROR);
		return NULL;
	}

	if (listen(sock->socket, backlog) == SOCKET_ERROR)
	{
		closesocket(sock->socket);
		ls_handle_dealloc(sock);
		ls_set_errno(LS_IO_ERROR);
		return NULL;
	}

	sock->port = port;

	return sock;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

ls_handle ls_net_accept(ls_handle sock)
{
#if LS_WINDOWS
	ls_server_t *server = sock;
	ls_socket_t *client;

	union
	{
		SOCKADDR_STORAGE addr;
		SOCKADDR_IN addr4;
		SOCKADDR_IN6 addr6;
	} u;

	PSOCKADDR pAddr = (PSOCKADDR)&u.addr;
	int addr_len = sizeof(u.addr);

	if (!sock)
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	client = ls_handle_create(&SocketClass, 0);
	if (!client)
		return NULL;

	client->socket = accept(server->socket, pAddr, &addr_len);
	if (client->socket == INVALID_SOCKET)
	{
		ls_handle_dealloc(client);
		ls_set_errno(LS_IO_ERROR);
		return NULL;
	}

	switch (pAddr->sa_family)
	{
	default:
		break;
	case AF_INET:
		client->port = ntohs(u.addr4.sin_port);
		client->host = ls_malloc(INET_ADDRSTRLEN);
		if (client->host)
		{
			snprintf(client->host, INET_ADDRSTRLEN, "%hu.%hu.%hu.%hu",
				u.addr4.sin_addr.S_un.S_un_b.s_b1,
				u.addr4.sin_addr.S_un.S_un_b.s_b2,
				u.addr4.sin_addr.S_un.S_un_b.s_b3,
				u.addr4.sin_addr.S_un.S_un_b.s_b4);
		}
		break;
	case AF_INET6:
		client->port = ntohs(u.addr6.sin6_port);
		client->host = NULL;
		break;
	}

	client->can_recv = 1;
	client->can_send = 1;

	return client;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

int ls_net_shutdown(ls_handle sock, int how)
{
#if LS_WINDOWS
	ls_socket_t *socket = sock;

	if (ls_type_check(sock, LS_SOCKET))
		return -1;

	switch (how)
	{
	default:
		return ls_set_errno(LS_INVALID_ARGUMENT);
	case LS_NET_SHUT_RECV:
		if (!socket->can_recv)
			return 0;
		how = SD_RECEIVE;
		break;
	case LS_NET_SHUT_SEND:
		if (!socket->can_send)
			return 0;
		how = SD_SEND;
		break;
	case LS_NET_SHUT_BOTH:
		if (!socket->can_recv && !socket->can_send)
			return 0;
		how = SD_BOTH;
		break;
	}

	if (shutdown(socket->socket, how) == SOCKET_ERROR)
		return ls_set_errno(LS_IO_ERROR);

	switch (how)
	{
	case SD_RECEIVE:
		socket->can_recv = 0;
		break;
	case SD_SEND:
		socket->can_send = 0;
		break;
	case SD_BOTH:
		socket->can_recv = 0;
		socket->can_send = 0;
		break;
	}

	return 0;
#else
	if (ls_type_check(sock, LS_SOCKET))
		return -1;
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}

const char *ls_net_gethost(ls_handle sock)
{
	ls_socket_t *socket = sock;

	if (ls_type_check(sock, LS_SOCKET))
		return NULL;

	ls_set_errno(LS_SUCCESS);
	return socket->host;
}

unsigned short ls_net_getport(ls_handle sock)
{
	ls_socket_t *socket = sock;

	if (ls_type_check(sock, LS_SOCKET))
		return 0;

	ls_set_errno(LS_SUCCESS);
	return socket->port;
}

size_t ls_net_recv(ls_handle sock, void *buffer, size_t size)
{
#if LS_WINDOWS
	ls_socket_t *socket = sock;
	int to_read, rc;
	size_t remain;
	char *buf;

	if (ls_type_check(sock, LS_SOCKET))
		return -1;

	if (!socket->can_recv)
		return ls_set_errno(LS_ACCESS_DENIED);

	remain = size;
	buf = buffer;

	while (remain != 0)
	{
		to_read = remain > INT_MAX ? INT_MAX : (int)remain;

		rc = recv(socket->socket, buf, to_read, 0);
		if (rc == SOCKET_ERROR)
			return ls_set_errno(LS_IO_ERROR);

		if (rc == 0)
			break;

		remain -= rc;
		buf += rc;
	}

	return size - remain;
#else
	if (ls_type_check(sock, LS_SOCKET))
		return -1;
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}

size_t ls_net_send(ls_handle sock, const void *buffer, size_t size)
{
#if LS_WINDOWS
	ls_socket_t *socket = sock;
	int to_write, rc;
	size_t remain;
	const char *buf;

	if (ls_type_check(sock, LS_SOCKET))
		return -1;

	if (!socket->can_send)
		return ls_set_errno(LS_ACCESS_DENIED);

	remain = size;
	buf = buffer;

	while (remain != 0)
	{
		to_write = remain > INT_MAX ? INT_MAX : (int)remain;

		rc = send(socket->socket, buf, to_write, 0);
		if (rc == SOCKET_ERROR)
			return ls_set_errno(LS_IO_ERROR);

		if (rc == 0)
			break;

		remain -= rc;
		buf += rc;
	}

	return size - remain;
#else
	if (ls_type_check(sock, LS_SOCKET))
		return -1;
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}
