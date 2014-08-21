/*
 * Ctunnel - Cyptographic tunnel.
 * Copyright (C) 2008-2014 Jess Mahan <ctunnel@nardcore.org>
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

#ifdef HAVE_OPENSSL
crypto_ctx *openssl_crypto_init(struct options opt, int dir)
{
    crypto_ctx *ctx = calloc(1, sizeof(crypto_ctx));

    if (opt.proxy == 0) {

    /* STREAM CIPHERS ONLY */
    EVP_CIPHER_CTX_init(ctx);
    EVP_CipherInit_ex(ctx, opt.key.cipher, NULL,
                       opt.key.key, opt.key.iv, dir);
    /* Encrypt final for UDP? */
    EVP_CIPHER_CTX_set_padding(ctx, 1);
    }

    return ctx;

}
void openssl_crypto_reset(crypto_ctx *ctx, struct options opt, int dir)
{
    if (opt.proxy == 0) {
    if (dir == 1)
	EVP_EncryptInit_ex(ctx, opt.key.cipher, NULL, opt.key.key, opt.key.iv);
    else
	EVP_DecryptInit_ex(ctx, opt.key.cipher, NULL, opt.key.key, opt.key.iv);
    }
}
void openssl_crypto_deinit(crypto_ctx *ctx)
{
    EVP_CIPHER_CTX_cleanup(ctx);
    free(ctx);
}
struct Packet openssl_do_encrypt(crypto_ctx *ctx, unsigned char *intext,
				 int size)
{
    struct Packet crypto;

    crypto.len = 0;
    crypto.data = calloc(1, size * sizeof(unsigned char *));

    if (!EVP_EncryptUpdate(ctx, crypto.data, &crypto.len, intext, size))
    	ctunnel_log(stderr, LOG_CRIT, "EVP_EncryptUpdate: Error");

    return crypto;
}
struct Packet openssl_do_decrypt(crypto_ctx *ctx, unsigned char *intext,
                                 int size)
{
    struct Packet crypto;

    crypto.len = 0;
    crypto.data = calloc(1, size * sizeof(unsigned char *));

    if (!EVP_DecryptUpdate(ctx, crypto.data, &crypto.len, intext, size))
   	ctunnel_log(stderr, LOG_CRIT, "EVP_DecryptUpdate: Error");

    return crypto;
}
#else
crypto_ctx gcrypt_crypto_init(struct options opt, int dir)
{
    int ret = 0;

    crypto_ctx ctx;
    ret = gcry_cipher_open(&ctx, opt.key.cipher, opt.key.mode, 0);
    if (!ctx) {
	ctunnel_log(stderr, LOG_CRIT, "gcry_open_cipher: %s", gpg_strerror(ret));
        exit(ret);
    }
    ret = gcry_cipher_setkey(ctx, opt.key.key, 16);
    if (ret) {
	ctunnel_log(stderr, LOG_CRIT, "gcry_cipher_setkey: %s", gpg_strerror(ret));
        exit(ret);
    }
    ret = gcry_cipher_setiv(ctx, opt.key.iv, 16);
    if (ret) {
	ctunnel_log(stderr, LOG_CRIT, "gcry_cipher_setiv: %s", gpg_strerror(ret));
        exit(ret);
    }
    return ctx;
}
void gcrypt_crypto_deinit(crypto_ctx ctx)
{
    gcry_cipher_close(ctx);
}
void gcrypt_crypto_reset(crypto_ctx ctx, struct options opt, int dir)
{
    gcrypt_crypto_deinit(ctx);
    ctx = gcrypt_crypto_init(opt, dir);
}
struct Packet gcrypt_do_encrypt(crypto_ctx ctx, unsigned char *intext,
	                        int size)
{
    int ret = 0;
    struct Packet crypto;

    crypto.len = size;
    crypto.data = calloc(1, crypto.len * sizeof(char *));
    ret = gcry_cipher_encrypt(ctx, crypto.data, crypto.len,
	                      intext, size);
    if (ret) {
	ctunnel_log(stderr, LOG_CRIT, "Encrypt Error: %s", gpg_strerror(ret));
    }

    return crypto;
}
struct Packet gcrypt_do_decrypt(crypto_ctx ctx, unsigned char *intext,
	                        int size)
{
    struct Packet crypto;

    crypto.len = size;
    crypto.data = calloc(1, crypto.len);
    gcry_cipher_decrypt(ctx, crypto.data,
	                      crypto.len, intext, size);

    return crypto;
}
#endif
