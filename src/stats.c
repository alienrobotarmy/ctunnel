#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "ctunnel.h"

#define KILA 1024
#define MEGA 1048576
#define GIGA 1073741824 
#define TERA 1099511627776LL

void xfer_stats_init(struct xfer_stats *x, int t) {
    strcpy(x->ticker, ".oOo");
    strcpy(x->units, " B/s");
    x->time_l = t;
    x->bps = 0;
    x->ticker_i = 0;
}
void xfer_stats_update(struct xfer_stats *x, int total, int t) {
    x->ticker_i++;
    if (x->ticker_i > 3) { x->ticker_i = 0; }

    if (t > x->time_l+1) {
	x->bps = (total - x->total_l) / (t - x->time_l);
	if (x->bps > KILA) {
	    if (x->bps > MEGA) { 
		if (x->bps > GIGA) {
		    if (x->bps > TERA) {
			x->units[0] = 'T'; x->bps /= TERA;
		    }
		    x->units[0] = 'G'; x->bps /= GIGA;
		}
		x->units[0] = 'M'; x->bps /= MEGA;
	    }
	    x->units[0] = 'K'; x->bps /= KILA;
	} else {
	    x->units[0] = ' ';
	}
	x->total_l = total;
	x->time_l = t;
    }
}
void xfer_stats_print(FILE *fp, struct xfer_stats *tx, struct xfer_stats *rx)
{
    fprintf(fp, "[ctunnel] tx[%c] rx[%c] %.2f%s %.2f%s       \r",
	    tx->ticker[tx->ticker_i], rx->ticker[rx->ticker_i],
	    tx->bps, tx->units, rx->bps, rx->units);
    fflush(fp);
}
