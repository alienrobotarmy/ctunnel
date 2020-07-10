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

#include "ctunnel.h"

int bin_size(unsigned char *s)
{
	unsigned char *p = s;

	while (*p)
		p++;

	return p - s;
}

void version(void)
{
	fprintf(stdout, "%s %d.%d %s\n", SOFTWARE_NAME, VERSION_MAJOR,
			VERSION_MINOR, COPYRIGHT);
}

void usage(void)
{
	fprintf(stdout, "Compiled with "
#ifdef HAVE_OPENSSL
					"OpenSSL\n"
#else
					"libgcrypt\n"
#endif
#ifndef _WIN32
					"./ctunnel [-V][-U][-k][-n][-i][-v][-h][-b][-e][-z level] -c|-s -l ip:port -f ip:port -C cipher\n"
					"  -V\t(optional) Operate as VPN, otherwise just tunnel\n"
					"    -t #  (requires -V) TUN device number.\n"
					"    -i IP (requires -V) VPN PTP Pool (10.0.5.0 default)\n"
					"    -r A  (requires -V) Remote networks to route over VPN (comma sperated)\n"
					"    -P '/usr/sbin/pppd nodetach noauth' PPP VPN Mode\n"
#else
					"./ctunnel [-U][-k][-n][-i][-v][-h][-b][-e][-z level] -c|-s -l ip:port -f ip:port -C cipher\n"
#endif
					"  -U\t(optional) Use UDP (default is TCP)\n"
					"  -n\t(optional) Do not fork into Background\n"
					"  -p\t(optional) Print Stored Key, IV, and Cipher then exit\n"
					"  -v\t(optional) Print Version information then exit\n"
					"  -h\t(optional) Print the screen you are reading then exit\n"
					"  -e\t(optional) Post Connection UP exec\n"
					"  -z #  (optional) Enable Compression at level #, 5 default.\n"
					"  -b #  (optional) Packet buffer size in bytes (%d) by default.\n"
					"  -k\t(optional) Speficy non-default Key File.\n"
					"  -S\t(optional) Display traffic stats.\n"
					"  -c\t(manditory) Operate in Client Mode (do not use with -s)\n"
					"  -s\t(manditory) Operate in Server Mode (do not use with -c)\n"
					"  -l\t(manditory) Listen for connections on this ip:port\n"
					"  -f\t(manditory) Forward connection from -l to this ip:port\n"
#ifndef _WIN32
					"                  VPN Mode: connect to this address\n"
#endif
					"  -C\t(manditory) Encrypt traffic with this cipher\n"
#ifndef HAVE_OPENSSL
					"  -M\t(manditory) Cipher Mode (ecb,cfb,cbc,ofb) cfb is recommended\n\n"
#endif
					"See ctunnel(1) for examples\n",
			PACKET_SIZE);
}

struct options get_options(int argc, char *argv[])
{
	int i = 0, x = 0, ret = 0;
	struct options opt;
	struct JToken tok;
#ifndef HAVE_OPENSSL
	crypto_ctx ctx;
#endif

	if (argc <= 1)
	{
		usage();
		exit(0);
	}

	opt.vpn = 0;
	opt.port_listen = -1;
	opt.port_forward = -1;
	opt.listen = 0;
	opt.forward = 0;
	opt.role = -1;
	opt.daemon = -1;
	opt.proto = TCP; /* Default to TCP if nothing specified */
	opt.comp = 0;
	opt.comp_level = 5;
	opt.tun = 0;
	opt.proxy = 0;
	opt.printkey = 0;
	opt.packet_size = PACKET_SIZE;
	opt.keyfile = NULL;
	opt.networks = NULL;
	opt.pexec = NULL;
	opt.ppp = NULL;
	opt.vmode = VMODE_TUN;
	opt.stats = 0;
	memset(opt.key.key, 0x00, sizeof(opt.key.key));
	memset(opt.key.iv, 0x00, sizeof(opt.key.iv));
	memset(opt.cipher, 0x00, sizeof(opt.cipher));
	memset(opt.pool, 0x00, sizeof(opt.pool));
#ifndef HAVE_OPENSSL
	memset(opt.mode, 0x00, sizeof(opt.mode));
#endif

	for (i = 0; argv[i] != NULL; i++)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 'V':
				opt.vpn = 1;
				break;
			case 'p':
				opt.printkey = 1;
				exit(0);
				break;
			case 's':
				opt.role = 1;
				break;
			case 'c':
				opt.role = 0;
				break;
			case 'n':
				opt.daemon = 1;
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case 'v':
				version();
				break;
			case 'U':
				opt.proto = UDP;
				break;
			case 'z':
				opt.comp = 1;
				break;
			case 'S':
				opt.stats = 1;
				break;
			}
		}
	}
	for (i = 0; argv[i] != NULL; i++)
	{
		if (argv[i][0] == '-' && argv[i + 1])
		{
			switch (argv[i][1])
			{
			case 'l':
				opt.listen = 1;
				if (argv[i + 1][0] != '-')
				{
					tok.match = NULL;
					jtok_init(&tok);
					ret = jtok(&tok, argv[i + 1], ':');
					if (ret == 0)
					{
						fprintf(stderr, "Must specify hostname:port\n");
						exit(1);
					}
					opt.local.ip = strdup(tok.match);
					ret = jtok(&tok, argv[i + 1], ':');
					if (ret == 0)
					{
						fprintf(stderr, "Must specify hostname:port\n");
						exit(1);
					}
					opt.local.ps = atoi(tok.match);
					jtok_init(&tok);
				}
				break;
			case 'k':
				if (argv[i + 1][0] != '-')
					opt.keyfile = strdup(argv[i + 1]);
				break;
			case 'f':
				opt.forward = 1;
				if (argv[i + 1][0] != '-')
				{
					tok.match = NULL;
					jtok_init(&tok);
					ret = jtok(&tok, argv[i + 1], ':');
					if (ret == 0)
					{
						fprintf(stderr, "Must specify hostname:port\n");
						exit(1);
					}
					opt.remote.ip = strdup(tok.match);
					ret = jtok(&tok, argv[i + 1], ':');
					if (ret == 0)
					{
						fprintf(stderr, "Must specify hostname:PORT\n");
						exit(1);
					}
					opt.remote.ps = atoi(tok.match);
					jtok_init(&tok);
				}
				break;
			case 'C':
				if (argv[i + 1][0] != '-')
					strcpy(opt.cipher, argv[i + 1]);
				if ((!strcmp(opt.cipher, "proxy")) ||
					(!strcmp(opt.cipher, "plain")))
					opt.proxy = 1;
				break;
#ifndef HAVE_OPENSSL
			case 'M':
				if (argv[i + 1][0] != '-')
					strcpy(opt.mode, argv[i + 1]);
				break;
#else
			case 'M':
				fprintf(stderr, "Cipher and Mode are specified with -C "
								"only for OpenSSL\n");
				exit(1);
				break;
#endif
			case 'z':
				if (argv[i + 1][0] != '-')
					opt.comp_level = atoi(argv[i + 1]);
				break;
			case 'b':
				if (argv[i + 1][0] != '-')
					opt.packet_size = atoi(argv[i + 1]);
				break;
			case 'r':
				if (argv[i + 1][0] != '-')
					opt.networks = strdup(argv[i + 1]);
				break;
			case 'e':
				if (argv[i + 1][0] != '-')
					opt.pexec = strdup(argv[i + 1]);
				break;
			case 'i':
				if (argv[i + 1][0] != '-')
					strncpy(opt.pool, argv[i + 1], 32);
				break;
			case 'P':
				if (argv[i + 1][0] != '-')
				{
					opt.vmode = VMODE_PPP;
					opt.ppp = strdup(argv[i + 1]);
				}
				break;
			case 't':
				if (argv[i + 1][0] != '-')
					opt.tun = atoi(argv[i + 1]);
				break;
			}
		}
	}
	if (opt.listen != 1 || opt.forward != 1)
	{
		fprintf(stdout,
				"ERROR: Both -l and -f must be set\n");
		exit(0);
	}
	if (opt.role < 0)
	{
		fprintf(stdout,
				"ERROR: Please specify Client of Server mode with -c or -s\n");
		exit(0);
	}
	if (opt.comp_level > 9 || opt.comp_level < 0)
	{
		fprintf(stdout,
				"ERROR: Compression level must be between 0 and 9\n");
		exit(0);
	}
	if (opt.packet_size < 1)
	{
		fprintf(stdout, "ERROR: Packet size must be greater than 0\n");
		exit(0);
	}
	if (opt.vpn == 1)
	{
		opt.packet_size = opt.packet_size * sizeof(unsigned char *) + sizeof(int);
	}
	if ((strlen(opt.pool)) < 1)
	{
		strcpy(opt.pool, "10.0.5");
	}
	if (opt.proxy == 1)
		return opt;

	if ((keyfile_read(&opt, opt.keyfile)) < 0)
	{
		ctunnel_log(stderr, LOG_CRIT, "keyfile: %s", strerror(errno));
		exit(1);
	}

	if ((bin_size(opt.key.key)) < 16)
	{
		fprintf(stdout, "Enter Key [16 Characters]: ");
		for (x = 0; x < 16; x++)
		{
			opt.key.key[x] = fgetc(stdin);
		}
		fgetc(stdin);
	}
	if ((bin_size(opt.key.iv)) < 16)
	{
		fprintf(stdout, "Enter IV  [16 Characters]: ");
		for (x = 0; x < 16; x++)
		{
			opt.key.iv[x] = fgetc(stdin);
		}
		fgetc(stdin);
	}

	/* Check ciphers/modes */
#ifndef HAVE_OPENSSL
	if ((strlen(opt.mode)) > 0)
	{
		if (!strcmp(opt.mode, "ecb"))
			opt.key.mode = GCRY_CIPHER_MODE_ECB;
		else if (!strcmp(opt.mode, "cfb"))
			opt.key.mode = GCRY_CIPHER_MODE_CFB;
		else if (!strcmp(opt.mode, "cbc"))
			opt.key.mode = GCRY_CIPHER_MODE_CBC;
		else if (!strcmp(opt.mode, "ofb"))
			opt.key.mode = GCRY_CIPHER_MODE_OFB;
		else
			opt.key.mode = 0;
	}
	else
	{
		fprintf(stdout, "ERROR: Please specify Cipher Mode with -M\n");
		exit(0);
	}
#endif

	if ((strlen(opt.cipher)) > 0)
		strcpy(opt.key.ciphername, opt.cipher);

#ifdef HAVE_OPENSSL
	OpenSSL_add_all_ciphers();
	opt.key.cipher = EVP_get_cipherbyname(opt.cipher);
	if (!opt.key.cipher)
	{
		ctunnel_log(stderr, LOG_CRIT,
					"[OpenSSL] Cipher \"%s\" not found!", opt.cipher);
		exit(1);
	}
	EVP_cleanup();
#else
	if ((gcry_cipher_map_name(opt.cipher)) == 0)
	{
		ctunnel_log(stderr, LOG_CRIT,
					"[libgcrypt] Cipher \"%s\" not found!", opt.cipher);
		exit(1);
	}
	opt.key.cipher = gcry_cipher_map_name(opt.cipher);
	if (opt.key.cipher == 0)
	{
		ctunnel_log(stderr, LOG_CRIT, "gcry_cipher_map_name: %s",
					gpg_strerror(ret));
		gcry_cipher_close(ctx);
		exit(1);
	}
	ret = gcry_cipher_open(&ctx, opt.key.cipher, opt.key.mode, 0);
	if (!ctx)
	{
		ctunnel_log(stderr, LOG_CRIT, "gcry_open_cipher: %s",
					gpg_strerror(ret));
		gcry_cipher_close(ctx);
		exit(1);
	}
	gcry_cipher_close(ctx);
#endif

	if ((bin_size((unsigned char *)opt.key.ciphername)) <= 0)
	{
		fprintf(stdout, "ERROR: Please specify Stream Cipher with -C\n");
		exit(0);
	}
	keyfile_write(&opt, opt.keyfile);

	return opt;
}
