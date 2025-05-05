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
// net_udp.c

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Including the device-specific network layer header:
#include <network.h>
// <<< FIX

#include "../nzportable_def.h"

#include <sys/types.h>
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Removing non-present headers:
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <netdb.h>
// <<< FIX
#include <sys/param.h>
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Removing non-present headers:
//#include <sys/ioctl.h>
// <<< FIX
#include <errno.h>

#ifdef __sun__
#include <sys/filio.h>
#endif

#ifdef NeXT
#include <libc.h>
#endif

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Adding stuff supposed to be present on the previously removed headers:
#define MAXHOSTNAMELEN	256
// <<< FIX

extern int gethostname (char *, int);
extern int close (int);

extern cvar_t hostname;

static int net_acceptsocket = -1;		// socket for fielding new connections
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static unsigned long myAddr;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// New variable to signal if_config() error:
int netinit_error;
// <<< FIX

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// New variable containing the current IP address of the device:
char ipaddress_text[16];
// <<< FIX

#include "net_udp_wii.h"

//=============================================================================

int UDP_Init (void)
{
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// This variable is not needed in the current implementation:
	//struct hostent *local;
// <<< FIX
	char	buff[MAXHOSTNAMELEN];
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// This variable is not needed in the current implementation:
	//struct qsockaddr addr;
// <<< FIX
	char *colon;
	
	if (COM_CheckParm ("-noudp"))
		return -1;

	do
	{
		netinit_error = if_config(ipaddress_text, NULL, NULL, true);
	} while(netinit_error == -EAGAIN);

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Signal as uninitialized if if_config() failed previously:
	if(netinit_error < 0)
	{
		Con_Printf("UDP_Init: if_config() failed with %i", netinit_error);
		return -1;
	};
// <<< FIX

	// determine my name & address
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Since we don't currently have a gethostname(), or equivalent, function, let's just paste the IP address of the device:
	//gethostname(buff, MAXHOSTNAMELEN);
	//local = gethostbyname(buff);
	//myAddr = *(int *)local->h_addr_list[0];
	strcpy(buff, ipaddress_text);
// <<< FIX

	// if the quake hostname isn't set, set it to the machine name
	if (strcmp(hostname.string, "UNNAMED") == 0)
	{
		buff[15] = 0;
		Cvar_Set ("hostname", buff);
	}

	if ((net_controlsocket = UDP_OpenSocket (0)) == -1)
		Sys_Error("UDP_Init: Unable to open control socket\n");

	((struct sockaddr_in *)&broadcastaddr)->sin_family = AF_INET;
	((struct sockaddr_in *)&broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
	((struct sockaddr_in *)&broadcastaddr)->sin_port = htons(net_hostport);

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Since we can't bind anything to port 0, the following line does not work. Replacing:
	//UDP_GetSocketAddr (net_controlsocket, &addr);
	//strcpy(my_tcpip_address,  UDP_AddrToString (&addr));
	strcpy(my_tcpip_address, buff);
// <<< FIX
	colon = strrchr (my_tcpip_address, ':');
	if (colon)
		*colon = 0;

	Con_Printf("UDP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void UDP_Shutdown (void)
{
	UDP_Listen (false);
	UDP_CloseSocket (net_controlsocket);
}

//=============================================================================

void UDP_Listen (qboolean state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != -1)
			return;
		if ((net_acceptsocket = UDP_OpenSocket (net_hostport)) == -1)
			Sys_Error ("UDP_Listen: Unable to open accept socket\n");
		return;
	}

	// disable listening
	if (net_acceptsocket == -1)
		return;
	UDP_CloseSocket (net_acceptsocket);
	net_acceptsocket = -1;
}

//=============================================================================

int UDP_OpenSocket (int port)
{
	int newsocket;
	struct sockaddr_in address;
	qboolean _TRUE = true;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function in the library (and using supported parameters):
	//if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	if ((newsocket = net_socket (PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
// <<< FIX
		return -1;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function in the library:
	//if (ioctl (newsocket, FIONBIO, (char *)&_TRUE) == -1)
	if (net_ioctl (newsocket, FIONBIO, (char *)&_TRUE) < 0)
// <<< FIX
		goto ErrorReturn;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Do not bind the socket if port == 0 (part 1):
	if(port > 0) 
	{
// <<< FIX
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function in the library:
	//if( bind (newsocket, (void *)&address, sizeof(address)) == -1)
	if( net_bind (newsocket, (void *)&address, sizeof(address)) < 0)
// <<< FIX
		goto ErrorReturn;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Do not bind the socket if port == 0 (part 2):
	}
// <<< FIX
	return newsocket;

ErrorReturn:
	close (newsocket);
	return -1;
}

//=============================================================================

int UDP_CloseSocket (int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
	return close (socket);
}


//=============================================================================
/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int PartialIPAddress (char *in, struct qsockaddr *hostaddr)
{
	char buff[256];
	char *b;
	int addr;
	int num;
	int mask;
	int run;
	int port;
	
	buff[0] = '.';
	b = buff;
	strcpy(buff+1, in);
	if (buff[1] == '.')
		b++;

	addr = 0;
	mask=-1;
	while (*b == '.')
	{
		b++;
		num = 0;
		run = 0;
		while (!( *b < '0' || *b > '9'))
		{
		  num = num*10 + *b++ - '0';
		  if (++run > 3)
		  	return -1;
		}
		if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
			return -1;
		if (num < 0 || num > 255)
			return -1;
		mask<<=8;
		addr = (addr<<8) + num;
	}
	
	if (*b++ == ':')
		port = atoi(b);
	else
		port = net_hostport;

	hostaddr->sa_family = AF_INET;
	((struct sockaddr_in *)hostaddr)->sin_port = htons((short)port);	
	((struct sockaddr_in *)hostaddr)->sin_addr.s_addr = (myAddr & htonl(mask)) | htonl(addr);
	
	return 0;
}
//=============================================================================

int UDP_Connect (int socket, struct qsockaddr *addr)
{
	return 0;
}

//=============================================================================

int UDP_CheckNewConnections (void)
{
	unsigned long	available;

	if (net_acceptsocket == -1)
		return -1;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function in the library:
	//if (ioctl (net_acceptsocket, FIONREAD, &available) == -1)
	if (net_ioctl (net_acceptsocket, FIONREAD, &available) < 0)
// <<< FIX
		Sys_Error ("UDP: ioctlsocket (FIONREAD) failed\n");
	if (available)
		return net_acceptsocket;
	return -1;
}

//=============================================================================

int UDP_Read (int socket, byte *buf, int len, struct qsockaddr *addr)
{
	int addrlen = sizeof (struct qsockaddr);
	int ret;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function (and data structures) in the library:
	//ret = recvfrom (socket, buf, len, 0, (struct sockaddr *)addr, &addrlen);
	//if (ret == -1 && (errno == EWOULDBLOCK || errno == ECONNREFUSED))
	struct sockaddr_in torecv;
	socklen_t torecvlen;

	memset(&torecv, 0, sizeof(torecv));
	torecvlen = addrlen;
	ret = net_recvfrom(socket, buf, len > 4096 ? 4096 : len, 0, (struct sockaddr *)&torecv, &torecvlen);
	addr->sa_family = torecv.sin_family;
	((struct sockaddr_in *)addr)->sin_port = torecv.sin_port;
	((struct sockaddr_in *)addr)->sin_addr = torecv.sin_addr;
	if((ret == -EWOULDBLOCK) || (ret == -ECONNREFUSED))
// <<< FIX
		return 0;
	return ret;
}

//=============================================================================

int UDP_MakeSocketBroadcastCapable (int socket)
{
	int				i = 1;

	// make this socket broadcast capable
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function in the library:
	//if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0)
	if (net_setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0)
// <<< FIX
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

//=============================================================================

int UDP_Broadcast (int socket, byte *buf, int len)
{
	int ret;

	if (socket != net_broadcastsocket)
	{
		if (net_broadcastsocket != 0)
			Sys_Error("Attempted to use multiple broadcasts sockets\n");
		ret = UDP_MakeSocketBroadcastCapable (socket);
		if (ret == -1)
		{
			Con_Printf("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return UDP_Write (socket, buf, len, &broadcastaddr);
}

//=============================================================================

int UDP_Write (int socket, byte *buf, int len, struct qsockaddr *addr)
{
	int ret;

// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function (and data structures) in the library:
	//ret = sendto (socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));
	//if (ret == -1 && errno == EWOULDBLOCK)
	struct sockaddr_in tosend;

    memset(&tosend, 0, sizeof(struct sockaddr_in));
    tosend.sin_family = ((struct sockaddr_in *)addr)->sin_family;
    tosend.sin_port = ((struct sockaddr_in *)addr)->sin_port;
    tosend.sin_addr = ((struct sockaddr_in *)addr)->sin_addr;
    tosend.sin_len = 8;
	ret = net_sendto (socket, buf, len, 0, (struct sockaddr *)&tosend, 8);
	if(ret == -EWOULDBLOCK)
// <<< FIX
		return 0;
	return ret;
}

//=============================================================================

char *UDP_AddrToString (struct qsockaddr *addr)
{
	static char buffer[22];
	int haddr;

	haddr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
	sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff, ntohs(((struct sockaddr_in *)addr)->sin_port));
	return buffer;
}

//=============================================================================

int UDP_StringToAddr (char *string, struct qsockaddr *addr)
{
	int ha1, ha2, ha3, ha4, hp;
	int ipaddr;

	sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
	ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

	addr->sa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
	((struct sockaddr_in *)addr)->sin_port = htons(hp);
	return 0;
}

//=============================================================================

int UDP_GetSocketAddr (int socket, struct qsockaddr *addr)
{
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Since there's no getsockname(), or equivalent, function in the library, just assign it myAddr:
	//int addrlen = sizeof(struct qsockaddr);
	//unsigned int a;

	//memset(addr, 0, sizeof(struct qsockaddr));
	//getsockname(socket, (struct sockaddr *)addr, &addrlen);
	//a = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
	//if (a == 0 || a == inet_addr("127.0.0.1"))
// <<< FIX
		((struct sockaddr_in *)addr)->sin_addr.s_addr = myAddr;

	return 0;
}

//=============================================================================

int UDP_GetNameFromAddr (struct qsockaddr *addr, char *name)
{
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Since there's no gethostbyaddr(), or equivalent, function in the library, just copy addr to name:
	//struct hostent *hostentry;

	//hostentry = gethostbyaddr ((char *)&((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
	//if (hostentry)
	//{
	//	strncpy (name, (char *)hostentry->h_name, NET_NAMELEN - 1);
	//	return 0;
	//}
// <<< FIX

	strcpy (name, UDP_AddrToString (addr));
	return 0;
}

//=============================================================================

int UDP_GetAddrFromName(char *name, struct qsockaddr *addr)
{
	struct hostent *hostentry;

	if (name[0] >= '0' && name[0] <= '9')
		return PartialIPAddress (name, addr);
	
// >>> FIX: For Nintendo Wii using devkitPPC / libogc
// Switching to the equivalent function in the library:
	//hostentry = gethostbyname (name);
	hostentry = net_gethostbyname (name);
// <<< FIX
	if (!hostentry)
		return -1;

	addr->sa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_port = htons(net_hostport);	
	((struct sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

	return 0;
}

//=============================================================================

int UDP_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2)
{
	if (addr1->sa_family != addr2->sa_family)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_addr.s_addr != ((struct sockaddr_in *)addr2)->sin_addr.s_addr)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_port != ((struct sockaddr_in *)addr2)->sin_port)
		return 1;

	return 0;
}

//=============================================================================

int UDP_GetSocketPort (struct qsockaddr *addr)
{
	return ntohs(((struct sockaddr_in *)addr)->sin_port);
}


int UDP_SetSocketPort (struct qsockaddr *addr, int port)
{
	((struct sockaddr_in *)addr)->sin_port = htons(port);
	return 0;
}

//=============================================================================
