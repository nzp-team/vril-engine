/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "../../nzportable_def.h"
#include "net_udp.h"
#include <nxdk/net.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/netif.h>
#include <errno.h>

static int control_socket = -1, accept_socket = -1;
static uint32_t local_address;
static struct qsockaddr broadcast_address;
extern struct netif * g_pnetif;
typedef char address_size_check[sizeof(struct sockaddr_in) == sizeof(struct qsockaddr) ? 1 : -1];

static void
Address(struct qsockaddr * addr, uint32_t ip, int port)
{
    struct sockaddr_in * value = (struct sockaddr_in *) addr;

    memset(addr, 0, sizeof(*addr));
    value->sin_len         = sizeof(*value);
    value->sin_family      = AF_INET;
    value->sin_addr.s_addr = ip;
    value->sin_port        = htons(port);
}

int
UDP_OpenSocket(int port)
{
    int enabled = 1;
    int handle  = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct qsockaddr address;

    if (handle < 0)
        return -1;

    Address(&address, INADDR_ANY, port);
    if (lwip_ioctl(handle, FIONBIO, &enabled) < 0 ||
      lwip_bind(handle, (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        lwip_close(handle);
        return -1;
    }
    return handle;
}

int UDP_CloseSocket(int handle){ return lwip_close(handle); }

int UDP_Connect(int handle, struct qsockaddr * addr){ (void) handle; (void) addr; return 0; }

int
UDP_Init(void)
{
    extern cvar_t hostname;
    int enabled = 1;

    if (COM_CheckParm("-noudp"))
        return -1;

    if (nxNetInit(NULL) < 0) {
        Con_Printf("Xbox networking unavailable.\n");
        return -1;
    }
    local_address  = netif_ip4_addr(g_pnetif)->addr;
    control_socket = UDP_OpenSocket(0);
    if (control_socket < 0) {
        nxNetShutdown();
        return -1;
    }
    lwip_setsockopt(control_socket, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));
    Address(&broadcast_address, INADDR_BROADCAST, net_hostport);
    Q_strlcpy(my_tcpip_address, ip4addr_ntoa(netif_ip4_addr(g_pnetif)), sizeof(my_tcpip_address));
    if (!strcmp(hostname.string, "UNNAMED"))
        Cvar_Set("hostname", "Xbox");
    tcpipAvailable = true;
    Con_Printf("Xbox UDP initialized: %s\n", my_tcpip_address);
    return control_socket;
}

void
UDP_Listen(qboolean state)
{
    if (state && accept_socket < 0) {
        accept_socket = UDP_OpenSocket(net_hostport);
        if (accept_socket < 0)
            Sys_Error("UDP: could not open port %d", net_hostport);
    } else if (!state && accept_socket >= 0) {
        UDP_CloseSocket(accept_socket);
        accept_socket = -1;
    }
}

void
UDP_Shutdown(void)
{
    UDP_Listen(false);
    if (control_socket >= 0) {
        UDP_CloseSocket(control_socket);
        control_socket = -1;
        nxNetShutdown();
    }
    tcpipAvailable = false;
}

int
UDP_CheckNewConnections(void)
{
    unsigned long bytes = 0;

    return accept_socket >= 0 && lwip_ioctl(accept_socket, FIONREAD, &bytes) == 0 && bytes ? accept_socket : -1;
}

int
UDP_Read(int handle, byte * buffer, int length, struct qsockaddr * addr)
{
    socklen_t size = sizeof(*addr);
    int result     = lwip_recvfrom(handle, buffer, length, 0, (struct sockaddr *) addr, &size);

    return result < 0 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == ECONNREFUSED) ? 0 : result;
}

int
UDP_Write(int handle, byte * buffer, int length, struct qsockaddr * addr)
{
    int result = lwip_sendto(handle, buffer, length, 0, (struct sockaddr *) addr, sizeof(*addr));

    return result < 0 && (errno == EWOULDBLOCK || errno == EAGAIN) ? 0 : result;
}

int
UDP_Broadcast(int handle, byte * buffer, int length)
{
    int enabled = 1;

    if (lwip_setsockopt(handle, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) < 0)
        return -1;

    UDP_SetSocketPort(&broadcast_address, net_hostport);
    return UDP_Write(handle, buffer, length, &broadcast_address);
}

char *
UDP_AddrToString(struct qsockaddr * addr)
{
    static char text[32];
    struct sockaddr_in * value = (struct sockaddr_in *) addr;

    snprintf(text, sizeof(text), "%s:%u", inet_ntoa(value->sin_addr), ntohs(value->sin_port));
    return text;
}

int
UDP_StringToAddr(char * text, struct qsockaddr * addr)
{
    char host[NET_NAMELEN], * colon;
    struct in_addr ip;
    int port = net_hostport;

    if (Q_strlcpy(host, text, sizeof(host)) >= sizeof(host))
        return -1;

    colon = strchr(host, ':');
    if (colon) {
        char * end;
        long value = strtol(colon + 1, &end, 10);
        if (end == colon + 1 || *end || value < 1 || value > 65535)
            return -1;

        port   = value;
        *colon = '\0';
    }
    if (!inet_aton(host, &ip))
        return -1;

    Address(addr, ip.s_addr, port);
    return 0;
}

int
UDP_GetSocketAddr(int handle, struct qsockaddr * addr)
{
    socklen_t size = sizeof(*addr);
    struct sockaddr_in * value = (struct sockaddr_in *) addr;
    int result = lwip_getsockname(handle, (struct sockaddr *) addr, &size);

    if (!result && (value->sin_addr.s_addr == INADDR_ANY || value->sin_addr.s_addr == htonl(INADDR_LOOPBACK)))
        value->sin_addr.s_addr = local_address;
    return result;
}

int
UDP_GetNameFromAddr(struct qsockaddr * addr, char * name)
{
    Q_strlcpy(name, UDP_AddrToString(addr), NET_NAMELEN);
    return 0;
}

int
UDP_GetAddrFromName(char * name, struct qsockaddr * addr)
{
    struct hostent * host;

    if (!UDP_StringToAddr(name, addr))
        return 0;

    host = lwip_gethostbyname(name);
    if (!host || host->h_addrtype != AF_INET || host->h_length != 4)
        return -1;

    Address(addr, *(uint32_t *) host->h_addr_list[0], net_hostport);
    return 0;
}

int
UDP_AddrCompare(struct qsockaddr * first, struct qsockaddr * second)
{
    struct sockaddr_in * a = (struct sockaddr_in *) first, * b = (struct sockaddr_in *) second;

    if (a->sin_family != b->sin_family || a->sin_addr.s_addr != b->sin_addr.s_addr)
        return -1;

    return a->sin_port == b->sin_port ? 0 : 1;
}

int UDP_GetSocketPort(struct qsockaddr * addr){ return ntohs(((struct sockaddr_in *) addr)->sin_port); }

int
UDP_SetSocketPort(struct qsockaddr * addr, int port)
{
    ((struct sockaddr_in *) addr)->sin_port = htons(port);
    return 0;
}
