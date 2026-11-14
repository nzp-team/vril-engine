/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
#include "../../nzportable_def.h"
#include "net_udp.h"
#include <network/network.h>
#include <lwip/udp.h>
#include <lwip/dns.h>
#include <lwip/inet.h>

#define SOCKET_COUNT 8
#define PACKET_COUNT 16

typedef struct {
    uint16_t family, port;
    uint32_t ip;
    byte padding[8];
} xenon_address_t;

typedef struct {
    struct udp_pcb * pcb;
    struct pbuf * packets[PACKET_COUNT];
    xenon_address_t addresses[PACKET_COUNT];
    unsigned read, count;
} xenon_socket_t;

static xenon_socket_t sockets[SOCKET_COUNT];
static int control_socket = -1, accept_socket = -1, network_ready;
static struct qsockaddr broadcast_address;
typedef char address_size_check[sizeof(xenon_address_t) == sizeof(struct qsockaddr) ? 1 : -1];

void
X360_NetworkPoll(void)
{
    if (network_ready)
        network_poll();
}

static void
Address(struct qsockaddr * addr, uint32_t ip, int port)
{
    xenon_address_t * value = (xenon_address_t *) addr;

    memset(addr, 0, sizeof(*addr));
    value->family = 2;
    value->port = htons(port);
    value->ip = ip;
}

static void
Receive(void * context, struct udp_pcb * pcb, struct pbuf * packet, ip_addr_t * ip, u16_t port)
{
    xenon_socket_t * socket = context;
    unsigned index = (socket->read + socket->count) % PACKET_COUNT;

    (void) pcb;
    if (socket->count == PACKET_COUNT || packet->tot_len > NET_DATAGRAMSIZE) {
        pbuf_free(packet);
        return;
    }
    socket->packets[index] = packet;
    Address((struct qsockaddr *) &socket->addresses[index], ip->addr, port);
    ++socket->count;
}

int
UDP_OpenSocket(int port)
{
    int i;

    for (i = 0; i < SOCKET_COUNT; ++i) {
        xenon_socket_t * socket = &sockets[i];

        if (socket->pcb)
            continue;
        socket->pcb = udp_new();
        if (!socket->pcb)
            return -1;
        if (udp_bind(socket->pcb, IP_ADDR_ANY, port) != ERR_OK) {
            udp_remove(socket->pcb);
            socket->pcb = NULL;
            return -1;
        }
        socket->pcb->so_options |= SOF_BROADCAST;
        udp_recv(socket->pcb, Receive, socket);
        return i;
    }
    return -1;
}

int
UDP_CloseSocket(int handle)
{
    xenon_socket_t * socket;
    unsigned i;

    if (handle < 0 || handle >= SOCKET_COUNT || !sockets[handle].pcb)
        return -1;
    socket = &sockets[handle];
    udp_remove(socket->pcb);
    for (i = 0; i < socket->count; ++i)
        pbuf_free(socket->packets[(socket->read + i) % PACKET_COUNT]);
    memset(socket, 0, sizeof(*socket));
    return 0;
}

int
UDP_Init(void)
{
    if (COM_CheckParm("-noudp"))
        return -1;
    if (!network_ready) {
        int result = network_init();

        network_ready = result >= 0;
        if (result != NETWORK_INIT_SUCCESS)
            return -1;
    }
    control_socket = UDP_OpenSocket(0);
    if (control_socket < 0)
        return -1;
    Address(&broadcast_address, 0xffffffff, net_hostport);
    Q_strlcpy(my_tcpip_address, ipaddr_ntoa(&netif.ip_addr), sizeof(my_tcpip_address));
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
    int i;

    for (i = 0; i < SOCKET_COUNT; ++i) {
        if (sockets[i].pcb)
            UDP_CloseSocket(i);
    }
    control_socket = accept_socket = -1;
    tcpipAvailable = false;
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
    X360_NetworkPoll();
    return accept_socket >= 0 && sockets[accept_socket].count ? accept_socket : -1;
}

int
UDP_Read(int handle, byte * buffer, int length, struct qsockaddr * addr)
{
    xenon_socket_t * socket;
    struct pbuf * packet;
    int result;

    X360_NetworkPoll();
    if (handle < 0 || handle >= SOCKET_COUNT || !sockets[handle].pcb)
        return -1;
    socket = &sockets[handle];
    if (!socket->count)
        return 0;
    packet = socket->packets[socket->read];
    memcpy(addr, &socket->addresses[socket->read], sizeof(*addr));
    result = packet->tot_len <= length ? pbuf_copy_partial(packet, buffer, packet->tot_len, 0) : -1;
    pbuf_free(packet);
    socket->read = (socket->read + 1) % PACKET_COUNT;
    --socket->count;
    return result;
}

int
UDP_Write(int handle, byte * buffer, int length, struct qsockaddr * addr)
{
    xenon_address_t * address = (xenon_address_t *) addr;
    ip_addr_t ip = { address->ip };
    struct pbuf * packet;
    err_t result;

    if (handle < 0 || handle >= SOCKET_COUNT || !sockets[handle].pcb || length < 0 || length > NET_DATAGRAMSIZE)
        return -1;
    packet = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
    if (!packet)
        return 0;
    pbuf_take(packet, buffer, length);
    result = udp_sendto(sockets[handle].pcb, packet, &ip, ntohs(address->port));
    pbuf_free(packet);
    X360_NetworkPoll();
    return result == ERR_OK ? length : result == ERR_MEM ? 0 : -1;
}

int
UDP_Broadcast(int socket, byte * buffer, int length)
{
    UDP_SetSocketPort(&broadcast_address, net_hostport);
    return UDP_Write(socket, buffer, length, &broadcast_address);
}

char *
UDP_AddrToString(struct qsockaddr * addr)
{
    static char text[32];
    xenon_address_t * address = (xenon_address_t *) addr;
    uint32_t ip = ntohl(address->ip);

    snprintf(text, sizeof(text), "%u.%u.%u.%u:%u", ip >> 24, (ip >> 16) & 255,
      (ip >> 8) & 255, ip & 255, ntohs(address->port));
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
    Address(addr, htonl((a << 24) | (b << 16) | (c << 8) | d), port);
    return 0;
}

int
UDP_GetSocketAddr(int handle, struct qsockaddr * addr)
{
    if (handle < 0 || handle >= SOCKET_COUNT || !sockets[handle].pcb)
        return -1;
    Address(addr, netif.ip_addr.addr, sockets[handle].pcb->local_port);
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
    ip_addr_t ip;

    if (strchr(name, ':'))
        return UDP_StringToAddr(name, addr);
    if (!ipaddr_aton(name, &ip))
        return -1;
    Address(addr, ip.addr, net_hostport);
    return 0;
}

int
UDP_AddrCompare(struct qsockaddr * first, struct qsockaddr * second)
{
    xenon_address_t * a = (xenon_address_t *) first, * b = (xenon_address_t *) second;

    if (a->family != b->family || a->ip != b->ip)
        return -1;
    return a->port == b->port ? 0 : 1;
}

int
UDP_GetSocketPort(struct qsockaddr * addr)
{
    return ntohs(((xenon_address_t *) addr)->port);
}

int
UDP_SetSocketPort(struct qsockaddr * addr, int port)
{
    ((xenon_address_t *) addr)->port = htons(port);
    return 0;
}
