/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Peter Mackay and Chris Swindle.

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

//#include <psputility_netmodules.h>
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspwlan.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>

#include "network_psp.hpp"

#include <arpa/inet.h>	// inet_addr
#include <netinet/in.h>	// sockaddr_in
#include <sys/socket.h>	// PSP socket API.
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>

#include "gethost.hpp"

#include <pspsdk.h>

#define ntohs(x) __builtin_bswap16(x)
#define ntohl(x) __builtin_bswap32(x)
#define htons(x) __builtin_bswap16(x)
#define htonl(x) __builtin_bswap32(x)

#define ADHOC_NET 29
#define ADHOC_EWOULDBLOCK	0x80410709

#define MAXHOSTNAMELEN	64
#define EWOULDBLOCK		111
#define ECONNREFUSED	11

int totalAccessPoints = 0;
cvar_t accesspoint = {"accesspoint", "1", qtrue};
int accessPointNumber[100];


typedef struct sockaddr_adhoc
{
        char   len;
        short family;
        u16	port;
        char mac[6];
	char zero[6];
} sockaddr_adhoc;

#ifdef PROFILE
#define sceUtilityLoadNetModule
#define sceUtilityUnLoadNetModule
#endif

static pdpStatStruct gPdpStat;
pdpStatStruct *findPdpStat(int socket, pdpStatStruct *pdpStat)
{
	if(socket == pdpStat->pdpId) {
		memcpy_vfpu(&gPdpStat, pdpStat, sizeof(pdpStatStruct));
		return &gPdpStat;
	}
		if(pdpStat->next) return findPdpStat(socket, pdpStat->next);
		return (pdpStatStruct *)-1;
}


namespace quake
{
	namespace network
	{
		namespace infrastructure
		{
			static int accept_socket = -1;		// socket for fielding new connections
			static int control_socket = -1;
			static int broadcast_socket = 0;
			static struct qsockaddr broadcast_addr;
			static int my_addr = 0;

			int init (void)
			{
				if(totalAccessPoints == 0)
				{
					int iNetIndex;
					memset(accessPointNumber, 0, sizeof(accessPointNumber));
					for (iNetIndex = 1; iNetIndex < 100; iNetIndex++) // skip the 0th connection
					{
						if (sceUtilityCheckNetParam(iNetIndex) == 0)
						{
							totalAccessPoints++;
							accessPointNumber[totalAccessPoints] = iNetIndex;
						}
					}
					if(accesspoint.value > totalAccessPoints)
						Cvar_SetValue("accesspoint", 1);
				}

				if(!host_initialized)
				{
					Cvar_RegisterVariable(&accesspoint);
				}

				if(!tcpipAvailable)
					return -1;

				char szMyIPAddr[32];
				struct qsockaddr addr;
				char *colon;

				char	buff[MAXHOSTNAMELEN];

				S_ClearBuffer ();		// so dma doesn't loop current sound

				// Load the network modules when they are required
				const int err1 = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
				if(err1 < 0)
				{
					Con_Printf("Couldn't load MODULE_COMMON: %08x\n",err1);
				}
				const int err2 = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
				if(err2 < 0)
				{
					Con_Printf("Couldn't load MODULE_INET: %09x\n ",err2);
				}

				// Initialise the network.
				const int err = pspSdkInetInit();
				if (err)
				{
					Con_Printf("Couldn't initialize the network %08X\n", err);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
					return -1;
				}

				if(!connect_to_apctl(accessPointNumber[(int)accesspoint.value]))
				{
					Con_Printf("Unable to connect to access point\n");
					pspSdkInetTerm();
					sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);

					return -1;
				}

				// connected, get my IPADDR and run test
				if (sceNetApctlGetInfo(8, (SceNetApctlInfo*)szMyIPAddr) != 0)
					strcpy(szMyIPAddr, "unknown IP address");

				// determine my name & address
				gethostname(buff, MAXHOSTNAMELEN);

				my_addr = inet_addr(szMyIPAddr);

				// if the quake hostname isn't set, set it to the machine name
				if (strcmp(hostname.string, "UNNAMED") == 0)
				{
					buff[15] = 0;
					Cvar_Set ("hostname", buff);
				}

				if ((control_socket = open_socket(0)) == -1)
				{
					pspSdkInetTerm();
					sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
					Sys_Error("init: Unable to open control socket\n");
				}

				((struct sockaddr_in *)&broadcast_addr)->sin_family = AF_INET;
				((struct sockaddr_in *)&broadcast_addr)->sin_addr.s_addr = INADDR_BROADCAST;
				((struct sockaddr_in *)&broadcast_addr)->sin_port = htons(net_hostport);

				get_socket_addr (control_socket, &addr);
				Q_strcpy(my_tcpip_address,  addr_to_string(&addr));
				colon = Q_strrchr (my_tcpip_address, ':');
				if (colon)
					*colon = 0;

				Con_Printf("UDP Initialized\n");
				tcpipAvailable = qtrue;

				return control_socket;
			}

			//=============================================================================

			void shut_down (void)
			{
				listen(qfalse);
				close_socket(control_socket);

				pspSdkInetTerm();

				// Now to unload the network modules, no need to keep them loaded all the time
				sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
				sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
			}

			//=============================================================================

			void listen (qboolean state)
			{
				// enable listening
				if (state)
				{
					if (accept_socket != -1)
						return;
					if ((accept_socket = open_socket(net_hostport)) == -1)
						Sys_Error ("listen: Unable to open accept socket\n");
					return;
				}

				// disable listening
				if (accept_socket == -1)
					return;
				close_socket(accept_socket);

				accept_socket = -1;
			}

			//=============================================================================

			int open_socket (int port)
			{
				int newsocket;
				struct sockaddr_in address;

				if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
					return -1;

				int val = 1;
				if(setsockopt(newsocket, SOL_SOCKET, SO_NONBLOCK, &val, sizeof(val)) < 0)
					goto ErrorReturn;

				address.sin_family = AF_INET;
				address.sin_addr.s_addr = INADDR_ANY;
				address.sin_port = htons(port);
				if( bind (newsocket, (sockaddr *)&address, sizeof(address)) == -1)
					goto ErrorReturn;

				printf("got here4\n");
				return newsocket;

			ErrorReturn:
				close(newsocket);
				return -1;
			}

			//=============================================================================

			int close_socket (int socket)
			{
				if (socket == broadcast_socket)
					broadcast_socket = 0;
				return close(socket);
			}

			int connect (int socket, struct qsockaddr *addr)
			{
				return 0;
			}

			//=============================================================================

			int check_new_connections (void)
			{
				byte buf[1000];

				if (accept_socket == -1)
					return -1;

				// Peek at the message and if there is a message waiting then return the
				// socket
				int ret = recvfrom(accept_socket, buf, 1000, MSG_PEEK, NULL, NULL);

				if(ret > 0)
					return accept_socket;

				return -1;
			}

			//=============================================================================

			int read (int socket, byte *buf, int len, struct qsockaddr *addr)
			{
				int addrlen = sizeof (struct qsockaddr);
				int ret;

				ret = recvfrom (socket, buf, len, 0, (struct sockaddr *)addr, (socklen_t*)&addrlen);
				if (ret == -1)
				{
					int errno = sceNetInetGetErrno();
					if(errno == EWOULDBLOCK || errno == ECONNREFUSED)
					{
						return 0;
					}
				}

				return ret;
			}

			//=============================================================================

			static int make_socket_broadcast_capable (int socket)
			{
				int				i = 1;

				// make this socket broadcast capable
				if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i)) < 0)
					return -1;
				broadcast_socket = socket;

				return 0;
			}

			//=============================================================================

			int broadcast (int socket, byte *buf, int len)
			{
				int ret;

				if (socket != broadcast_socket)
				{
					if (broadcast_socket != 0)
						Sys_Error("Attempted to use multiple broadcasts sockets\n");
					ret = make_socket_broadcast_capable (socket);
					if (ret == -1)
					{
						Con_Printf("Unable to make socket broadcast capable\n");
						return ret;
					}
				}

				return write(socket, buf, len, &broadcast_addr);
			}

			//=============================================================================

			int write (int socket, byte *buf, int len, struct qsockaddr *addr)
			{
				int ret;

				ret = sendto (socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));
				//ret = sceNetInetSendto (socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));
				if (ret == -1)
				{
					int errno = sceNetInetGetErrno();
					if(errno == EWOULDBLOCK || errno == ECONNREFUSED)
					{
						return 0;
					}
					Con_Printf("Failed to send message, errno=%08X\n", errno);
				}
				return ret;
			}

			//=============================================================================

			char* addr_to_string (struct qsockaddr *addr)
			{
				static char buffer[22];
				int haddr;

				haddr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
				sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff, ntohs(((struct sockaddr_in *)addr)->sin_port));
				return buffer;
			}

			//=============================================================================

			int string_to_addr (char *string, struct qsockaddr *addr)
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

			int get_socket_addr (int socket, struct qsockaddr *addr)
			{
				socklen_t addrlen = sizeof(struct qsockaddr);
				unsigned int a;

				Q_memset(addr, 0, sizeof(struct qsockaddr));
				getsockname(socket, (struct sockaddr *)addr, &addrlen);
				a = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
				if (a == 0 || a == inet_addr("127.0.0.1"))
					((struct sockaddr_in *)addr)->sin_addr.s_addr = my_addr;

				return 0;
			}

			//=============================================================================

			int get_name_from_addr (struct qsockaddr *addr, char *name)
			{
				struct hostent *hostentry;

				hostentry = gethostbyaddr ((char *)&((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
				if (hostentry)
				{
					Q_strncpy (name, (char *)hostentry->h_name, NET_NAMELEN - 1);
					return 0;
				}

				Q_strcpy (name, addr_to_string (addr));
				return 0;
			}

			//=============================================================================

			int get_addr_from_name(char *name, struct qsockaddr *addr)
			{
				struct hostent *hostentry;

				if (name[0] >= '0' && name[0] <= '9')
					return PartialIPAddress (name, addr);

				hostentry = gethostbyname (name);
				if (!hostentry)
					return -1;

				addr->sa_family = AF_INET;
				((struct sockaddr_in *)addr)->sin_port = htons(net_hostport);
				((struct sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

				return 0;
			}

			//=============================================================================

			int addr_compare (struct qsockaddr *addr1, struct qsockaddr *addr2)
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

			int get_socket_port (struct qsockaddr *addr)
			{
				return ntohs(((struct sockaddr_in *)addr)->sin_port);
			}


			int set_socket_port (struct qsockaddr *addr, int port)
			{
				((struct sockaddr_in *)addr)->sin_port = htons(port);
				return 0;
			}

			/*
			============
			PartialIPAddress

			this lets you type only as much of the net address as required, using
			the local network components to fill in the rest
			============
			*/
			int PartialIPAddress (char *in, struct qsockaddr *hostaddr)
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
					port = Q_atoi(b);
				else
					port = net_hostport;

				hostaddr->sa_family = AF_INET;
				((struct sockaddr_in *)hostaddr)->sin_port = htons((short)port);
				((struct sockaddr_in *)hostaddr)->sin_addr.s_addr = (my_addr & htonl(mask)) | htonl(addr);

				return 0;
			}
			//=============================================================================
			/* Connect to an access point */
			int connect_to_apctl(int config)
			{
				int err;
				int stateLast = -1;
				int timeout = 0;

				/* Connect using the first profile */
				err = sceNetApctlConnect(config);
				if (err != 0)
				{
					return 0;
				}

				while (1)
				{
					int state;
					err = sceNetApctlGetState(&state);
					if (err != 0)
					{
						break;
					}
					if (state > stateLast)
					{
						stateLast = state;
						timeout = 0;
					}
					if (state == 4)
						break;  // connected with static IP

					// wait a little before polling again
					sceKernelDelayThread(50*1000); // 50ms

					timeout++;
					if(timeout > 200)
					{
						Con_Printf("Timeout connecting to access point. State=%d\n", state);
						return 0;
					}
				}

				if(err != 0)
				{
					return 0;
				}

				return 1;
			}
			//=============================================================================
		}
		namespace adhoc
		{
			static int accept_socket = -1;		// socket for fielding new connections
			static int control_socket = -1;
			static int broadcast_socket = 0;
			static struct qsockaddr broadcast_addr;

			int init (void)
			{
				if(!tcpipAvailable) return -1;

				struct qsockaddr addr;
				char *colon;
				char	buff[MAXHOSTNAMELEN];
				int i,rc;

				S_ClearBuffer ();

				sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
				sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);

				int stateLast = -1;
				rc = pspSdkAdhocInit("ULUS00443");
				if(rc < 0) {
					Con_Printf("Couldn't initialise the network %08X\n", rc);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
					return -1;
				}

				rc = sceNetAdhocctlConnect((const char *)"quake");
				if(rc < 0) {
					Con_Printf("Couldn't initialise the network %08X\n", rc);
					pspSdkAdhocTerm();
					sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
					return -1;
				}

				while (1) {
					int state;
					rc = sceNetAdhocctlGetState(&state);
					if (rc != 0) {
						Con_Printf("Couldn't initialise the network %08X\n", rc);
						pspSdkAdhocTerm();
						sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
						sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
						return -1;
					}
					if (state > stateLast) {
						stateLast = state;
					}
					if (state == 1) {
						break;
					}
					sceKernelDelayThread(50*1000); // 50ms
				}

				gethostname(buff, MAXHOSTNAMELEN);
				if (strcmp(hostname.string, "UNNAMED") == 0)
				{
					buff[15] = 0;
					Cvar_Set ("hostname", buff);
				}

				if ((control_socket = open_socket(0)) == -1) {
					pspSdkAdhocTerm();
					sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
					sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
					Sys_Error("init: Unable to open control socket\n");
				}

				((sockaddr_adhoc *)&broadcast_addr)->family = ADHOC_NET;
				for(i=0; i<6; i++) ((sockaddr_adhoc *)&broadcast_addr)->mac[i] = 0xFF;
				((sockaddr_adhoc *)&broadcast_addr)->port = net_hostport;

				get_socket_addr (control_socket, &addr);
				Q_strcpy(my_tcpip_address,  addr_to_string(&addr));
				colon = Q_strrchr(my_tcpip_address, ':');
				if (colon) *colon = 0;

				listen(qtrue);

				Con_Printf("AdHoc Initialized\n");
				tcpipAvailable = qtrue;
				tcpipAdhoc = qtrue;
				return control_socket;
			}

			//=============================================================================

			void shut_down (void)
			{
				listen(qfalse);
				close_socket(control_socket);
				pspSdkAdhocTerm();

				// Now to unload the network modules, no need to keep them loaded all the time
				sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
				sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
			}

			//=============================================================================

			void listen (qboolean state)
			{
				// enable listening
				if (state)
				{
					if (accept_socket != -1)
						return;
					if ((accept_socket = open_socket(net_hostport)) == -1)
						Sys_Error ("listen: Unable to open accept socket\n");
					return;
				}

				// disable listening
				if (accept_socket == -1)
					return;
				close_socket(accept_socket);
				accept_socket = -1;
			}

			//=============================================================================

			int open_socket (int port)
			{
				u8 mac[8];
				sceWlanGetEtherAddr(mac);
				int rc = sceNetAdhocPdpCreate(mac, port, 0x2000, 0);
				if(rc < 0) return -1;
				return rc;
			}

			//=============================================================================

			int close_socket (int socket)
			{
				if (socket == broadcast_socket) broadcast_socket = 0;
				return sceNetAdhocPdpDelete(socket, 0);
			}

			int connect (int socket, struct qsockaddr *addr)
			{
				return 0;
			}

			//=============================================================================

			int check_new_connections (void)
			{
				pdpStatStruct pdpStat[20];
				int length = sizeof(pdpStatStruct) * 20;

				if (accept_socket == -1)
					return -1;

				int err = sceNetAdhocGetPdpStat(&length, pdpStat);
				if(err < 0) return -1;

				pdpStatStruct *tempPdp = findPdpStat(accept_socket, pdpStat);

				if(tempPdp->pdpId < 0) return -1;

				if(tempPdp->rcvdData > 0) return accept_socket;

				return -1;
			}

			//=============================================================================

			int read (int socket, byte *buf, int len, struct qsockaddr *addr)
			{
				unsigned short port;
				int datalength = len;
				unsigned int ret;

				sceKernelDelayThread(1);

				ret = sceNetAdhocPdpRecv(socket, (unsigned char *)((sockaddr_adhoc *)addr)->mac, &port, buf, &datalength, 0, 1);
				if(ret == ADHOC_EWOULDBLOCK) return 0;

				((sockaddr_adhoc *)addr)->port = port;

				return datalength;
			}

			//=============================================================================

			static int make_socket_broadcast_capable (int socket)
			{
				broadcast_socket = socket;
				return 0;
			}

			//=============================================================================

			int broadcast (int socket, byte *buf, int len)
			{
				int ret;

				if (socket != broadcast_socket)
				{
					if (broadcast_socket != 0)
						Sys_Error("Attempted to use multiple broadcasts sockets\n");
					ret = make_socket_broadcast_capable (socket);
					if (ret == -1)
					{
						Con_Printf("Unable to make socket broadcast capable\n");
						return ret;
					}
				}

				return write(socket, buf, len, &broadcast_addr);
			}

			//=============================================================================

			int write (int socket, byte *buf, int len, struct qsockaddr *addr)
			{
				int ret = -1;

				ret = sceNetAdhocPdpSend(socket, (unsigned char*)((sockaddr_adhoc *)addr)->mac, ((sockaddr_adhoc *)addr)->port, buf, len, 0, 1);

				//if(ret < 0) Con_Printf("Failed to send message, errno=%08X\n", ret);//blubs
				return ret;
			}

			//=============================================================================

			char* addr_to_string (struct qsockaddr *addr)
			{
				static char buffer[22];

				sceNetEtherNtostr((unsigned char *)((sockaddr_adhoc *)addr)->mac, buffer);
				sprintf(buffer + strlen(buffer), ":%d", ((sockaddr_adhoc *)addr)->port);
				return buffer;
			}

			//=============================================================================

			int string_to_addr (char *string, struct qsockaddr *addr)
			{
				int ha1, ha2, ha3, ha4, ha5, ha6, hp;

				sscanf(string, "%x:%x:%x:%x:%x:%x:%d", &ha1, &ha2, &ha3, &ha4, &ha5, &ha6, &hp);
				addr->sa_family = ADHOC_NET;
				((struct sockaddr_adhoc *)addr)->mac[0] = ha1 & 0xFF;
				((struct sockaddr_adhoc *)addr)->mac[1] = ha2 & 0xFF;
				((struct sockaddr_adhoc *)addr)->mac[2] = ha3 & 0xFF;
				((struct sockaddr_adhoc *)addr)->mac[3] = ha4 & 0xFF;
				((struct sockaddr_adhoc *)addr)->mac[4] = ha5 & 0xFF;
				((struct sockaddr_adhoc *)addr)->mac[5] = ha6 & 0xFF;
				((struct sockaddr_adhoc *)addr)->port = hp & 0xFFFF;
				return 0;
			}

			//=============================================================================

			int get_socket_addr (int socket, struct qsockaddr *addr)
			{
				pdpStatStruct pdpStat[20];
				int length = sizeof(pdpStatStruct) * 20;

				int err = sceNetAdhocGetPdpStat(&length, pdpStat);
				if(err<0) return -1;

				pdpStatStruct *tempPdp = findPdpStat(socket, pdpStat);
				if(tempPdp->pdpId < 0) return -1;

				memcpy_vfpu(((struct sockaddr_adhoc *)addr)->mac, tempPdp->mac, 6);
				((struct sockaddr_adhoc *)addr)->port = tempPdp->port;
				addr->sa_family = ADHOC_NET;
				return 0;
			}

			//=============================================================================

			int get_name_from_addr (struct qsockaddr *addr, char *name)
			{
				strcpy(name, addr_to_string(addr));
				return 0;
			}

			//=============================================================================

			int get_addr_from_name(char *name, struct qsockaddr *addr)
			{
				return string_to_addr(name, addr);
			}

			//=============================================================================

			int addr_compare (struct qsockaddr *addr1, struct qsockaddr *addr2)
			{
				//if (addr1->sa_family != addr2->sa_family) return -1;
				if (memcmp(((struct sockaddr_adhoc *)addr1)->mac, ((struct sockaddr_adhoc *)addr2)->mac, 6) != 0) return -1;
				if (((struct sockaddr_adhoc *)addr1)->port != ((struct sockaddr_adhoc *)addr2)->port) return 1;
				return 0;
			}

			//=============================================================================

			int get_socket_port (struct qsockaddr *addr)
			{
				return ((struct sockaddr_adhoc *)addr)->port;
			}


			int set_socket_port (struct qsockaddr *addr, int port)
			{
				((struct sockaddr_adhoc *)addr)->port = port;
				return 0;
			}

			//=============================================================================

			int pspSdkAdhocInit(char *product)
			{
				u32 retVal;
				struct productStruct temp;

				retVal = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
				if (retVal != 0) return retVal;

				retVal = sceNetAdhocInit();
				if (retVal != 0) return retVal;

				strcpy(temp.product, product);
				temp.unknown = 0;

				retVal = sceNetAdhocctlInit(0x2000, 0x20, &temp);
				if (retVal != 0) return retVal;

				return 0;
			}

			//=============================================================================

			void pspSdkAdhocTerm()
			{
				sceNetAdhocctlTerm();
				sceNetAdhocTerm();
				sceNetTerm();
			}

			//=============================================================================

		}
	}
}
