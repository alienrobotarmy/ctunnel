/*
 * Ctunnel - Cyptographic tunnel.
 * Copyright (C) 2008-2020 Jess Mahan <ctunnel@alienrobotarmy.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#include "ctunnel.h"

void crypto_shutdown(int sig)
{
	ctunnel_log(stdout, LOG_INFO, "Signal(%d) received Shutting Down\n", sig);
#ifdef HAVE_OPENSSL
	EVP_cleanup();
#endif
	exit(0);
}

int main(int argc, char *argv[])
{
#ifndef _WIN32
	pid_t pid;
#endif
	struct options opt;

	version();
	opt = get_options(argc, argv);

#ifndef _WIN32
	openlog("ctunnel", LOG_NDELAY | LOG_PID, LOG_DAEMON);
	ctunnel_log(stdout, LOG_INFO, "ctunnel %d.%d starting", VERSION_MAJOR,
				VERSION_MINOR);
#endif

	//memset(opt.host, 0x00, sizeof(opt.host));

#ifndef _WIN32
	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, crypto_shutdown);
	signal(SIGTERM, crypto_shutdown);
#endif

	if (opt.proto == TCP)
	{
		if (opt.proxy == 0)
		{
#ifdef HAVE_OPENSSL
			if (opt.vpn == 0)
				ctunnel_log(stdout, LOG_INFO,
							"TCP [OpenSSL] Listening on %s:%d using %s",
							opt.local.ip, opt.local.ps, opt.key.ciphername);
			else
				ctunnel_log(stdout, LOG_INFO,
							"TCP [OpenSSL] VPN using %s", opt.key.ciphername);
#else
			if (opt.vpn == 0)
				ctunnel_log(stdout, LOG_INFO,
							"TCP [libgcrypt] Listening on %s:%d using %s %s",
							opt.local.ip, opt.local.ps, opt.key.ciphername,
							opt.mode);
			else
				ctunnel_log(stdout, LOG_INFO,
							"TCP [libgcrypt] VPN using %s %s",
							opt.key.ciphername, opt.mode);

#endif
		}
		else
		{
			ctunnel_log(stdout, LOG_INFO, "TCP [PROXY] Listening on %s:%d",
						opt.local.ip, opt.local.ps);
		}
	}
	else
	{
		if (opt.proxy == 0)
		{
#ifdef HAVE_OPENSSL
			if (opt.vpn == 0)
				ctunnel_log(stdout, LOG_INFO,
							"UDP [OpenSSL] Listening on %s:%d using %s",
							opt.local.ip, opt.local.ps, opt.key.ciphername);
			else
				ctunnel_log(stdout, LOG_INFO,
							"UDP [OpenSSL] VPN using %s", opt.key.ciphername);
#else
			if (opt.vpn == 0)
				ctunnel_log(stdout, LOG_INFO,
							"UDP [libgcrypt] Listening on %s:%d using %s %s",
							opt.local.ip, opt.local.ps, opt.key.ciphername,
							opt.mode);
			else
				ctunnel_log(stdout, LOG_INFO,
							"UDP [libgcrypt] VPN using %s %s",
							opt.key.ciphername, opt.mode);
#endif
		}
		else
		{
			ctunnel_log(stdout, LOG_INFO, "UDP [PROXY] Listening on %s:%d",
						opt.local.ip, opt.local.ps);
		}
	}

	if (opt.comp == 1)
		ctunnel_log(stdout, LOG_INFO,
					"zlib compression enabled [level %d]", opt.comp_level);

#ifndef _WIN32
	if (opt.daemon != 1)
	{
		ctunnel_log(stdout, LOG_INFO, "Going into background\n");
		pid = fork();
		if (pid == 0)
			setsid();
		if (pid == -1)
		{
			ctunnel_log(stdout, LOG_CRIT, "fork() %s\n", strerror(errno));
			exit(1);
		}
		if (pid > 0)
		{
			exit(0);
		}
	}
#endif

	if (opt.vpn == 0)
		tunnel_loop(&opt);
#ifdef HAVE_TUNTAP
	else
		vpn_loop(opt);
#endif

	return 0;
}
