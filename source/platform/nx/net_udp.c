/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../nzportable_def.h"
/* socket for fielding new connections */
static int net_acceptsocket = -1;
static int net_controlsocket;
static int net_broadcastsocket = 0;
static netadr_t broadcastaddr;

/*
 * There are three addresses that we may use in different ways:
 *   myAddr    - This is the "default" address returned by the OS
 *   localAddr    - This is an address to advertise in CCREP_SERVER_INFO
 *         and CCREP_ACCEPT response packets, rather than the
 *         default address (sometimes the default address is not
 *         suitable for LAN clients; i.e. loopback address). Set
 *         on the command line using the "-localip" option.
 *   bindAddr    - The address to which we bind our network socket. The
 *         default is INADDR_ANY, but in some cases we may want
 *         to only listen on a particular address. Set on the
 *         command line using the "-ip" option.
 */
static netadr_t myAddr;
static netadr_t localAddr;
static netadr_t bindAddr;
static char ifname[IFNAMSIZ];

static void NetadrToSockadr(const netadr_t *a, struct sockaddr_in *s) {
    memset(s, 0, sizeof(*s));
    s->sin_family = AF_INET;

    s->sin_addr.s_addr = a->ip.l;
    s->sin_port = a->port;
}

static void SockadrToNetadr(const struct sockaddr_in *s, netadr_t *a) {
    a->ip.l = s->sin_addr.s_addr;
    a->port = s->sin_port;
}

static int udp_scan_iface(int sock) {
    struct ifconf ifc;
    struct ifreq *ifr;
    char buf[8192];
    int i, n;
    struct sockaddr_in *iaddr;

    if (COM_CheckParm("-noifscan")) return -1;

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
        Con_Printf("%s: SIOCGIFCONF failed\n", __func__);
        return -1;
    }

    ifr = ifc.ifc_req;
    n = ifc.ifc_len / sizeof(struct ifreq);

    for (i = 0; i < n; i++) {
        if (ioctl(sock, SIOCGIFADDR, &ifr[i]) == -1) continue;
        iaddr = (struct sockaddr_in *)&ifr[i].ifr_addr;
        Con_DPrintf("%s: %s\n", ifr[i].ifr_name, inet_ntoa(iaddr->sin_addr));
        if (iaddr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            SockadrToNetadr(iaddr, &myAddr);
            strcpy(ifname, ifr[i].ifr_name);
            return 0;
        }
    }

    return -1;
}

int UDP_Init(void) {
    int i;
    int err;
    char buff[MAXHOSTNAMELEN];
    char *colon;
    netadr_t addr;

    if (COM_CheckParm("-noudp")) return -1;

    /* determine my name & address, default to loopback */
    myAddr.ip.l = htonl(INADDR_LOOPBACK);
    myAddr.port = htons(DEFAULTnet_hostport);
    err = gethostname(buff, MAXHOSTNAMELEN);
    if (err) {
        Con_Printf("%s: WARNING: gethostname failed (%s)\n", __func__, strerror(errno));
    } else {
        buff[MAXHOSTNAMELEN - 1] = 0;
        myAddr.ip.l = htonl(gethostid());
    }

    i = COM_CheckParm("-ip");
    if (i && i < com_argc - 1) {
        bindAddr.ip.l = inet_addr(com_argv[i + 1]);
        if (bindAddr.ip.l == INADDR_NONE)
            Sys_Error("%s: %s is not a valid IP address", __func__, com_argv[i + 1]);
        Con_Printf("Binding to IP Interface Address of %s\n", com_argv[i + 1]);
    } else {
        bindAddr.ip.l = INADDR_NONE;
    }

    i = COM_CheckParm("-localip");
    if (i && i < com_argc - 1) {
        localAddr.ip.l = inet_addr(com_argv[i + 1]);
        if (localAddr.ip.l == INADDR_NONE)
            Sys_Error("%s: %s is not a valid IP address", __func__, com_argv[i + 1]);
        Con_Printf("Advertising %s as the local IP in response packets\n", com_argv[i + 1]);
    } else {
        localAddr.ip.l = INADDR_NONE;
    }

    net_controlsocket = UDP_OpenSocket(0);
    if (net_controlsocket == -1) {
        Con_Printf("%s: Unable to open control socket, UDP disabled\n", __func__);
        return -1;
    }

    /* myAddr may resolve to 127.0.0.1, see if we can do any better */
    memset(ifname, 0, sizeof(ifname));
    if (myAddr.ip.l == htonl(INADDR_LOOPBACK)) {
        if (udp_scan_iface(net_controlsocket) == 0)
            Con_Printf("UDP, Local address: %s (%s)\n", NET_AdrToString(&myAddr), ifname);
    }
    if (ifname[0] == 0) {
        Con_Printf("UDP, Local address: %s\n", NET_AdrToString(&myAddr));
    }

    broadcastaddr.ip.l = INADDR_BROADCAST;
    broadcastaddr.port = htons(net_hostport);

    UDP_GetSocketAddr(net_controlsocket, &addr);
    strcpy(my_tcpip_address, NET_AdrToString(&addr));
    colon = strrchr(my_tcpip_address, ':');
    if (colon) *colon = 0;

    Con_Printf("UDP Initialized (%s)\n", my_tcpip_address);
    tcpipAvailable = true;

    return net_controlsocket;
}

void UDP_Shutdown(void) {
    UDP_Listen(false);
    UDP_CloseSocket(net_controlsocket);
}

void UDP_Listen(qboolean state) {
    /* enable listening */
    if (state) {
        if (net_acceptsocket != -1) return;
        if ((net_acceptsocket = UDP_OpenSocket(net_hostport)) == -1)
            Sys_Error("%s: Unable to open accept socket", __func__);
        return;
    }
    /* disable listening */
    if (net_acceptsocket == -1) return;
    UDP_CloseSocket(net_acceptsocket);
    net_acceptsocket = -1;
}

int UDP_OpenSocket(int port) {
    int newsocket;
    struct sockaddr_in address;
    int _true = 1;

    if ((newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) return -1;
    if (ioctl(newsocket, FIONBIO, &_true) == -1) goto ErrorReturn;

    address.sin_family = AF_INET;
    if (bindAddr.ip.l != INADDR_NONE)
        address.sin_addr.s_addr = bindAddr.ip.l;
    else
        address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);
    if (bind(newsocket, (struct sockaddr *)&address, sizeof(address)) == -1) goto ErrorReturn;

    return newsocket;

ErrorReturn:
    close(newsocket);
    return -1;
}

int UDP_CloseSocket(int socket) {
    if (socket == net_broadcastsocket) net_broadcastsocket = 0;
    return close(socket);
}

int UDP_CheckNewConnections(void) {
    unsigned long available;
    struct sockaddr_in from;
    socklen_t fromlen;
    char buff[1];

    if (net_acceptsocket == -1) return -1;

    if (ioctl(net_acceptsocket, FIONREAD, &available) == -1)
        Sys_Error("%s: ioctlsocket (FIONREAD) failed", __func__);
    if (available) return net_acceptsocket;
    /* quietly absorb empty packets */
    recvfrom(net_acceptsocket, buff, 0, 0, (struct sockaddr *)&from, &fromlen);
    return -1;
}

int UDP_Read(int socket, void *buf, int len, netadr_t *addr) {
    struct sockaddr_in saddr;
    socklen_t addrlen = sizeof(saddr);
    int ret;

    ret = recvfrom(socket, buf, len, 0, (struct sockaddr *)&saddr, &addrlen);
    SockadrToNetadr(&saddr, addr);
    if (ret == -1 && (errno == EWOULDBLOCK || errno == ECONNREFUSED)) return 0;
    return ret;
}

static int UDP_MakeSocketBroadcastCapable(int socket) {
    int i = 1;

    /* make this socket broadcast capable */
    if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i)) < 0) return -1;
    net_broadcastsocket = socket;

    return 0;
}

int UDP_Broadcast(int socket, const void *buf, int len) {
    int ret;

    if (socket != net_broadcastsocket) {
        if (net_broadcastsocket != 0) Sys_Error("Attempted to use multiple broadcasts sockets");
        ret = UDP_MakeSocketBroadcastCapable(socket);
        if (ret == -1) {
            Con_Printf("Unable to make socket broadcast capable\n");
            return ret;
        }
    }

    return UDP_Write(socket, buf, len, &broadcastaddr);
}

int UDP_Write(int socket, const void *buf, int len, const netadr_t *addr) {
    struct sockaddr_in saddr;
    int ret;

    NetadrToSockadr(addr, &saddr);
    ret = sendto(socket, buf, len, 0, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret == -1 && errno == EWOULDBLOCK) return 0;
    return ret;
}

int UDP_GetSocketAddr(int socket, netadr_t *addr) {
    struct sockaddr_in saddr;
    socklen_t len = sizeof(saddr);

    memset(&saddr, 0, len);
    getsockname(socket, (struct sockaddr *)&saddr, &len);

    /*
     * The returned IP is embedded in our repsonse to a broadcast request for
     * server info from clients. The server admin may wish to advertise a
     * specific IP for various reasons, so allow the "default" address
     * returned by the OS to be overridden.
     */
    if (localAddr.ip.l != INADDR_NONE)
        saddr.sin_addr.s_addr = localAddr.ip.l;
    else {
        struct in_addr a = saddr.sin_addr;
        if (!a.s_addr || a.s_addr == htonl(INADDR_LOOPBACK)) saddr.sin_addr.s_addr = myAddr.ip.l;
    }
    SockadrToNetadr(&saddr, addr);

    return 0;
}

int UDP_GetNameFromAddr(const netadr_t *addr, char *name) {
    struct hostent *hostentry;

    hostentry = gethostbyaddr(&addr->ip.l, sizeof(addr->ip.l), AF_INET);
    if (hostentry) {
        strncpy(name, (char *)hostentry->h_name, NET_NAMELEN - 1);
        return 0;
    }
    strcpy(name, NET_AdrToString(addr));

    return 0;
}

int UDP_GetAddrFromName(const char *name, netadr_t *addr) {
    struct hostent *hostentry;

    if (name[0] >= '0' && name[0] <= '9') return NET_PartialIPAddress(name, &myAddr, addr);

    hostentry = gethostbyname(name);
    if (!hostentry) return -1;

    addr->ip.l = *(int *)hostentry->h_addr_list[0];
    addr->port = htons(net_hostport);

    return 0;
}

int UDP_GetDefaultMTU(void) { return 1400; }
