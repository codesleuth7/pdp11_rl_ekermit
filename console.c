/*

  E-Kermit 1.7 -- Embedded Kermit (PDP-11 RL Bare Metal version)

  Kermit Author:  Frank da Cruz
  PDP-11 RL Bare Metal port: Todd Markley
  License: Revised 3-Clause BSD License

  Copyright (C) 1995, 2011, 2023
  Trustees of Columbia University in the City of New York.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Columbia University nor the names of its contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CONSOLE_H
#include "console.h"
#endif

#define DL11_RCSR	0177560
#define DL11_RCSR_DONE	0x80
#define DL11_RCSR_INTR	0x40
#define DL11_RBUF	0177562
#define DL11_XCSR	0177564
#define DL11_XCSR_READY	0x80
#define DL11_XBUF	0177566

#define LP11_CSR       0177514 //Line Printer CSR
#define LP11_RDY       0x0080 // Ready bit in CSR
#define LP11_BUF       0177516 //Line Printer Output Buffer

#ifndef NODLINTR
void xmtchr(char c);
#define RCVBSZ 64
#define XMTBSZ 128
extern volatile char rcvbuf[];
extern volatile int rcv_in;
extern volatile int rcv_out;
extern volatile int rcv_err;
extern volatile char xmtbuf[];
extern volatile int xmt_in;
extern volatile int xmt_out;
#endif

void lp_putc(char c)
{	volatile unsigned int *csr = (unsigned int *)LP11_CSR;
	volatile unsigned char *obuf = (unsigned char *)LP11_BUF;
	while (!(*csr & LP11_RDY)) ; // Wait until ready
	if( c == 13 ) {
	   *obuf = 10;
	} else {
	   *obuf = c;
	}
}

void cons_putc(char c)
{
#ifdef NORMAL
#ifdef NODLINTR
	volatile unsigned int *xcsr = (unsigned int *)DL11_XCSR;
	unsigned char *xbuf = (unsigned char *)DL11_XBUF;
	while (!(*xcsr & DL11_XCSR_READY)) ;
	*xbuf = c;
        if( c == 10 ) {
	   while (!(*xcsr & DL11_XCSR_READY)) ;
	   *xbuf = 13;
        } else if( c == 13 ) {
	   while (!(*xcsr & DL11_XCSR_READY)) ;
	   *xbuf = 10;
        }
#else
   xmtchr(c);
   if( c == 10 ) {
      xmtchr((char)13);
   } else if( c == 13 ) {
      xmtchr((char)10);
   }

#endif
#else // NORMAL
   lp_putc(c);
#endif // NORMAL
}

char cons_getc()
{   char c;
#ifdef NODLINTR
	volatile unsigned int *rcsr = (unsigned int *)DL11_RCSR;
	unsigned char *rbuf = (unsigned char *)DL11_RBUF;
	while (! (*rcsr & DL11_RCSR_DONE)) ;
	return *rbuf & 0x7F;
#else
   while( rcv_out == rcv_in );
   c = rcvbuf[rcv_out++];
   if( rcv_out >= RCVBSZ ) { rcv_out = 0; }
   return(c);
#endif
}

void cons_gets(char *buffer, int size)
{
	char c, *p = buffer;
	while (1) {
		c = cons_getc();
		if ((c == '\b') || (c == 0x7F)) {
			if (p > buffer) {
				cons_putc(8); // ^H
				cons_putc(' ');
				cons_putc(8); // ^H
				p--;
			} else {
				cons_putc(7);	// Ring Bell
			}
		} else if ( c == 0x15 ) { // ^U
			cons_puts("^U\n");
			p = buffer;
		} else if ( c == 0x0c ) { // ^l
			cons_puts("^L\n");
			cons_puts(buffer);
		} else if (c >= ' ') {
			if (p < buffer + size - 2) {
				*(p++) = c;
				cons_putc(c);
			}
		} else if (c == '\r') {
			cons_putc(c);
			cons_putc('\n');
			return;
		}
		*p = 0;
	}
}

void cons_puts(char *s)
{
	for (;*s;s++) cons_putc(*s);
}

