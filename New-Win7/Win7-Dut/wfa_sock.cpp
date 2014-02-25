 
/****************************************************************************
 *  (c) Copyright 2007 Wi-Fi Alliance.  All Rights Reserved
 *
 *
 *  LICENSE
 *
 *  License is granted only to Wi-Fi Alliance members and designated
 *  contractors ($B!H(BAuthorized Licensees$B!I(B)..AN  Authorized Licensees are granted
 *  the non-exclusive, worldwide, limited right to use, copy, import, export
 *  and distribute this software:
 *  (i) solely for noncommercial applications and solely for testing Wi-Fi
 *  equipment; and
 *  (ii) solely for the purpose of embedding the software into Authorized
 *  Licensee$B!G(Bs proprietary equipment and software products for distribution to
 *  its customers under a license with at least the same restrictions as
 *  contained in this License, including, without limitation, the disclaimer of
 *  warranty and limitation of liability, below..AN  The distribution rights
 *  granted in clause
 *  (ii), above, include distribution to third party companies who will
 *  redistribute the Authorized Licensee$B!G(Bs product to their customers with or
 *  without such third party$B!G(Bs private label. Other than expressly granted
 *  herein, this License is not transferable or sublicensable, and it does not
 *  extend to and may not be used with non-Wi-Fi applications..AN  Wi-Fi Alliance
 *  reserves all rights not expressly granted herein..AN 
 *.AN 
 *  Except as specifically set forth above, commercial derivative works of
 *  this software or applications that use the Wi-Fi scripts generated by this
 *  software are NOT AUTHORIZED without specific prior written permission from
 *  Wi-Fi Alliance.
 *.AN 
 *  Non-Commercial derivative works of this software for internal use are
 *  authorized and are limited by the same restrictions; provided, however,
 *  that the Authorized Licensee shall provide Wi-Fi Alliance with a copy of
 *  such derivative works under a perpetual, payment-free license to use,
 *  modify, and distribute such derivative works for purposes of testing Wi-Fi
 *  equipment.
 *.AN 
 *  Neither the name of the author nor "Wi-Fi Alliance" may be used to endorse
 *  or promote products that are derived from or that use this software without
 *  specific prior written permission from Wi-Fi Alliance.
 *
 *  THIS SOFTWARE IS PROVIDED BY WI-FI ALLIANCE "AS IS" AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY, NON-INFRINGEMENT AND FITNESS FOR A.AN PARTICULAR PURPOSE,
 *  ARE DISCLAIMED. IN NO EVENT SHALL WI-FI ALLIANCE BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, THE COST OF PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE) ARISING IN ANY WAY OUT OF
 *  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. ******************************************************************************
 */

/*
 *   Revision History: WINv03.00.00 - Simga 3.0 Release, supports TGn Program including WMM and WPA2
 */
#include "stdafx.h"
#ifdef _WINDOWS
#include <winsock2.h>
#else
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef _CYGWIN
#include <cygwin/if.h>
#else
#include <linux/if.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sched.h>
#endif

#include "wfa_debug.h"
#include "wfa_types.h"
#include "wfa_main.h"
#include "wfa_sock.h"

int wfaGetifAddr(char *ifname, struct sockaddr_in *sa);

extern unsigned short wfa_defined_debug;

extern BOOL gtgTransac;
extern char gnetIf[];

#define MAXPENDING 2    /* Maximum outstanding connection requests */

/*
 * wfaCreateTCPServSock(): initially create a TCP socket
 * intput:   port -- TCP socket port to listen
 * return:   socket id;
 */
int wfaCreateTCPServSock(unsigned short port)
{
    int sock;                        /* socket to create */
    struct sockaddr_in servAddr; /* Local address */
    const int on = 1;
#ifdef _WINDOWS
 //WSADATA is a struct that is filled up by the call 
    //to WSAStartup
    WSADATA wsaData;
    BOOL bOpt = TRUE;
    //The sockaddr_in specifies the address of the socket
    //for TCP/IP sockets. Other protocols use similar structures.
    //WSAStartup initializes the program for calling WinSock.
    //The first parameter specifies the highest version of the 
    //WinSock specification, the program is allowed to use.

    int wsaret=WSAStartup(0x101,&wsaData);
    //WSAStartup returns zero on success.

    //If it fails we exit.

    if(wsaret!=0)
    {
        DPRINT_ERR(WFA_ERR, "createTCPServSock socket() failed");
        return WFA_FAILURE;
    }
#endif

    /* Create socket for incoming connections */
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        DPRINT_ERR(WFA_ERR, "createTCPServSock socket() failed");
        return WFA_FAILURE;
    }

    setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&bOpt,sizeof(BOOL));
      
    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));
    wfaGetifAddr(gnetIf, &servAddr);
    servAddr.sin_family = AF_INET;        /* Internet address family */
    //servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    servAddr.sin_port = htons(port);              /* Local port */
#ifdef resolve
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif
    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
    {
        DPRINT_ERR(WFA_ERR, "bind() failed");
        return WFA_FAILURE;
    }

    /* Mark the socket so it will listen for incoming connections */
    if (listen(sock, MAXPENDING) < 0)
    {
        DPRINT_ERR(WFA_ERR, "listen() failed");
        return WFA_FAILURE;
    }

    return sock;
}

/*
 * wfaCreateUDPSock(): create a UDP socket
 * input:     
       ipaddr -- local ip address for test traffic
       port -- UDP port to receive and send
 * return:    socket id
 */
int wfaCreateUDPSock(char *ipaddr, unsigned short port)
{
    int udpsock;                        /* socket to create */
    struct sockaddr_in servAddr; /* Local address */
#ifdef _WINDOWS
    WSADATA wsadata;
    int wsaret=WSAStartup(MAKEWORD(2,2),&wsadata);
 
    if(wsaret!=0)
    {
        int errsv = WSAGetLastError();
        DPRINT_ERR(WFA_ERR, "createUDPSock socket() falled with error %d",errsv);
        return WFA_FAILURE;
    }
#endif
    if((udpsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        int errsv = WSAGetLastError();
        DPRINT_ERR(WFA_ERR, "createUDPSock socket() failed with error %d for port %d",errsv,port);
        //DPRINT_ERR(WFA_ERR, "createUDPSock socket() failed");
        return WFA_FAILURE;
    }
#ifndef _WINDOWS
    bzero(&servAddr, sizeof(servAddr));
#endif
    servAddr.sin_family      = AF_INET;
    //inet_aton(ipaddr, &servAddr.sin_addr);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port        = htons(port);

    bind(udpsock, (struct sockaddr *) &servAddr, sizeof(servAddr)); 

    return udpsock;
}

#define IP_MULTICAST_TTL    3           /* set/get IP multicast timetolive  */
#define IP_ADD_MEMBERSHIP   5           /* add  an IP group membership      */
/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct ip_mreq {
        struct in_addr  imr_multiaddr;  /* IP multicast address of group */
        struct in_addr  imr_interface;  /* local IP address of interface */
};

int wfaSetSockMcastSendOpt(int sockfd)
{
    unsigned char ttlval = 1;

    return setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL,(const char *) &ttlval, sizeof(ttlval));
}

int wfaSetSockMcastRecvOpt(int sockfd, char *mcastgroup)
{
    struct ip_mreq mcreq;
    int so;

    mcreq.imr_multiaddr.s_addr = inet_addr(mcastgroup);
    mcreq.imr_interface.s_addr = htonl(INADDR_ANY);
    so = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                  (const char *)&mcreq, sizeof(mcreq));

    return so;
}

int wfaConnectUDPPeer(int mysock, char *daddr, int dport)
{
    struct sockaddr_in peerAddr;

    memset(&peerAddr, 0, sizeof(peerAddr));
    peerAddr.sin_family = AF_INET;                
#ifdef resolve
    inet_aton(daddr, &peerAddr.sin_addr);
#endif
    peerAddr.sin_port   = htons(dport);    

    connect(mysock, (struct sockaddr *)&peerAddr, sizeof(peerAddr));
    return mysock;
}

/*
 * acceptTCPConn(): handle and accept any incoming socket connection request.
 * input:        serSock -- the socket id to listen
 * return:       the connected socket id.
 */
int wfaAcceptTCPConn(int servSock)
{
    int clntSock;                /* Socket descriptor for client */
    struct sockaddr_in clntAddr; /* Client address */
    unsigned int clntLen;        /* Length of client address data structure */

    /* Set the size of the in-out parameter */
    clntLen = sizeof(clntAddr);
    
    /* Wait for a client to connect */
    if ((clntSock = accept(servSock, (struct sockaddr *) &clntAddr, 
           (int *)&clntLen)) < 0)
    {
        DPRINT_ERR(WFA_ERR, "accept() failed");
        exit(1);
    }
    
    /* clntSock is connected to a client! */
    return clntSock;
}

struct timeval *wfaSetTimer(int secs, int usecs, struct timeval *tv)
{
   struct timeval *mytv;
 
   if(gtgTransac != 0)
   {
      tv->tv_sec = secs ;             /* timeout (secs.) */
      tv->tv_usec = usecs;            /* 0 microseconds */
   }
   else
   {
      tv->tv_sec =  0;
      tv->tv_usec = 0;                /* 0 microseconds */
   }

   if(tv->tv_sec == 0 && tv->tv_usec == 0)
      mytv = NULL;
   else
      mytv = tv;

   return mytv; 
}

/* this only set three file descriptors, the main agent fd, control agent
 * port fd and traffic generation fd.
 */ 
void wfaSetSockFiDesc(fd_set *fdset, int *maxfdn1, struct sockfds *fds) 
{
    FD_ZERO(fdset);
    if(fdset != NULL)
       FD_SET(*fds->agtfd, fdset);

    /* if the traffic generator socket port valid */
#if 0
    if(*fds->tgfd > 0)
    {
         FD_SET(*fds->tgfd, fdset);
         *maxfdn1 = max(*maxfdn1-1, *fds->tgfd) + 1;
    }
#endif

    /* if the control agent socket fd valid */
    if(*fds->cafd >0)
    {
         FD_SET(*fds->cafd, fdset);
         *maxfdn1 = max(*maxfdn1-1, *fds->cafd) + 1;
    }

#ifdef WFA_WMM_EXT
#ifdef WFA_WMM_PS_EXT
    /* if the power save socket port valid */
    if(*fds->psfd > 0)
    {
         FD_SET(*fds->psfd, fdset);
         *maxfdn1 = max(*maxfdn1-1, *fds->psfd) + 1;
    }
#endif

#if 0
    /* if any of wmm traffic stream socket fd valid */
    for(i = 0; i < WFA_MAX_WMM_STREAMS; i++)
    {
        if(fds->wmmfds[i] > 0)
        {
            FD_SET(fds->wmmfds[i], fdset);
            *maxfdn1 = max(*maxfdn1-1, fds->wmmfds[i]) +1;
        }
    }
#endif
#endif

    return;
} 

/*
 * wfaCtrlSend(): Send control message/response through
 *                control link. 
 *  Note: the function used to wfaTcpSend().
 */
#ifdef _WINDOWS
int wfaCtrlSend(SOCKET sock, unsigned char *buf, int bufLen)
#else
int wfaCtrlSend(int sock, unsigned char *buf, int bufLen)
#endif
{
    int bytesSent = 0;

    // for send start message, it could be "0" Len
    if(bufLen == 0)
    {
        DPRINT_WARNING(WFA_WNG, "Buffer Len is 0\n");
        return bufLen;
    }

    bytesSent = send(sock,( const char *)buf, bufLen, 0); 

    if(bytesSent == SOCKET_ERROR)
    {
       DPRINT_WARNING(WFA_WNG, "Error sending tcp packet %d\n", WSAGetLastError());
    }

    return bytesSent;
}

/*
 * wfaCtrlRecv(): Receive control message/response through
 *                control link. 
 *  Note: the function used to wfaTcpRecv().
 */
int wfaCtrlRecv(int sock, unsigned char *buf)
{
   int bytesRecvd = 0;
   bytesRecvd = recv(sock,(char *) buf, WFA_BUFF_4K-1, 0);

   return bytesRecvd; 
}

/*
 * wfaTrafficSendTo(): Send Traffic through through traffic interface. 
 *  Note: the function used to wfaUdpSendTo().
 */
int wfaTrafficSendTo(int sock, char *buf, int bufLen, struct sockaddr *to) 
{
   int bytesSent;

   bytesSent = sendto(sock, buf, bufLen, 0, to, sizeof(struct sockaddr));

   return bytesSent;
}

/*
 * wfaTrafficRecv(): Receive Traffic through through traffic interface. 
 *  Note: the function used to wfaRecvSendTo().
 */
int wfaTrafficRecv(int sock, char *buf, struct sockaddr *from)
{
   int bytesRecvd =0;
   int  addrLen=sizeof(struct sockaddr);

#ifndef _WINDOWS
   /* get current flags setting */
   int ioflags = fcntl(sock, F_GETFL, 0);

   /* set only BLOCKING flag to non-blocking */ 
   fcntl(sock, F_SETFL, ioflags | O_NONBLOCK);
#else
   int NonBlock=0; // 0 is blocking
   if (ioctlsocket(sock, FIONBIO, (u_long *)&NonBlock) == SOCKET_ERROR)
   {
      return SOCKET_ERROR;
   }
#endif
   /* check the buffer to avoid an uninitialized pointer - bugz 159 */
   if(buf == NULL)
   {
       DPRINT_ERR(WFA_ERR, "Uninitialized buffer\n");
       return WFA_FAILURE;
   }

//   bytesRecvd = recvfrom(sock, buf, MAX_RCV_BUF_LEN, 0, from, &addrLen);
     bytesRecvd = recv(sock, buf, MAX_RCV_BUF_LEN, 0); 

   return bytesRecvd;
}

int wfaGetifAddr(char *ifname, struct sockaddr_in *sa)
{
#ifdef resolve
    struct ifreq ifr;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if(fd < 0)
    {
       DPRINT_ERR(WFA_ERR, "socket open error\n");
       return WFA_FAILURE;
    }

    strcpy(ifr.ifr_name, ifname);

    ifr.ifr_addr.sa_family = AF_INET;
    if(ioctl(fd, SIOCGIFADDR, &ifr) == 0)
    {
         memcpy(sa, (struct sockaddr_in *)&ifr.ifr_addr, sizeof(struct sockaddr_in));
    }
    else
    {
         return WFA_FAILURE;
    }

    close(fd);
#endif
    return WFA_FAILURE;
}

/*
 * wfaSetProcPriority():
 * With the linux 2.6 kernel, it allows an application process dynamically 
 * adjust its running priority level. In order to achieve higher control of 
 * packet sending/receiving and timer response, it is helpful to raise the 
 * process priority level over others and lower it back once it finishes. 
 * 
 * This is purely used for performance tuning purpose and not required to
 * port if it is not needed.
 */
#ifdef resolve
int wfaSetProcPriority(int set)
{
    int maxprio, currprio;
    struct sched_param schp;

    memset(&schp, 0, sizeof(schp));
    sched_getparam(0, &schp);

    currprio = schp.sched_priority;

    if(set != 0)
    {
        if(geteuid() == 0)
        {
           maxprio = sched_get_priority_max(SCHED_FIFO);
           if(maxprio == -1)
           {
              return WFA_FAILURE;
           }

           schp.sched_priority = maxprio;
           if(sched_setscheduler(0, SCHED_FIFO, &schp) != 0)
           {
           }
        }
       
        if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        {

        }
    } 
    else
    {
        if(geteuid() == 0)
        {
           schp.sched_priority = 0;
           if(sched_setscheduler(0, SCHED_OTHER, &schp) != 0)
           {
           }
        }
       
        munlockall();
    }

    return currprio;
}
#endif
