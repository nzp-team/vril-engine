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

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "../../nzportable_def.h"
#include "net_udp.h"

#if !defined(_WIN32)
#include <net/net.h>
#include <net/netctl.h>
#include <sysmodule/sysmodule.h>
#include <netinet/in.h>

#endif

#include <sys/fcntl.h>
#include <unistd.h>

extern cvar_t hostname;

static int net_acceptsocket = -1;
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static uint32_t myAddr;

#include "net_udp.h"

int
UDP_Init(void)
{
    #if defined(_WIN32)
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return -1;

    #endif

    struct qsockaddr addr;
    char * colon;

    if ((sysModuleIsLoaded(SYSMODULE_NET) != 0 && sysModuleLoad(SYSMODULE_NET) != 0) ||
      (sysModuleIsLoaded(SYSMODULE_NETCTL) != 0 && sysModuleLoad(SYSMODULE_NETCTL) != 0))
        return -1;

    if (netInitialize() != 0)
        return -1;

    if (netCtlInit() != 0) {
        netDeinitialize();
        return -1;
    }

    if (COM_CheckParm("-noudp")) {
        netCtlTerm();
        netDeinitialize();
        return -1;
    }

    #if defined(_WIN32)
    {
        char hostname[256];
        struct net_hostent * hostentry;

        gethostname(hostname, sizeof(hostname));

        hostentry = netGetHostByName(hostname);

        if (hostentry && hostentry->h_addr_list[0])
            myAddr = *(unsigned long *) hostentry->h_addr_list[0];
        else
            myAddr = inet_addr("127.0.0.1");
    }
    #else
    union net_ctl_info info;

    if (netCtlGetInfo(NET_CTL_INFO_IP_ADDRESS, &info) == 0)
        myAddr = inet_addr(info.ip_address);
    else
        myAddr = inet_addr("127.0.0.1");

    #endif

    if (strcmp(hostname.string, "UNNAMED") == 0) {
        char local_hostname[NET_NAMELEN] = "localhost";
        Q_strcpy(local_hostname, "PS3");
        Cvar_Set("hostname", local_hostname);
    }

    if ((net_controlsocket = UDP_OpenSocket(0)) == -1) {
        netCtlTerm();
        netDeinitialize();
        return -1;
    }

    ((struct sockaddr_in *) &broadcastaddr)->sin_len         = sizeof(broadcastaddr);
    ((struct sockaddr_in *) &broadcastaddr)->sin_family      = AF_INET;
    ((struct sockaddr_in *) &broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
    ((struct sockaddr_in *) &broadcastaddr)->sin_port        = htons(net_hostport);

    UDP_GetSocketAddr(net_controlsocket, &addr);
    Q_strcpy(my_tcpip_address, UDP_AddrToString(&addr));
    colon = Q_strrchr(my_tcpip_address, ':');

    if (colon)
        *colon = 0;

    Con_Printf("UDP Initialized\n");
    tcpipAvailable = true;

    return net_controlsocket;
}

void
UDP_Shutdown(void)
{
    UDP_Listen(false);
    UDP_CloseSocket(net_controlsocket);
    netCtlTerm();
    netDeinitialize();
}

void
UDP_Listen(qboolean state)
{
    if (state) {
        if (net_acceptsocket != -1)
            return;

        if ((net_acceptsocket = UDP_OpenSocket(net_hostport)) == -1)
            Sys_Error("Unable to open accept socket\n");

        return;
    }

    if (net_acceptsocket == -1)
        return;

    UDP_CloseSocket(net_acceptsocket);
    net_acceptsocket = -1;
}

int
UDP_OpenSocket(int port)
{
    int newsocket;
    struct sockaddr_in address = { 0 };

    if ((newsocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        return -1;

    #if defined(_WIN32)
    u_long mode = 1;

    if (ioctlsocket(newsocket, FIONBIO, &mode) != 0)
        goto ErrorReturn;

    #else
    int enabled = 1;

    if (setsockopt(newsocket, SOL_SOCKET, SO_NBIO, &enabled, sizeof(enabled)) != 0)
        goto ErrorReturn;

    #endif

    address.sin_len         = sizeof(address);
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port        = htons(port);

    if (bind(newsocket, (void *) &address, sizeof(address)) == -1)
        goto ErrorReturn;

    return newsocket;

ErrorReturn:
    #if defined(_WIN32)
    closesocket(newsocket);
    #else
    netClose(newsocket);
    #endif
    return -1;
}

int
UDP_CloseSocket(int socket)
{
    if (socket == net_broadcastsocket)
        net_broadcastsocket = 0;

    return netClose(socket);
}

static int
PartialIPAddress(char * in, struct qsockaddr * hostaddr)
{
    char buff[256];
    char * b;
    int addr;
    int num;
    int mask;
    int run;
    int port;

    buff[0] = '.';
    b       = buff;
    strcpy(buff + 1, in);

    if (buff[1] == '.')
        b++;

    addr = 0;
    mask = -1;

    while (*b == '.') {
        b++;
        num = 0;
        run = 0;

        while (!(*b < '0' || *b > '9')) {
            num = num * 10 + *b++ - '0';

            if (++run > 3)
                return -1;
        }

        if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
            return -1;

        if (num < 0 || num > 255)
            return -1;

        mask <<= 8;
        addr   = (addr << 8) + num;
    }

    if (*b++ == ':')
        port = Q_atoi(b);
    else
        port = net_hostport;

    hostaddr->sa_len    = sizeof(*hostaddr);
    hostaddr->sa_family = AF_INET;
    ((struct sockaddr_in *) hostaddr)->sin_port        = htons((short) port);
    ((struct sockaddr_in *) hostaddr)->sin_addr.s_addr = (myAddr & htonl(mask)) | htonl(addr);

    return 0;
}

int
UDP_Connect(int socket, struct qsockaddr * addr)
{
    return 0;
}

int
UDP_CheckNewConnections(void)
{
    char buf[4096];

    if (net_acceptsocket == -1)
        return -1;

    if (recvfrom(net_acceptsocket, buf, 4096, MSG_PEEK, NULL, NULL) > 0)
        return net_acceptsocket;

    return -1;
}

int
UDP_Read(int socket, byte * buf, int len, struct qsockaddr * addr)
{
    int addrlen = sizeof(struct qsockaddr);
    int ret;

    ret = recvfrom(socket, (char *) buf, len, 0, (struct sockaddr *) addr, (socklen_t *) &addrlen);

    if (ret == -1)
        return 0;

    return ret;
}

int
UDP_MakeSocketBroadcastCapable(int socket)
{
    int enabled = 1;

    #if defined(_WIN32)

    if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (const char *) &enabled, sizeof(enabled)) ==
      SOCKET_ERROR)
        return -1;

    #else

    if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) == -1)
        return -1;

    #endif
    net_broadcastsocket = socket;
    return 0;
}

int
UDP_Broadcast(int socket, byte * buf, int len)
{
    int ret;

    if (socket != net_broadcastsocket) {
        if (net_broadcastsocket != 0)
            Sys_Error("Attempted to use multiple broadcasts sockets\n");

        ret = UDP_MakeSocketBroadcastCapable(socket);

        if (ret == -1) {
            Con_Printf("Unable to make socket broadcast capable\n");
            return ret;
        }
    }

    return UDP_Write(socket, buf, len, &broadcastaddr);
}

int
UDP_Write(int socket, byte * buf, int len, struct qsockaddr * addr)
{
    int ret;

    ret = sendto(socket, (const char *) buf, len, 0, (struct sockaddr *) addr,
        sizeof(struct qsockaddr));

    if (ret == -1)
        return 0;

    return ret;
}

char *
UDP_AddrToString(struct qsockaddr * addr)
{
    static char buffer[22];
    int haddr;

    haddr = ntohl(((struct sockaddr_in *) addr)->sin_addr.s_addr);
    sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff,
      (haddr >> 8) & 0xff, haddr & 0xff, ntohs(((struct sockaddr_in *) addr)->sin_port));
    return buffer;
}

int
UDP_StringToAddr(char * string, struct qsockaddr * addr)
{
    int ha1, ha2, ha3, ha4, hp;
    int ipaddr;

    sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
    ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

    addr->sa_len    = sizeof(*addr);
    addr->sa_family = AF_INET;
    ((struct sockaddr_in *) addr)->sin_addr.s_addr = htonl(ipaddr);
    ((struct sockaddr_in *) addr)->sin_port        = htons(hp);
    return 0;
}

int
UDP_GetSocketAddr(int socket, struct qsockaddr * addr)
{
    socklen_t addrlen = sizeof(struct qsockaddr);
    unsigned int a;

    Q_memset(addr, 0, sizeof(struct qsockaddr));
    getsockname(socket, (struct sockaddr *) addr, (socklen_t *) &addrlen);
    a = ((struct sockaddr_in *) addr)->sin_addr.s_addr;

    if (a == 0 || a == inet_addr("127.0.0.1"))
        ((struct sockaddr_in *) addr)->sin_addr.s_addr = myAddr;

    return 0;
}

int
UDP_GetNameFromAddr(struct qsockaddr * addr, char * name)
{
    struct net_hostent * hostentry;

    hostentry = netGetHostByAddr((char *) &((struct sockaddr_in *) addr)->sin_addr,
        sizeof(struct in_addr), AF_INET);

    if (hostentry) {
        Q_strncpy(name, (char *) (uintptr_t) hostentry->h_name, NET_NAMELEN - 1);
        return 0;
    }

    Q_strcpy(name, UDP_AddrToString(addr));
    return 0;
}

int
UDP_GetAddrFromName(char * name, struct qsockaddr * addr)
{
    struct net_hostent * hostentry;

    if (name[0] >= '0' && name[0] <= '9')
        return PartialIPAddress(name, addr);

    hostentry = netGetHostByName(name);

    if (!hostentry)
        return -1;

    addr->sa_len    = sizeof(*addr);
    addr->sa_family = AF_INET;
    ((struct sockaddr_in *) addr)->sin_port        = htons(net_hostport);
    ((struct sockaddr_in *) addr)->sin_addr.s_addr =
      *(uint32_t *) (uintptr_t) *(uint32_t *) (uintptr_t) hostentry->h_addr_list;

    return 0;
}

int
UDP_AddrCompare(struct qsockaddr * addr1, struct qsockaddr * addr2)
{
    if (addr1->sa_family != addr2->sa_family)
        return -1;

    if (((struct sockaddr_in *) addr1)->sin_addr.s_addr !=
      ((struct sockaddr_in *) addr2)->sin_addr.s_addr)
        return -1;

    if (((struct sockaddr_in *) addr1)->sin_port != ((struct sockaddr_in *) addr2)->sin_port)
        return 1;

    return 0;
}

int
UDP_GetSocketPort(struct qsockaddr * addr)
{
    return ntohs(((struct sockaddr_in *) addr)->sin_port);
}

int
UDP_SetSocketPort(struct qsockaddr * addr, int port)
{
    ((struct sockaddr_in *) addr)->sin_port = htons(port);
    return 0;
}
