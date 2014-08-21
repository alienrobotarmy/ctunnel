/* crypto.c */
crypto_ctx gcrypt_crypto_init(struct options opt, int dir);
void gcrypt_crypto_reset(crypto_ctx ctx, struct options opt, int dir);
void gcrypt_crypto_deinit(crypto_ctx ctx);
struct Packet gcrypt_do_encrypt(crypto_ctx ctx, unsigned char *intext, int size);
struct Packet gcrypt_do_decrypt(crypto_ctx ctx, unsigned char *intext, int size);
/* crypto.c */
crypto_ctx *openssl_crypto_init(struct options opt, int dir);
void openssl_crypto_reset(crypto_ctx *ctx, struct options opt, int dir);
void openssl_crypto_deinit(crypto_ctx *ctx);
struct Packet openssl_do_encrypt(crypto_ctx *ctx, unsigned char *intext, int size);
struct Packet openssl_do_decrypt(crypto_ctx *ctx, unsigned char *intext, int size);
