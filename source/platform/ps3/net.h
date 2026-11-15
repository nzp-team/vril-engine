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

struct qsockaddr {
    unsigned char sa_len;
    unsigned char sa_family;
    unsigned char sa_data[14];
};

#define NET_NAMELEN          64

#define NET_MAXMESSAGE       16384
#define NET_HEADERSIZE       (2 * sizeof(unsigned int))
#define NET_DATAGRAMSIZE     (MAX_DATAGRAM + NET_HEADERSIZE)

#define NETFLAG_LENGTH_MASK  0x0000ffff
#define NETFLAG_DATA         0x00010000
#define NETFLAG_ACK          0x00020000
#define NETFLAG_NAK          0x00040000
#define NETFLAG_EOM          0x00080000
#define NETFLAG_UNRELIABLE   0x00100000
#define NETFLAG_CTL          0x80000000

#define NET_PROTOCOL_VERSION 3

#define CCREQ_CONNECT        0x01
#define CCREQ_SERVER_INFO    0x02
#define CCREQ_PLAYER_INFO    0x03
#define CCREQ_RULE_INFO      0x04

#define CCREP_ACCEPT         0x81
#define CCREP_REJECT         0x82
#define CCREP_SERVER_INFO    0x83
#define CCREP_PLAYER_INFO    0x84
#define CCREP_RULE_INFO      0x85

typedef struct qsocket_s {
    struct qsocket_s * next;
    double             connecttime;
    double             lastMessageTime;
    double             lastSendTime;

    qboolean           disconnected;
    qboolean           canSend;
    qboolean           sendNext;

    int                driver;
    int                landriver;
    int                socket;
    void *             driverdata;

    unsigned int       ackSequence;
    unsigned int       sendSequence;
    unsigned int       unreliableSendSequence;
    int                sendMessageLength;
    byte               sendMessage[NET_MAXMESSAGE];

    unsigned int       receiveSequence;
    unsigned int       unreliableReceiveSequence;
    int                receiveMessageLength;
    byte               receiveMessage[NET_MAXMESSAGE];

    struct qsockaddr   addr;
    char               address[NET_NAMELEN];
} qsocket_t;

extern qsocket_t * net_activeSockets;
extern qsocket_t * net_freeSockets;
extern int net_numsockets;

typedef struct {
    char *   name;
    qboolean initialized;
    int      controlSock;
    int (*Init)(void);
    void (*Shutdown)(void);
    void (*Listen)(qboolean state);
    int (*OpenSocket)(int port);
    int (*CloseSocket)(int socket);
    int (*Connect)(int socket, struct qsockaddr * addr);
    int (*CheckNewConnections)(void);
    int (*Read)(int socket, byte * buf, int len, struct qsockaddr * addr);
    int (*Write)(int socket, byte * buf, int len, struct qsockaddr * addr);
    int (*Broadcast)(int socket, byte * buf, int len);
    char * (*AddrToString)(struct qsockaddr * addr);
    int (*StringToAddr)(char * string, struct qsockaddr * addr);
    int (*GetSocketAddr)(int socket, struct qsockaddr * addr);
    int (*GetNameFromAddr)(struct qsockaddr * addr, char * name);
    int (*GetAddrFromName)(char * name, struct qsockaddr * addr);
    int (*AddrCompare)(struct qsockaddr * addr1, struct qsockaddr * addr2);
    int (*GetSocketPort)(struct qsockaddr * addr);
    int (*SetSocketPort)(struct qsockaddr * addr, int port);
} net_landriver_t;

#define MAX_NET_DRIVERS 8
extern int net_numlandrivers;
extern net_landriver_t net_landrivers[MAX_NET_DRIVERS];

typedef struct {
    char *   name;
    qboolean initialized;
    int (*Init)(void);
    void (*Listen)(qboolean state);
    void (*SearchForHosts)(qboolean xmit);
    qsocket_t * (*Connect)(char * host);
    qsocket_t * (*CheckNewConnections)(void);
    int (*QGetMessage)(qsocket_t * sock);
    int (*QSendMessage)(qsocket_t * sock, sizebuf_t * data);
    int (*SendUnreliableMessage)(qsocket_t * sock, sizebuf_t * data);
    qboolean (*CanSendMessage)(qsocket_t * sock);
    qboolean (*CanSendUnreliableMessage)(qsocket_t * sock);
    void (*Close)(qsocket_t * sock);
    void (*Shutdown)(void);
    int controlSock;
} net_driver_t;

extern int net_numdrivers;
extern net_driver_t net_drivers[MAX_NET_DRIVERS];

extern int DEFAULTnet_hostport;
extern int net_hostport;

extern int net_driverlevel;
extern cvar_t hostname;
extern char playername[];
extern int playercolor;

extern int messagesSent;
extern int messagesReceived;
extern int unreliableMessagesSent;
extern int unreliableMessagesReceived;

qsocket_t *
NET_NewQSocket(void);
void
NET_FreeQSocket(qsocket_t *);
double
SetNetTime(void);

#define HOSTCACHESIZE 8

typedef struct {
    char             name[16];
    char             map[16];
    char             cname[32];
    int              users;
    int              maxusers;
    int              driver;
    int              ldriver;
    struct qsockaddr addr;
} hostcache_t;

extern int hostCacheCount;
extern hostcache_t hostcache[HOSTCACHESIZE];

#ifdef IDGODS
qboolean
IsID(struct qsockaddr * addr);
#endif

extern double net_time;
extern sizebuf_t net_message;
extern int net_activeconnections;

void
NET_Init(void);
void
NET_Shutdown(void);

struct qsocket_s *
NET_CheckNewConnections(void);

struct qsocket_s *
NET_Connect(char * host);

qboolean
NET_CanSendMessage(qsocket_t * sock);

int
NET_GetMessage(struct qsocket_s * sock);

int
NET_SendMessage(struct qsocket_s * sock, sizebuf_t * data);
int
NET_SendUnreliableMessage(struct qsocket_s * sock, sizebuf_t * data);

int
NET_SendToAll(sizebuf_t * data, int blocktime);

void
NET_Close(struct qsocket_s * sock);

void
NET_Poll(void);

typedef struct _PollProcedure {
    struct _PollProcedure * next;
    double                  nextTime;
    void (*procedure)(void *);
    void *                  arg;
} PollProcedure;

void
SchedulePollProcedure(PollProcedure * pp, double timeOffset);

extern qboolean serialAvailable;
extern qboolean ipxAvailable;
extern qboolean tcpipAvailable;
extern char my_ipx_address[NET_NAMELEN];
extern char my_tcpip_address[NET_NAMELEN];
extern void (*GetComPortConfig)(int portNumber, int * port, int * irq, int * baud,
  qboolean * useModem);
extern void (*SetComPortConfig)(int portNumber, int port, int irq, int baud, qboolean useModem);
extern void (*GetModemConfig)(int portNumber, char * dialType, char * clear, char * init,
  char * hangup);
extern void (*SetModemConfig)(int portNumber, char * dialType, char * clear, char * init,
  char * hangup);

extern qboolean slistInProgress;
extern qboolean slistSilent;
extern qboolean slistLocal;

void
NET_Slist_f(void);
