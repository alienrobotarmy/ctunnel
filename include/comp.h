/* comp.c */
struct Compress z_compress_init(struct options opt);
struct Packet z_compress(z_stream str, unsigned char *data, int size);
struct Packet z_uncompress(z_stream str, unsigned char *data, int size);
void z_compress_reset(struct Compress comp);
void z_compress_end(struct Compress comp);
