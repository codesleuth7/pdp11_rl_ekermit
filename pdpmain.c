
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

/*

PDP-11 Compiler sizeof results:
sizeof(char)=1
sizeof(char*)=2
sizeof(int)=2
sizeof(unsigned int)=2
sizeof(unsigned int*)=2
sizeof(unsigned short)=2
sizeof(long int)=4
sizeof(long int*)=2
sizeof(long)=4
sizeof(long*)=2

*/
#include "cdefs.h"
#include "platform.h"
#include "kermit.h"
#include "console.h"
#include "rl.h"

#define MBSZ 12
char mbuf[MBSZ+4];
UCHAR *sndfiles[] = {
   "rldisk01.b64"
};

int devopen(char *);                    /* Communications device/path */
int devsettings(char *);
int devrestore(void);
int devclose(void);
int pktmode(short);

int readpkt(struct k_data *, UCHAR *, int); /* Communications i/o functions */
int tx_data(struct k_data *, UCHAR *, int);
int inchk(struct k_data *);
void xmtchr(char c);

int openfile(struct k_data *, UCHAR *, int); /* File i/o functions */
int writefile(struct k_data *, UCHAR *, int);
int readfile(struct k_data *);
int closefile(struct k_data *, UCHAR, int);
ULONG fileinfo(struct k_data *, UCHAR *, UCHAR *, int, short *, short);

/* External data */


/* This is the clock tick counter */
volatile unsigned int cktick;
volatile unsigned long cksec_cnt=0;
volatile unsigned int spnow;
volatile unsigned int spmin=0x7FFF;

#define RCVBSZ 64
#define XMTBSZ 128
extern volatile char rcvbuf[];
extern volatile int rcv_in;
extern volatile int rcv_out;
extern volatile char xmtbuf[];
extern volatile int xmt_in;
extern volatile int xmt_out;

void
ckint()  // Called only from the clock interrupt routine
{
   cktick++;
   if( spnow < spmin ) { spmin = spnow; }
   if( cktick >= 60 ) {
      cktick=0;
      cksec();
      return;
   }
   return;
}

void
cksec()  // Called only from the clock interrupt routine
{
   cksec_cnt++; // Good for 18 days before overflow
#ifdef DBG1
//cons_puts("sec: ");cons_hex((char*)&cksec_cnt,4,0);
cons_puts(".");
if( rcv_in != rcv_out ) {
   cons_puts("rcvbuf: ");cons_hex((char*)rcvbuf,8,1);
   cons_puts("rcv_in: ");cons_hex((char*)&rcv_in,2,0);
   cons_puts("rcv_out: ");cons_hex((char*)&rcv_out,2,0);
}
#endif
   return;
}


extern UCHAR o_buf[];                   /* Must be defined in io.c */
extern UCHAR i_buf[];                   /* Must be defined in io.c */

volatile struct k_data k;                        /* Kermit data structure */
volatile struct k_response r;                    /* Kermit response structure */

int action = 0;                         /* Send or Receive */
int xmode = 0;                          /* File-transfer mode */
int ftype = 1;                          /* Global file type 0=text 1=binary*/
int keep = 0;                           /* Keep incompletely received files */
short fmode = -1;                       /* Transfer mode for this file */
int parity = 1;                         /* Parity */
#ifdef F_CRC
int check = 3;                          /* Block check */
#else
int check = 1;
#endif /* F_CRC */
int remote = 1;                         /* 1 = Remote, 0 = Local */



void
doexit(int status) {
    devrestore();                       /* Restore device */
    devclose();                         /* Close device */
    while( 1 ) ; // Hang in the forever loop
}



// Print a word (16 bits) on the console
void
cons_num(char *msg,unsigned int x)
{  int i,c,n;
   cons_puts("\n");
   cons_puts(msg);
   cons_puts(" ");
#ifdef PNTBIN
   for(i=15,c=0;i>=0;i--) {
      if( ((x>>i) & 1) ) {
         cons_puts("1");
      } else {
         cons_puts("0");
      }
      c++;
      if( c>3 ) {
         c=0;
         cons_puts(" ");
      }
   }
   cons_puts("  ");
   n = (x>>15) & 0x1;
   n += 0x30; // Add "0"
   cons_putc((char)n);
   for(i=12,c=0;i>=0;i-=3) {
      n = (x>>i) & 0x7;
      n += 0x30; // Add "0"
      cons_putc((char)n);
   }
#endif
   cons_puts("  0x");
   for(i=12,c=0;i>=0;i-=4) {
      unsigned int n;
      n = (x>>i) & 0xf;
      if( n <= 9 ) {
         n += 0x30; // Add "0"
      } else {
         n += ( 0x41 - 10); // Offset 10 to "A"
      }
      cons_putc((char)n);
   }
   cons_puts("\n");
   
}

// Print a long (32 bits) on the console
void
cons_lnum(char *msg,unsigned long x)
{  int i,c,n;
   cons_puts("\n");
   cons_puts(msg);
   cons_puts(" ");
#ifdef PNTBIN
   for(i=31,c=0;i>=0;i--) {
      if( ((x>>i) & 1) ) {
         cons_puts("1");
      } else {
         cons_puts("0");
      }
      c++;
      if( c>3 ) {
         c=0;
         cons_puts(" ");
      }
   }
   cons_puts("  ");
   n = (x>>15) & 0x1;
   n += 0x30; // Add "0"
   cons_putc((char)n);
   for(i=28,c=0;i>=0;i-=3) {
      n = (x>>i) & 0x7;
      n += 0x30; // Add "0"
      cons_putc((char)n);
   }
#endif
   cons_puts("  0x");
   for(i=28,c=0;i>=0;i-=4) {
      long int n;
      n = (x>>i) & 0xf;
      if( n <= 9 ) {
         n += 0x30; // Add "0"
      } else {
         n += ( 0x41 - 10); // Offset 10 to "A"
      }
      cons_putc((char)n);
   }
   cons_puts("\n");
   
}

void
cons_hex(char *ptr,unsigned int len,int ascfg)
{  int i, c, ix;
   unsigned int n;
   for(ix=0,c=0;ix<len;ix++) {
      for(i=4;i>=0;i-=4) {
         n = (ptr[ix]>>i) & 0xf;
         if( n <= 9 ) {
            n += 0x30; // Add "0"
         } else {
            n += 55; // Offset 10 to "A"
         }
         cons_putc((char)n);
         c++;
      }
      cons_putc(' '); cons_putc(' ');
      c+=2;
      if( c > 80 ) {
         cons_puts("\n");
         c=0;
      }
   }
   cons_puts("\r\n");
   if( ascfg ) {
      for(ix=0,c=0;ix<len;ix++) {
         n = (unsigned int)ptr[ix];
         if( n >= (unsigned int)0x20 & n <= (unsigned int)0x7E ) {
            if( n == (unsigned int)0x27 ) {
               cons_putc((char)' ');
               cons_putc((char)n);
               cons_putc((char)' ');
               cons_putc((char)' ');
            } else {
               cons_putc((char)0x27);
               cons_putc((char)n);
               cons_putc((char)0x27);
               cons_putc((char)' ');
            }
         } else if( n <= 0x1A && n >= 1 ) {
               cons_putc((char)' ');
               cons_putc((char)'^');
               cons_putc((char)(n+0x40));
               cons_putc((char)' ');
         } else {
            switch(n) {
               case 0x00:
               cons_puts("NUL ");
               break;
               case 0x7F:
               cons_puts("DEL ");
               break;
               case 0x1B:
               cons_puts("ESC ");
               break;
               case 0x1C:
               cons_puts("FS  ");
               break;
               case 0x1D:
               cons_puts("GS  ");
               break;
               case 0x1E:
               cons_puts("RS  ");
               break;
               case 0x1F:
               cons_puts("US  ");
               break;
            }
         }
         c+=4;
         if( c > 80 ) {
            cons_puts("\n");
            c=0;
         }
      }
      cons_puts("\r\n");
   }
}


int main()
{
   int status, rx_len, i, x;
   char c;
   UCHAR *inbuf;
   short r_slot;
   unsigned long start_sec;

   volatile unsigned int *ptr;
   RLDSK *rlp;

   if (!devopen("dummy"))              /* Open the communication device */
      doexit(FAILURE);
    if (!devsettings("dummy"))          /* Perform any needed settings */
      doexit(FAILURE);

    rl_wait_dready(0, 1, 1);
    rlp = rl_seek(0, (unsigned int)0);
    rlp = rl_status((unsigned int)0,0);

#ifdef WAIT
while( mbuf[0] != 1 ) {
#ifndef NOCONSINTR
   long cntr, cnts, cntt;
   unsigned long idx = 10475520L;
   unsigned long lsec, loff;
   unsigned int log_sec;
   int x,y;
   char frbuf[512];

//rlp->chridx = (unsigned long)10484480L;
rlp->chridx = (unsigned long)0L;
   lsec = idx>>8 & (unsigned long)0xFFFF;
   log_sec = (unsigned int)lsec & (unsigned int)0xFFFF;
   loff = (idx - (lsec<<8)) & (unsigned long)0xFFFF;
//cons_puts("idx: "); cons_hex((char*)&idx,4,0);
//cons_puts("log_sec: "); cons_hex((char*)&log_sec,2,0);
//cons_puts("loff: "); cons_hex((char*)&loff,4,0);
   logical_sec2shc(log_sec);
while( (x=rl_fread(frbuf,256)) ) {
//cons_puts("rtn cnt: "); cons_hex((char*)&x,2,0);
//cons_puts("head: "); cons_hex((char*)&rlp->head,2,0);
//cons_puts("sec: "); cons_hex((char*)&rlp->sector,2,0);
//cons_puts("cyl: "); cons_hex((char*)&rlp->cylinder,2,0);
//cons_puts("frbuf: "); cons_hex((char*)frbuf,x,0);
//cons_puts(rlp->err_msg);
}
//cons_hex((char*)frbuf,256,1);



   cons_puts("CMD> ");
   cons_gets(mbuf,MBSZ);
   cons_puts("<\n");
#else
   char c;
   xmtchr('C');
   xmtchr('M');
   xmtchr('D');
   xmtchr('>');
   while( 1 ) {
      while( rcv_in == rcv_out );
      c = rcvbuf[rcv_out++];
      if( rcv_out >= RCVBSZ ) { rcv_out=0; }
      xmtchr(c);
   }
#endif
}
#endif
#ifdef DBG1
cons_puts("Starting kermit loop after delay of 15sec\r");
start_sec = cksec_cnt;
while( cksec_cnt < ( start_sec+(unsigned long)15L ) );
#endif
    action = A_SEND; // This is the default, sending the image


while( 1 ) {
/*  Fill in parameters for this run */

    k.xfermode = xmode;                 /* Text/binary automatic/manual  */
    k.remote = remote;                  /* Remote vs local */
    k.binary = ftype;                   /* 0 = text, 1 = binary */
    k.parity = parity;                  /* Communications parity */
    k.bct = (check == 5) ? 3 : check;   /* Block check type */
    k.ikeep = keep;                     /* Keep incompletely received files */
    k.filelist = sndfiles;                /* List of files to send (if any) */
    k.cancel = 0;                       /* Not canceled yet */

/*  Fill in the i/o pointers  */

    k.zinbuf = i_buf;                   /* File input buffer */
    k.zinlen = IBUFLEN;                 /* File input buffer length */
    k.zincnt = 0;                       /* File input buffer position */
    k.obuf = o_buf;                     /* File output buffer */
    k.obuflen = OBUFLEN;                /* File output buffer length */
    k.obufpos = 0;                      /* File output buffer position */

/* Fill in function pointers */

    k.rxd    = readpkt;                 /* for reading packets */
    k.txd    = tx_data;                 /* for sending packets */
    k.ixd    = inchk;                   /* for checking connection */
    k.openf  = openfile;                /* for opening files */
    k.finfo  = fileinfo;                /* for getting file info */
    k.readf  = readfile;                /* for reading files */
    k.writef = writefile;               /* for writing to output file */
    k.closef = closefile;               /* for closing files */
    k.dbf    = 0;
    /* Force Type 3 Block Check (16-bit CRC) on all packets, or not */
    k.bctf   = (check == 5) ? 1 : 0;

/* Initialize Kermit protocol */

    status = kermit(K_INIT, &k, 0, 0, "", &r);
    if (status == X_ERROR)
      doexit(FAILURE);
    if (action == A_SEND) {
      status = kermit(K_SEND, &k, 0, 0, "", &r);
    }

#ifdef DBG1
cons_puts("kermit(SEND) return\n");
#endif
/*
  Now we read a packet ourselves and call Kermit with it.  Normally, Kermit
  would read its own packets, but in the embedded context, the device must be
  free to do other things while waiting for a packet to arrive.  So the real
  control program might dispatch to other types of tasks, of which Kermit is
  only one.  But in order to read a packet into Kermit's internal buffer, we
  have to ask for a buffer address and slot number.

  To interrupt a transfer in progress, set k.cancel to I_FILE to interrupt
  only the current file, or to I_GROUP to cancel the current file and all
  remaining files.  To cancel the whole operation in such a way that the
  both Kermits return an error status, call Kermit with K_ERROR.
*/
    while (status != X_DONE) {
/*
  Here we block waiting for a packet to come in (unless readpkt times out).
  Another possibility would be to call inchk() to see if any bytes are waiting
  to be read, and if not, go do something else for a while, then come back
  here and check again.
*/
        inbuf = getrslot(&k,&r_slot);	/* Allocate a window slot */
        rx_len = k.rxd(&k,inbuf,P_PKTLEN); /* Try to read a packet */

/*
  For simplicity, kermit() ACKs the packet immediately after verifying it was
  received correctly.  If, afterwards, the control program fails to handle the
  data correctly (e.g. can't open file, can't write data, can't close file),
  then it tells Kermit to send an Error packet next time through the loop.
*/
        if (rx_len < 1) {               /* No data was read */
            freerslot(&k,r_slot);	/* So free the window slot */
            if (rx_len < 0)             /* If there was a fatal error */
              doexit(FAILURE);          /* give up */

	    /* This would be another place to dispatch to another task */
	    /* while waiting for a Kermit packet to show up. */

        }
        /* Handle the input */

#ifdef DBG1
cons_puts("kermit(RUN) Starting\n");
#endif
        switch (status = kermit(K_RUN, &k, r_slot, rx_len, "", &r)) {
	  case X_OK:
	    /* Maybe do other brief tasks here... */
	    continue;			/* Keep looping */
	  case X_DONE:
	    break;			/* Finished */
	  case X_ERROR:
	    doexit(FAILURE);		/* Failed */
	}
    }
    doexit(SUCCESS);
}


#ifdef OLDCODE
 while( 1 ) {
   cons_puts("CMD> ");
   cons_gets(buf, 127);
   cons_puts("\n");

   switch( buf[0] ) {
      case 'S':
      case 's':
         rlp = rl_status((unsigned int)0,0);
         cons_puts("Controller: ");
         cons_puts(rlp->err_msg);
         cons_puts("\nDrive: ");
         cons_puts(rlp->drv_state);
         cons_num("CS: ",rlp->cs_cmd_rtn);
         cons_num("MP: ",rlp->mp_status);
         cons_puts("\n");
/*
         cons_num("char ",sizeof(char));
         cons_num("char* ",sizeof(char*));
         cons_num("int ",sizeof(int));
         cons_num("unsigned int ",sizeof(unsigned int));
         cons_num("unsigned int* ",sizeof(unsigned int*));
         cons_num("long int ",sizeof(long int));
         cons_num("long int* ",sizeof(long int*));
         cons_num("long ",sizeof(long));
         cons_num("long* ",sizeof(long*));
*/
         break;
      case 'R':
      case 'r':
         cons_puts("Controller: ");
         cons_puts(rlp->err_msg);
         cons_puts("\nDrive: ");
         cons_puts(rlp->err_msg);
         cons_num("CS-last: ",rlp->cs_cmd_rtn);
         cons_num("MP-last: ",rlp->mp_status);
         ptr = (unsigned int*)RL_CS;
         r = *ptr;
         cons_num("CS-REG: ",((unsigned int)r));
         ptr = (unsigned int*)RL_DA;
         r = *ptr;
         cons_num("DA-REG: ",((unsigned int)r));
         ptr = (unsigned int*)RL_MP;
         //cons_num("MP-ADR: ",((unsigned int)ptr));
         r = *ptr;
         cons_num("MP-REG: ",((unsigned int)r));
         break;
      case 'C':
      case 'c':
         rlp = rl_wait_cready();
         cons_puts("Controller: ");
         cons_puts(rlp->err_msg);
         cons_num("CS-last: ",rlp->cs_cmd_rtn);
         cons_num("MP-last: ",rlp->mp_status);
         cons_num("CS-REG: ",((unsigned int)r));
         cons_puts("\n");
         break;
      case 'D':
      case 'd':
         rlp = rl_wait_dready((unsigned int)0,1,0);
         cons_puts("Controller: ");
         cons_puts(rlp->err_msg);
         cons_puts("\nDrive: ");
         cons_puts(rlp->drv_state);
         cons_num("CS-last: ",rlp->cs_cmd_rtn);
         cons_num("MP-last: ",rlp->mp_status);
         cons_num("CS-REG: ",((unsigned int)r));
         cons_puts("\n");
         break;
      case 'M':
      case 'm':
         rlp = rl_wait_cready();
         ptr = (unsigned int*)rlp->last_blk;
         cons_puts("MEMORY:\r");
         cons_num("buf: ",(unsigned int)*ptr);
         cons_puts((char *)ptr);
         cons_puts("\r");
         break;
      case 'Z':
      case 'z':
         rlp = rl_wait_cready();
         ptr = (unsigned int*)rlp->last_blk;
         for(i=0;i<256;i++) {
            *ptr = ' ';
            ptr++;
         }
         break;
      case 'G':
      case 'g':
         rlp = rl_read((unsigned int)0,(unsigned int)0,(unsigned int)0,(unsigned int)0,(unsigned int)128);
         cons_puts("Controller: ");
         cons_puts(rlp->err_msg);
         cons_puts("\nDrive: ");
         cons_puts(rlp->drv_state);
         cons_num("CS-last: ",rlp->cs_cmd_rtn);
         cons_num("MP-last: ",rlp->mp_status);
         cons_num("CS-REG: ",((unsigned int)r));
         cons_puts("\n");
         break;
      case 'K':
      case 'k':
         rlp = logical_sec2shc((unsigned int)40959);
         rl_seek((unsigned int)0,rlp->cylinder);
         break;
      case 'A':
      case 'a':
         for(l=(long)38000;l<(long)40960;l++) { // Every logical sector
            rlp = logical_sec2shc((unsigned int)l);

            ptr = (unsigned int*)rlp->last_blk;
            for(j=0;j<256;j++) { // overwrite memory with blanks
               *ptr = ' ';
               ptr++;
            }
            *ptr = 0;

            rlp = rl_read((unsigned int)0,rlp->sector,rlp->head,rlp->cylinder,128);
            if( rlp->cs_cmd_rtn & RL_CS_ERR ) {
               cons_num("CS-Error: ",rlp->cs_cmd_rtn);
               cons_puts(rlp->err_msg);
               cons_puts(rlp->drv_state);
               l = 0x3FFFFF0; // Abort
            } else {
               ptr = (unsigned int*)rlp->last_blk;
               cons_puts((char*)ptr);
            }
         }
         break;
      case 'H':
      case 'h':
         rlp = rl_read_hdr();
         cons_puts("Controller: ");
         cons_puts(rlp->err_msg);
         cons_puts("\nDrive: ");
         cons_puts(rlp->drv_state);
         cons_num("CS-last: ",rlp->cs_cmd_rtn);
         cons_num("MP-last: ",rlp->mp_status);
         cons_num("Sector: ",((unsigned int)rlp->sector));
         cons_num("Head: ",((unsigned int)rlp->head));
         cons_num("Cylinder: ",((unsigned int)rlp->cylinder));
         cons_puts("\n");
         ptr = (unsigned int*)RL_MP;
         break;
      case 'B':
      case 'b':
         rl_wait_dready(0, 1, 1);
         cons_puts("B: wait_dready");
         rlp = rl_seek(0, (unsigned int)0);
         cons_puts("B: seek");
         rlp->chridx = (unsigned long)10485196;
         rlp->last_sector=(unsigned int)0xFFFF;
         rlp->last_head=2;
         rlp->last_cylinder=(unsigned int)0xFFFF;
         for(i=0;i<RL_SECTOR_BSIZE;i++) { rlp->last_blk[i] = (char)0; }
         while( rlp->chridx < (unsigned long)10485760 ) {
            for(i=0;i<MBSZ;i++) { mbuf[i] = (char)0; }
            //cons_num("mbuf-addr: ",(unsigned int)mbuf);
            j=rl_fread(mbuf,MBSZ-1);
            //cons_num("cnt: ",((unsigned int)j));
            //cons_num("ERR: ",rlp->last_error);
            //cons_puts(rlp->err_msg);
            //cons_puts("\nmbuf");
            //cons_hex(mbuf,16,0);
            //cons_puts("\nlblk: ");
            //cons_hex(rlp->last_blk,16,0);
            //cons_puts("\n");
            //cons_puts(rlp->last_blk);
            //cons_puts("\n");
            cons_puts(mbuf);
         }
         cons_puts("\n");
         break;
   }
 }
#endif // OLDCODE
}

