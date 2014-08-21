void xfer_stats_init(struct xfer_stats *x, int t);
void xfer_stats_update(struct xfer_stats *x, int total, int t); 
void xfer_stats_print(FILE *fp, struct xfer_stats *tx, struct xfer_stats *rx);
