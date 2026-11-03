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
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/Sysmodule.h>
#include "ps4_local.h"

#define SOL_SOCKET_PS4   0xffff
#define SO_NBIO_PS4      0x1200
#define SO_BROADCAST_PS4 0x20
#define MSG_PEEK_PS4     2

typedef struct {
    unsigned char length, family;
    uint16_t      port;
    uint32_t      address;
    unsigned char padding[8];
} ps4_address_t;
typedef char address_size_check[sizeof(ps4_address_t) == sizeof(struct qsockaddr) ? 1 : -1];
static int control_socket = -1, accept_socket = -1;
static uint32_t local_address;
static struct qsockaddr broadcast_address;

static void
Address(struct qsockaddr * addr, uint32_t ip, int port)
{
    ps4_address_t * value = (ps4_address_t *) addr;

    memset(addr, 0, sizeof(*addr));
    value->length  = sizeof(*addr);
    value->family  = ORBIS_NET_AF_INET;
    value->port    = sceNetHtons(port);
    value->address = ip;
}

int
UDP_OpenSocket(int port)
{
    int enabled = 1;
    int socket  = sceNetSocket("vril UDP", ORBIS_NET_AF_INET, ORBIS_NET_SOCK_DGRAM, 0);
    struct qsockaddr addr;

    if (socket < 0)
        return -1;

    Address(&addr, 0, port);

    if (sceNetSetsockopt(socket, SOL_SOCKET_PS4, SO_NBIO_PS4, &enabled, sizeof(enabled)) < 0 ||
      sceNetBind(socket, (OrbisNetSockaddr *) &addr, sizeof(addr)) < 0)
    {
        sceNetSocketClose(socket);

        return -1;
    }

    return socket;
}

int
UDP_CloseSocket(int socket)
{
    return sceNetSocketClose(socket);
}

int
UDP_Init(void)
{
    OrbisNetCtlInfo info;

    if (COM_CheckParm("-noudp"))
        return -1;

    // Imports are not callable until the corresponding PRXs are loaded.
    if (PS4_LoadModule(ORBIS_SYSMODULE_INTERNAL_NET) < 0 ||
      PS4_LoadModule(ORBIS_SYSMODULE_INTERNAL_NETCTL) < 0)
        return -1;

    int result = sceNetInit();

    if (result < 0)
        return -1;

    result = sceNetCtlInit();

    if (result < 0)
        return -1;

    if (sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info) < 0 ||
      sceNetInetPton(ORBIS_NET_AF_INET, info.ip_address, &local_address) != 1)
        return -1;

    control_socket = UDP_OpenSocket(0);

    if (control_socket < 0)
        return -1;

    Address(&broadcast_address, 0xffffffff, net_hostport);
    Q_strlcpy(my_tcpip_address, info.ip_address, sizeof(my_tcpip_address));
    tcpipAvailable = true;

    return control_socket;
}

void
UDP_Listen(qboolean enabled)
{
    if (enabled && accept_socket < 0)
        accept_socket = UDP_OpenSocket(net_hostport);

    if (!enabled && accept_socket >= 0) {
        UDP_CloseSocket(accept_socket);
        accept_socket = -1;
    }
}

void
UDP_Shutdown(void)
{
    UDP_Listen(false);

    if (control_socket >= 0)
        UDP_CloseSocket(control_socket);

    control_socket = -1;
}

int
UDP_Connect(int socket, struct qsockaddr * addr)
{
    (void) socket;
    (void) addr;

    return 0;
}

int
UDP_CheckNewConnections(void)
{
    byte data;

    return accept_socket >= 0 && sceNetRecvfrom(accept_socket, &data, 1, MSG_PEEK_PS4, NULL,
             NULL) > 0 ? accept_socket : -1;
}

int
UDP_Read(int socket, byte * buf, int len, struct qsockaddr * addr)
{
    OrbisNetSocklen_t size = sizeof(*addr);
    int result = sceNetRecvfrom(socket, buf, len, 0, (OrbisNetSockaddr *) addr, &size);

    return result < 0 ? 0 : result;
}

int
UDP_Write(int socket, byte * buf, int len, struct qsockaddr * addr)
{
    int result = sceNetSendto(socket, buf, len, 0, (OrbisNetSockaddr *) addr, sizeof(*addr));

    return result < 0 ? 0 : result;
}

int
UDP_Broadcast(int socket, byte * buf, int len)
{
    int enabled = 1;

    if (sceNetSetsockopt(socket, SOL_SOCKET_PS4, SO_BROADCAST_PS4, &enabled, sizeof(enabled)) < 0)
        return -1;

    UDP_SetSocketPort(&broadcast_address, net_hostport);

    return UDP_Write(socket, buf, len, &broadcast_address);
}

char *
UDP_AddrToString(struct qsockaddr * addr)
{
    static char text[32];
    ps4_address_t * value = (ps4_address_t *) addr;
    uint32_t ip = sceNetNtohl(value->address);

    snprintf(text, sizeof(text), "%u.%u.%u.%u:%u", ip >> 24, (ip >> 16) & 255,
      (ip >> 8) & 255, ip & 255, sceNetNtohs(value->port));

    return text;
}

int
UDP_StringToAddr(char * text, struct qsockaddr * addr)
{
    unsigned a, b, c, d, port;
    char extra;

    if (sscanf(text, "%u.%u.%u.%u:%u%c", &a, &b, &c, &d, &port, &extra) != 5 ||
      a > 255 || b > 255 || c > 255 || d > 255 || port > 65535)
        return -1;

    Address(addr, sceNetHtonl((a << 24) | (b << 16) | (c << 8) | d), port);

    return 0;
}

int
UDP_GetSocketAddr(int socket, struct qsockaddr * addr)
{
    OrbisNetSocklen_t length = sizeof(*addr);
    ps4_address_t * value    = (ps4_address_t *) addr;

    if (sceNetGetsockname(socket, (OrbisNetSockaddr *) addr, &length) < 0)
        return -1;

    if (!value->address)
        value->address = local_address;

    return 0;
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
    OrbisNetInAddr ip;
    int resolver, result;

    if (strchr(name, ':'))
        return UDP_StringToAddr(name, addr);

    if (sceNetInetPton(ORBIS_NET_AF_INET, name, &ip) == 1) {
        Address(addr, ip.s_addr, net_hostport);

        return 0;
    }

    resolver = sceNetResolverCreate("vril DNS", 0, 0);

    if (resolver < 0)
        return -1;

    result = sceNetResolverStartNtoa(resolver, name, &ip, 0, 0, 0);
    sceNetResolverDestroy(resolver);

    if (result < 0)
        return -1;

    Address(addr, ip.s_addr, net_hostport);

    return 0;
}

int
UDP_AddrCompare(struct qsockaddr * a, struct qsockaddr * b)
{
    ps4_address_t * left = (ps4_address_t *) a, * right = (ps4_address_t *) b;

    if (left->family != right->family || left->address != right->address)
        return -1;

    return left->port == right->port ? 0 : 1;
}

int
UDP_GetSocketPort(struct qsockaddr * addr)
{
    return sceNetNtohs(((ps4_address_t *) addr)->port);
}

int
UDP_SetSocketPort(struct qsockaddr * addr, int port)
{
    ((ps4_address_t *) addr)->port = sceNetHtons(port);

    return 0;
}
