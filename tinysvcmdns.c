/*
 * tinysvcmdns - a tiny MDNS implementation for publishing services
 * Copyright (C) 2011 Darell Tan
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <in6addr.h>
#include <ws2tcpip.h>
typedef uint32_t in_addr_t;
#elif defined (linux) || defined (__FreeBSD__)
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#if defined (__FreeBSD__)
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#endif
#elif defined (__APPLE__)
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#endif

#include "mdns.h"
#include "mdnsd.h"

struct mdns_service *svc;
struct mdnsd *svr;

/*---------------------------------------------------------------------------*/
#ifdef WIN32
static void winsock_init(void) {
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	int WSerr = WSAStartup(wVersionRequested, &wsaData);
	if (WSerr != 0) exit(1);
}


/*---------------------------------------------------------------------------*/
static void winsock_close(void) {
	WSACleanup();
}
#endif


/*---------------------------------------------------------------------------*/
#define MAX_INTERFACES 256
#define DEFAULT_INTERFACE 1
#if !defined(WIN32)
#define INVALID_SOCKET (-1)
#endif
static in_addr_t get_localhost(void)
{
#ifdef WIN32
	char buf[INET_ADDRSTRLEN];
	struct hostent *h = NULL;
	struct sockaddr_in LocalAddr;

	memset(&LocalAddr, 0, sizeof(LocalAddr));

	gethostname(buf, INET_ADDRSTRLEN);
	h = gethostbyname(buf);
	if (h != NULL) {
		memcpy(&LocalAddr.sin_addr, h->h_addr_list[0], 4);
		return LocalAddr.sin_addr.s_addr;
	}
	else return INADDR_ANY;
#elif defined (__APPLE__) || defined(__FreeBSD__)
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) return INADDR_ANY;

	/* cycle through available interfaces */
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		/* Skip loopback, point-to-point and down interfaces,
		 * except don't skip down interfaces
		 * if we're trying to get a list of configurable interfaces. */
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
			(!( ifa->ifa_flags & IFF_UP))) {
			continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET) {
			/* We don't want the loopback interface. */
			if (((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr.s_addr ==
				htonl(INADDR_LOOPBACK)) {
				continue;
			}
			return ((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr.s_addr;
			break;
		}
	}
	freeifaddrs(ifap);

	return INADDR_ANY;
#else
	char szBuffer[MAX_INTERFACES * sizeof (struct ifreq)];
	struct ifconf ifConf;
	struct ifreq ifReq;
	int nResult;
	long unsigned int i;
	int LocalSock;
	struct sockaddr_in LocalAddr;
	int j = 0;

	/* purify */
	memset(&ifConf,  0, sizeof(ifConf));
	memset(&ifReq,   0, sizeof(ifReq));
	memset(szBuffer, 0, sizeof(szBuffer));
	memset(&LocalAddr, 0, sizeof(LocalAddr));

	/* Create an unbound datagram socket to do the SIOCGIFADDR ioctl on.  */
	LocalSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (LocalSock == INVALID_SOCKET) return false;
	/* Get the interface configuration information... */
	ifConf.ifc_len = (int)sizeof szBuffer;
	ifConf.ifc_ifcu.ifcu_buf = (caddr_t) szBuffer;
	nResult = ioctl(LocalSock, SIOCGIFCONF, &ifConf);
	if (nResult < 0) {
		close(LocalSock);
		return INADDR_ANY;
	}

	/* Cycle through the list of interfaces looking for IP addresses. */
	for (i = 0lu; i < (long unsigned int)ifConf.ifc_len && j < DEFAULT_INTERFACE; ) {
		struct ifreq *pifReq =
			(struct ifreq *)((caddr_t)ifConf.ifc_req + i);
		i += sizeof *pifReq;
		/* See if this is the sort of interface we want to deal with. */
		memset(ifReq.ifr_name, 0, sizeof(ifReq.ifr_name));
		strncpy(ifReq.ifr_name, pifReq->ifr_name,
			sizeof(ifReq.ifr_name) - 1);
		/* Skip loopback, point-to-point and down interfaces,
		 * except don't skip down interfaces
		 * if we're trying to get a list of configurable interfaces. */
		ioctl(LocalSock, SIOCGIFFLAGS, &ifReq);
		if ((ifReq.ifr_flags & IFF_LOOPBACK) ||
			(!(ifReq.ifr_flags & IFF_UP))) {
			continue;
		}
		if (pifReq->ifr_addr.sa_family == AF_INET) {
			/* Get a pointer to the address...*/
			memcpy(&LocalAddr, &pifReq->ifr_addr,
				sizeof pifReq->ifr_addr);
			/* We don't want the loopback interface. */
			if (LocalAddr.sin_addr.s_addr ==
				htonl(INADDR_LOOPBACK)) {
				continue;
			}
		}
		/* increment j if we found an address which is not loopback
		 * and is up */
		j++;
	}
	close(LocalSock);

	return LocalAddr.sin_addr.s_addr;
#endif
}




	printf("hostname identity type port txt [txt] ... [txt]\n");
#ifdef WIN32
	winsock_close();
#endif
	return 1;
}




	mdnsd_stop(svr);
#ifdef WIN32
	winsock_close();
#endif
	exit(0);
}







	char type[255];
	int port;
	const char **txt;
	struct in_addr host;

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
#if defined(SIGPIPE)
	signal(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGQUIT)
	signal(SIGQUIT, sighandler);
#endif
#if defined(SIGHUP)
	signal(SIGHUP, sighandler);
#endif

#ifdef WIN32
	winsock_init();
#endif

	if (argc < 6) return print_usage();

	port = atoi(argv[4]);

	host.s_addr = get_localhost();
	if (host.s_addr == INADDR_ANY) {





	if (svr == NULL) return print_usage();

	txt = malloc((argc - 5 + 1) * sizeof(char**));
	memcpy(txt, argv + 5, (argc - 5) * sizeof(char**));
	txt[argc + 5] = NULL;

	mdnsd_set_hostname(svr, argv[1], host);

	sprintf(type, "%s.local", argv[3]);

	printf("server   : %s\nidentity : %s\ntype     : %s\n"
		   "ip       : %s\nport     : %u\n",
			argv[1], argv[2], type, inet_ntoa(host), port);

	svc = mdnsd_register_svc(svr, argv[2], type, port, NULL, txt);
	mdns_service_destroy(svc);

#ifdef WIN32
	Sleep(INFINITE);
#else
	pause();
#endif

	mdnsd_stop(svr);

#ifdef WIN32
	winsock_close();
#endif

	return 0;
}
