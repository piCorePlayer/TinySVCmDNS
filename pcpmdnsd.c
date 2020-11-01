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

#ifdef _WIN32
#include <winsock2.h>
#include <in6addr.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include "mdns.h"
#include "mdnsd.h"


static void usage(const char *argv0) {
   printf("Usage: %s [options]\n"
         "  -h <hostname>\t\tmdns hostname for device.  i.e. pcp.local\n"
         "  -i <ip address of pcp>\tSpecify the ip address of pcp.  i.e. 192.168.0.10\n"
		 "  -t for license terms\n"
		 , argv0);
}

static void license(void) {
	printf("\n"
 	"* pcpmdnsd - tiny MDNS daemon for piCorePlayer.\n"
	"* Copyright (C) 2020 pCP Team under the same license terms.\n"
	"\n"
	"* Conains:\n" 
	"* tinysvcmdns - a tiny MDNS implementation for publishing services\n"
	"* Copyright (C) 2011 Darell Tan\n"
	"* All rights reserved.\n"
	"\n"
	"* Licensed under the 3-clause (\"modified\") BSD License.\n"
	"* Redistribution and use in source and binary forms, with or without\n"
	"* modification, are permitted provided that the following conditions\n"
	"* are met:\n"
	"* 1. Redistributions of source code must retain the above copyright\n"
	"*    notice, this list of conditions and the following disclaimer.\n"
	"* 2. Redistributions in binary form must reproduce the above copyright\n"
	"*    notice, this list of conditions and the following disclaimer in the\n"
	"*    documentation and/or other materials provided with the distribution.\n"
	"* 3. The name of the author may not be used to endorse or promote products\n"
	"*    derived from this software without specific prior written permission.\n"
	"*\n"
	"* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR\n"
	"* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES\n"
	"* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n"
	"* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,\n"
	"* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT\n"
	"* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
	"* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
	"* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
	"* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF\n"
	"* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
	);
}

struct mdnsd *svr;
struct mdns_service *svc_http;
struct mdns_service *svc_ssh;
struct mdns_service *svc_deviceinfo;
int running = 1;

static void sighandler(int signum) {

	mdns_service_destroy(svc_http);
	mdns_service_destroy(svc_ssh);
	mdns_service_destroy(svc_deviceinfo);
	mdnsd_stop(svr);
	running = 0;
}

int main(int argc, char **argv) {

#define MAXCMDLINE 512
	char cmdline[MAXCMDLINE] = "";
	char *optarg = NULL;
	int optind = 1;
	int i;
	char *host = NULL;
	char *hostname = NULL;

	struct in_addr host_ip;
	char *ipv4addr = NULL;
	char *local = ".local";

	for (i = 0; i < argc && (strlen(argv[i]) + strlen(cmdline) + 2 < MAXCMDLINE); i++) {
		strcat(cmdline, argv[i]);
		strcat(cmdline, " ");
	}

	while (optind < argc && strlen(argv[optind]) >= 2 && argv[optind][0] == '-') {
		char *opt = argv[optind] + 1;
		if (strstr("hi", opt) && optind < argc - 1) {
			optarg = argv[optind + 1];
			optind += 2;
		} else if (strstr("t?", opt)){
			optarg = NULL;
			optind +=1;
		} else {
	 		fprintf(stderr, "\nOption error: -%s\n\n", opt);
			usage(argv[0]);
			exit(1);
		}
		switch (opt[0]){
			case 'h':
				// create host entries
				//char *hostname = "mypCP.local";

				if (!strstr(optarg, local)){
					host = optarg;
					hostname = malloc( strlen(optarg) + 8);
					strcat(strcat( hostname, optarg), local);
//					printf("1:%s:%s\n",host,hostname);
				} else {
					hostname = malloc( strlen(optarg) + 1);
					strcat( hostname, optarg);
					host = strtok( optarg, ".");
//					printf("2:%s:%s\n",host,hostname);
				}
				for(int i = 0;i<strlen(hostname); i++){
				  hostname[i] = tolower(hostname[i]);
				}
				break;
	 		case 'i':
				ipv4addr = optarg;
				host_ip.s_addr = inet_addr(ipv4addr);
				break;
	 		case '?':
				usage(argv[0]);
				exit(0);
			case 't':
				license();
				exit(0);
				break;
			default:
				fprintf(stderr, "Argument error: %s\n", argv[optind]);
				break;
		}
	}

	if ( (!strcmp(hostname,"")) || (ipv4addr == NULL)) {
		fprintf(stderr, "Required Arguments missing.\n");
		usage(argv[0]);
		exit(1);
	}

	svr = mdnsd_start( host_ip);
	if (svr == NULL) {
		fprintf(stderr,"mdnsd_start() error\n");
		return 1;
	}

//	printf("mdnsd_start OK. press ENTER to add hostname & service\n");
//	getchar();


	mdnsd_set_hostname(svr, hostname, host_ip);

	struct rr_entry *a2_e = NULL;
	a2_e = rr_create_a(create_nlabel(hostname), host_ip);
	mdnsd_add_rr(svr, a2_e);

/*	This is for ipv6, which we don't need in pCP
	struct rr_entry *aaaa_e = NULL;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_flags = AI_NUMERICHOST;
	struct addrinfo* results;
	getaddrinfo(
		"fe80::e2f8:47ff:fe20:28e0",
		NULL,
		&hints,
		&results);
	struct sockaddr_in6* addr = (struct sockaddr_in6*)results->ai_addr;
	struct in6_addr v6addr = addr->sin6_addr;
	freeaddrinfo(results);

	aaaa_e = rr_create_aaaa(create_nlabel(hostname), &v6addr);

	mdnsd_add_rr(svr, aaaa_e);
*/
	char adminurl[80]= "";
	snprintf(adminurl, sizeof(adminurl), "%s%s", "adminurl=http://", hostname);

	const char *txt[] = {
		adminurl,
		NULL
	};
	svc_http = mdnsd_register_svc(svr, host,
									"_http._tcp.local", 80, NULL, txt);

	const char *txt1[] = {
	   adminurl,
		NULL
	};
	svc_ssh = mdnsd_register_svc(svr, host,
									"_sftp-ssh._tcp.local", 22, NULL, txt1);

	char modelname[80]="";
	snprintf(modelname, sizeof(modelname), "%s%s", "name=", hostname);

	const char *txt2[] = {
		"model=piCorePlayer Audio Device",
		NULL
	};
	svc_deviceinfo = mdnsd_register_svc(svr, host,
	                                    "_device-info._tcp.local", 0, NULL, txt2);

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGHUP, sighandler);

	while (running){
		sleep(1);
	}
//	fprintf(stderr,"Exiting...\n");
	free(hostname);
	exit(0);
}
