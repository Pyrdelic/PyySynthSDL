#pragma once
#include <SDL.h>

#define VISBUFSIZE 1023		// as bitmap, number of samples in buffer, take advantage of integer wrapping
Sint16 visbuf[VISBUFSIZE+1] = { 0 };
Uint16 visbuf_whead = 0;
Uint16 visbuf_rhead = 0;


//void visbuf_init() {
//	visbuf_whead = 0;
//	visbuf_rhead = 0;
//}

void visbuf_write(Sint16 sample) {
	visbuf_whead = (visbuf_whead + 1) & VISBUFSIZE;
	visbuf[visbuf_whead] = sample;
	//printf("vb: %u\n", sample);
}

// sets read head to latest sample (rhead = whead)
void visbuf_set_rhead() {
	visbuf_rhead = visbuf_whead;
}

Sint16 visbuf_read() {
	Sint16 ret = visbuf[visbuf_rhead];
	visbuf_rhead = (visbuf_rhead -1) & VISBUFSIZE;
	return ret;
}

// read, decrease read head, take advantage of underflow