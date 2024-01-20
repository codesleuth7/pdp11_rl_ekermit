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
#undef DBG1

/*
  PDP-11 i/o routines.

  Device i/o:

    int devopen()    Communications device - open
    int pktmode()    Communications device - enter/exit packet mode
    int readpkt()    Communications device - read a packet
    int tx_data()    Communications device - send data
    int devclose()   Communications device - close
    int inchk()      Communications device - check if bytes are ready to read

  File i/o:

    int openfile()   File - open for input or output
    ULONG fileinfo() Get input file modtime and size
    int readfile()   Input file - read data
    int writefile()  Output file - write data
    int closefile()  Input or output file - close

  Full definitions below, prototypes in kermit.h.

  These routines must handle speed setting, parity, flow control, file i/o,
  and similar items without the kermit() routine knowing anything about it.
  If parity is in effect, these routines must add it to outbound characters
  and strip it from inbound characters.
*/

#include "cdefs.h"
#include "debug.h"
#include "platform.h"
#include "kermit.h"
#include "console.h"
#include "rl.h"

#define DL11_RCSR       0177560 //Receiver Status Register
#define DL11_RCSR_DONE  0x80
#define DL11_RCSR_INTR  0x40
#define DL11_RBUF       0177562 //Receiver Buffer Register
#define DL11_XCSR       0177564 //Transmitter Status Register
#define DL11_XCSR_READY 0x80
#define DL11_XCSR_INTR  0x40
#define DL11_XBUF       0177566 //Transmitter Buffer Register

extern volatile unsigned long cksec_cnt;
static volatile unsigned int rl_drive_selected = 0;
static volatile RLDSK * rlst_ptr;

extern void cons_num(char* s,unsigned int x);
extern void cons_hex(char* s,unsigned int x, int ascfg);


UCHAR o_buf[OBUFLEN+8];			/* File output buffer */
UCHAR i_buf[IBUFLEN+8];			/* File output buffer */


#ifndef NODLINTR
#define RCVBSZ 64
#define XMTBSZ 132
volatile char rcvbuf[RCVBSZ+1];			/* DL11 receiver interrupt buff */
volatile int rcv_in = 0;
volatile int rcv_out = 0;
volatile int rcv_err = 0;

volatile char xmtbuf[XMTBSZ+1];			/* DL11 xmit interrupt buff */
volatile int xmt_in = 0;
volatile int xmt_out = 0;

void
rcvintr()
{  char c;
   volatile unsigned int *rcsr = (unsigned int *)DL11_RCSR;
   volatile unsigned char *rb = (unsigned char *)DL11_RBUF;
   c = *rb;
   rcvbuf[rcv_in++] = c;
   if( rcv_in >= RCVBSZ ) { rcv_in = 0; } // Wrap around ring buffer
   if( rcv_in == rcv_out ) { // Overflow?
      rcv_out++; // Skip the oldest char, better to loose one then all
      rcv_err++;
      if( rcv_out >= RCVBSZ ) { rcv_out = 0; } // Wrap around ring buffer
   }
}

void
xmtintr()
{  char c;
   volatile unsigned int *xcsr = (unsigned int *)DL11_XCSR;
   volatile unsigned char *xb = (unsigned char *)DL11_XBUF;
   if( xmt_in != xmt_out ) { // Any char in buffer?
      if( *xcsr & DL11_RCSR_DONE ) { // This should already be true
         c = xmtbuf[xmt_out++];
         *xb = c;
         if( xmt_out >= XMTBSZ ) { xmt_out = 0; } // Wrap around ring buffer
      }
   }
   if( xmt_in != xmt_out ) { // Any char in buffer?
      *xcsr = DL11_XCSR_INTR; // Enable intr
   } else {
      *xcsr = 0x0; // Disable intr
   }
}

void
xmtchr(char c)
{  int nxt;
   volatile unsigned int *xcsr = (unsigned int *)DL11_XCSR;
   nxt = xmt_in; nxt++;
   if( nxt >= XMTBSZ ) { nxt = 0; }
   while( nxt == xmt_out ) { // Is the buffer full?
      *xcsr = DL11_XCSR_INTR; // Enable intr, which should send one out
   }
   xmtbuf[xmt_in] = c;
   xmt_in = nxt;
   if( xmt_in != xmt_out ) { // Any char in buffer?
      *xcsr = DL11_XCSR_INTR; // Enable intr
   } else {
      *xcsr = 0x0; // Disable intr
   }
}
#else // NODLINTR
void
rcvintr()
{
}
void
xmtintr()
{
}
void
xmtchr(char c)
{
cons_putc(c);
}
#endif // NODLINTR

/* The DL11 will not pass a DEL (0x7F) which is part of the 7bit char set.
   The base64 solution adds ~33% overhead, but it works over the DL11.
   The copied file can be decoded using the "base64" utility.
*/
static char base64_tbl[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                              'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                              'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                              'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                              'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                              'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                              'w', 'x', 'y', 'z', '0', '1', '2', '3',
                              '4', '5', '6', '7', '8', '9', '+', '/'};

static char base64_ibuf[4];
static char base64_obuf[5];
static int base64_idx=(-1);
int
base64_enc(char *buf, int len)
{  int ocnt=0;
   int j, rcnt;
   unsigned long x;
   while( ocnt < len ) {
      if( base64_idx < 0 ) {
         for(j=0;j<4;j++)base64_ibuf[j]=0;
         rcnt = rl_sread(base64_ibuf,3);
         if( rcnt < 1 ) { return(ocnt); }
         x = ((((unsigned long)base64_ibuf[0])<<16) & 0xFF0000) |
             ((((unsigned long)base64_ibuf[1])<<8) & 0xFF00) |
             (((unsigned long)base64_ibuf[2]) & 0xFF);
         base64_obuf[3] = base64_tbl[((x>>18) & 0x3F)];
         base64_obuf[2] = base64_tbl[((x>>12) & 0x3F)];
         base64_obuf[1] = base64_tbl[((x>>6) & 0x3F)];
         base64_obuf[0] = base64_tbl[(x & 0x3F)];
         base64_idx=3;
      }
      buf[ocnt++] = base64_obuf[base64_idx];
      base64_idx--;
   }
   return(ocnt);
}

/*
  In this example, the output file is unbuffered to ensure that every
  output byte is commited.  The input file, however, is buffered for speed.
  This is just one of many possible implmentation choices, invisible to the
  Kermit protocol module.
*/


/*  D E V O P E N  --  Open communications device  */
/*  

  Call with: string pointer to device name.  This routine should get the
  current device settings and save them so devclose() can restore them.
  It should open the device.  If the device is a serial port, devopen()
  set the speed, stop bits, flow control, etc.
  Returns: 0 on failure, 1 on success.
*/
int
devopen(char *device) {
#ifdef DBG1
cons_puts("devopen()\n");
#endif // DBG1
    return(1); // Not needed
}

/*  P K T M O D E  --  Put communications device into or out of packet mode  */
/*  
  Call with: 0 to put in normal (cooked) mode, 1 to put in packet (raw) mode.
  For a "dumb i/o device" like an i/o port that does not have a login attached
  to it, this routine can usually be a no-op.
  Returns: 0 on failure, 1 on success.
*/
int
pktmode(short on) {
#ifdef DBG1
cons_puts("pktmode()\n");
#endif // DBG1
    return(1); // Not needed
}


/*  D E V S E T T I N G S  */

int
devsettings(char * s) {
    return(1); // Not needed
}

/*  D E V R E S T O R E  */

int
devrestore(void) {
    return(1); // Not needed
}


/*  D E V C L O S E  --  Closes the current open communications device  */
/*  
  Call with: nothing
  Closes the device and puts it back the way it was found by devopen().
  Returns: 0 on failure, 1 on success.
*/
int
devclose(void) {
    return(1); // Not needed
}

/* I N C H K  --  Check if input waiting */

/*
  Check if input is waiting to be read, needed for sliding windows.  This
  sample version simply looks in the stdin buffer (which is not portable
  even among different Unixes).  If your platform does not provide a way to
  look at the device input buffer without blocking and without actually
  reading from it, make this routine return -1.  On success, returns the
  numbers of characters waiting to be read, i.e. that can be safely read
  without blocking.
*/
int
inchk(struct k_data * k) {
#ifndef NOTNOW
#ifdef NODLINTR
   volatile unsigned int *rcsr = (unsigned int *)DL11_RCSR;
   if( (*rcsr & DL11_RCSR_DONE) ) {
      return(1);
   }
   return(0);
#else // NODLINTR
   int dif=0;
   if( rcv_out == rcv_in ) { return(0); }
   if( rcv_in > rcv_out ) { dif = rcv_in - rcv_out; }
   if( rcv_in < rcv_out ) { dif = rcv_out - rcv_in; }
   return(dif);
#endif // NODLINTR
#else // NOTNOW
return(-1);
#endif // NOTNOW
}

/*  R E A D P K T  --  Read a Kermit packet from the communications device  */
/*
  Call with:
    k   - Kermit struct pointer
    p   - pointer to read buffer
    len - length of read buffer

  When reading a packet, this function looks for start of Kermit packet
  (k->r_soh), then reads everything between it and the end of the packet
  (k->r_eom) into the indicated buffer.  Returns the number of bytes read, or:
     0   - timeout or other possibly correctable error;
    -1   - fatal error, such as loss of connection, or no buffer to read into.
*/

int
readpkt(struct k_data * k, UCHAR *p, int len, int fc) {
    volatile unsigned int *rcsr = (unsigned int *)DL11_RCSR;
    unsigned char *rbuf = (unsigned char *)DL11_RBUF;

    unsigned long start_sec;
    int x, n, max;
    short flag;
    UCHAR c;
    char *outbuf = (char*)p;
/*
  Timeout not implemented in this sample.
  It should not be needed.  All non-embedded Kermits that are capable of
  making connections are also capable of timing out, and only one Kermit
  needs to time out.  NOTE: This simple example waits for SOH and then
  reads everything up to the negotiated packet terminator.  A more robust
  version might be driven by the value of the packet-length field.
*/

    flag = n = 0;                       /* Init local variables */

#ifdef DBG1
cons_puts("readpkt()\r\n");
//cons_puts("readpkt: soh= ");
//cons_hex((char*)&(k->r_soh),(unsigned int)1,0);
//cons_puts("readpkt: eom= ");
//cons_hex((char*)&(k->r_eom),(unsigned int)1,0);
cons_num("readpkt: maxlen= ",(unsigned int)k->r_maxlen);
#endif // DBG1
    while (1) {
        start_sec = cksec_cnt;

#ifdef NODLINTR
        while (! (*rcsr & DL11_RCSR_DONE)) {
           if( (cksec_cnt - start_sec) > (unsigned long)10L ) {
              return(0);
           }
        }
        x = (int)( *rbuf & 0xFF ); // Read the char
#else // NODLINTR
        while( rcv_out == rcv_in ) {
#ifdef DBG1
cons_puts("R> sec: ");cons_hex((char*)&cksec_cnt,4,0);
cons_puts("R> start: ");cons_hex((char*)&start_sec,4,0);
cons_puts("R> rcv_i: ");cons_hex((char*)&rcv_in,2,0);
cons_puts("R> rcv_o: ");cons_hex((char*)&rcv_out,2,0);
#endif
           if( (cksec_cnt - start_sec) > (unsigned long)10L ) {
//cons_puts("Rcv timout\r\n");
//cons_puts("rcv_i: ");cons_hex((char*)&rcv_in,2,0);
//cons_puts("rcv_o: ");cons_hex((char*)&rcv_out,2,0);
              return(0);
           }
        }
        x = (int)rcvbuf[rcv_out++]; // Read next char
        if( rcv_out >= RCVBSZ ) { rcv_out = 0; }
//cons_puts("rchr: ");cons_hex((char*)&x,2,1);
        c = (k->parity) ? x & 0x7f : x & 0xff; /* Strip parity */
#endif // NODLINTR

#ifdef DBG1
cons_puts("rchr: ");cons_hex((char*)&x,2,1);
#endif // DBG1
        if (!flag && c != k->r_soh)	/* No start of packet yet */
          continue;                     /* so discard these bytes. */
        if (c == k->r_soh) {		/* Start of packet */
            flag = 1;                   /* Remember */
            continue;                   /* But discard. */
        } else if (c == k->r_eom	/* Packet terminator */
		   || c == '\012'	/* 1.3: For HyperTerminal */
		   ) {
#ifdef DBG1
cons_num("readpkt: return= ",(unsigned int)n);
cons_hex((char*)outbuf,(unsigned int)n,1);
#endif // DBG1
            return(n);
        } else {                        /* Contents of packet */
            // org looks wrong: if (n++ > k->r_maxlen)	/* Check length */
            if (n++ > len)	/* Check length */
              return(0);
            else
              *p++ = x & 0xff;
        }
    }
    return(-1);
}

/*  T X _ D A T A  --  Writes n bytes of data to communication device.  */
/*
  Call with:
    k = pointer to Kermit struct.
    p = pointer to data to transmit.
    n = length.
  Returns:
    X_OK on success.
    X_ERROR on failure to write - i/o error.
*/
int
tx_data(struct k_data * k, UCHAR *p, int n) {
    volatile unsigned int *xcsr = (unsigned int *)DL11_XCSR;
    unsigned char *xbuf = (unsigned char *)DL11_XBUF;
    int x;

#ifdef DBG1
cons_puts("tx_data() start\n");
cons_num("Tx_Len: ",(unsigned int)n);
cons_hex((char*)&n,(unsigned int)2,0);
cons_hex((char*)p,(unsigned int)n,1);
#endif // DBG1
    while (n > 0) {                     /* Keep trying till done */
#ifndef NODLINTR
        while (!(*xcsr & DL11_XCSR_READY)); // Are we ready?
        *xbuf = *p; // Send 1 char
#else // NODLINTR
        xmtchr((char)*p); // Use xmtbuf & DL intr
#endif // NODLINTR
        x = 1; // Yes only one
        n -= x;
	p += x;
    }
    return(X_OK);                       /* Success */
}

/*  O P E N F I L E  --  Open output file  */
/*
  Call with:
    Pointer to filename.
    Size in bytes.
    Creation date in format yyyymmdd hh:mm:ss, e.g. 19950208 14:00:00
    Mode: 1 = read, 2 = create, 3 = append.
  Returns:
    X_OK on success.
    X_ERROR on failure, including rejection based on name, size, or date.    
*/
int
openfile(struct k_data * k, UCHAR * s, int mode) {
    UCHAR *cp = s;
    int i;
    unsigned int drv = 0;
    while( *cp ) { // Scan filename for drive number
       if( (cp[0]=='r' || cp[0]=='R') && (cp[1]=='l' || cp[1]=='L') &&
           cp[2]>='0' && cp[2]<='3' ) {
          drv = (unsigned int) (cp[2] - '0') & 0x3;
       }
       cp++;
    }
    rl_drive_selected = drv;
    rl_sread_init();
#ifdef DBG1
cons_puts("openfile(");
cons_puts((char*)s);
cons_puts(")");
cons_num("Drive_Select: ",(unsigned int)rl_drive_selected);
cons_num("Mode: ",(unsigned int)mode);
#endif // DBG1

    switch (mode) {
      case 1:				/* Read */
        rl_wait_dready(drv, 1, 1);
        rlst_ptr = rl_seek(drv, (unsigned int)0);
        rlst_ptr->chridx = (unsigned long)0;
        rlst_ptr->last_sector=(unsigned int)0xFFFF;
        rlst_ptr->last_head=2;
        rlst_ptr->last_cylinder=(unsigned int)0xFFFF;
        for(i=0;i<RL_SECTOR_BSIZE;i++) { rlst_ptr->last_blk[i] = (char)0; }//zero
	if( (rlst_ptr->cs_cmd_rtn & (unsigned int)RL_CS_ERR) != (unsigned int)0 ) {
	    return(X_ERROR);
	}
	k->s_first   = 1;		/* Set up for getkpt */
	k->zinbuf[0] = '\0';		/* Initialize buffer */
	k->zinptr    = k->zinbuf;	/* Set up buffer pointer */
	k->zincnt    = 0;		/* and count */
#ifdef DBG1
cons_puts("openfile() rtn=OK\n\r");
#endif
	return(X_OK);

      case 2:				/* Write (create) */
        rl_wait_dready(drv, 1, 1);
        rlst_ptr = rl_seek(drv, (unsigned int)0);
        rlst_ptr->chridx = (unsigned long)0;
        rlst_ptr->last_sector=(unsigned int)0xFFFF;
        rlst_ptr->last_head=2;
        rlst_ptr->last_cylinder=(unsigned int)0xFFFF;
        for(i=0;i<RL_SECTOR_BSIZE;i++) { rlst_ptr->last_blk[i] = (char)0; }//zero
	if( (rlst_ptr->cs_cmd_rtn & (unsigned int)RL_CS_ERR) != (unsigned int)0 ) {
	    return(X_ERROR);
	}
	return(X_OK);

      default:
        return(X_ERROR);
    }
}

/*  F I L E I N F O  --  Get info about existing file  */
/*
  Call with:
    Pointer to filename
    Pointer to buffer for date-time string
    Length of date-time string buffer (must be at least 18 bytes)
    Pointer to int file type:
       0: Prevailing type is text.
       1: Prevailing type is binary.
    Transfer mode (0 = auto, 1 = manual):
       0: Figure out whether file is text or binary and return type.
       1: (nonzero) Don't try to figure out file type. 
  Returns:
    X_ERROR on failure.
    0L or greater on success == file length.
    Date-time string set to yyyymmdd hh:mm:ss modtime of file.
    If date can't be determined, first byte of buffer is set to NUL.
    Type set to 0 (text) or 1 (binary) if mode == 0.
*/

ULONG
fileinfo(struct k_data * k,
	 UCHAR * filename, UCHAR * buf, int buflen, short * type, short mode) {
    ULONG sz;
    UCHAR *ocp = buf;
    UCHAR *icp = "19850101 00:00:00";

    if (!buf)
      return(X_ERROR);
    buf[0] = '\0';
    if (buflen < 18)
      return(X_ERROR);
    rlst_ptr = rl_status(rl_drive_selected, 0 );
    if( (rlst_ptr->cs_cmd_rtn & (unsigned int)RL_CS_ERR) != (unsigned int)0 ) {
      return(X_ERROR);
    }
    while( *icp && buflen>0 ) { *ocp = *icp; ocp++; icp++; buflen--; }
    *ocp++ = 0; // NULL at end!
    *type = 1; // Always binary
    if( rlst_ptr->mp_status & RL_MP_STA_DT ) { // Drive Type 0=RL01 1=RL02
#ifdef BINARYSAFE
       sz = (ULONG) 40*256*2*512; // RL02 40=sec, 256bytes/sec, 2=heads, 512=cyl
#else /* BINARYSAFE */
       sz = (ULONG) 13981016L; // Base64 size
#endif /* BINARYSAFE */
    } else {
#ifdef BINARYSAFE
       sz = (ULONG) 40*256*2*256; // RL01
#else /* BINARYSAFE */
       sz = (ULONG) 6990508L; // Base64 size
#endif /* BINARYSAFE */

    }

#ifdef DBG1
cons_puts("fileinfo: ");
cons_puts(buf);
cons_puts("fileinfo(): Return");
cons_hex((char*)&sz,4,0);
#endif

    return((ULONG)sz);
}

// This numstring() routine is defined here because of the
// pdp11 division with long problem. We avoid this issue by knowing
// the string values of the RL01 & RL02 but require access
// to the type information
#ifdef BINARYSAFE
static char rl01len[] = "5242880"; // Normal binary RL01 size
static char rl02len[] = "10485760"; // Normal binary RL02 size
#else /* BINARYSAFE */
static char rl01len[] = "6990508";
static char rl02len[] = "13981016";
#endif /* BINARYSAFE */

STATIC UCHAR *                          /* Convert number to string */
numstring(ULONG n, UCHAR * buf, int buflen, struct k_data * k) {
    int i, x;
    char *sp;
    buf[buflen - 1] = '\0';
    if( rlst_ptr->mp_status & RL_MP_STA_DT ) { // Drive Type 0=RL01 1=RL02
       //sp = rl02len;
       sp = rl02len; // DBG1 test last two tracks
    } else {
       sp = rl01len;
    }
    i=0;
    while( sp[i] && i<(buflen-1) ) { // Copy number string
       buf[i] = sp[i];
       i++;
    }
    buf[i]=0;

//cons_puts("\n\rNumstr: ");cons_hex((char*)buf,(i+1),1);
    return((UCHAR *)buf);
}



#ifndef DBG1
unsigned long tot_char_cnt=0L;
#endif


/*  R E A D F I L E  --  Read data from a file  */

int
readfile(struct k_data * k) {
    if (!k->zinptr) {
	return(X_ERROR);
    }
#ifdef DBG1
cons_puts("\r\nReadFile start\n");
#endif
    if (k->zincnt < 1) {		/* Nothing in buffer - must refill */
	k->dummy = 0;
#ifdef DBG1
cons_puts("ReadFile rl_fread()\n");
#endif
#ifdef DBG1
      /* This creates dummy blocks, instead of reading RL */
      /* This will remove the rl.c to isolate any bug */
      if( tot_char_cnt >= 10485760 ) {
	k->zincnt = 0;
      } else {
        int i;
        char *p;
        p = k->zinbuf;
        for(i=0;i<k->zinlen;i++) {
           *p = (char) ((i & 0xF) + 'A');
            p++;
        }
	k->zincnt = k->zinlen;
        tot_char_cnt += (unsigned long)k->zincnt;
      }
#else // DBG1
#ifdef OLDFREAD
	k->zincnt = rl_fread(k->zinbuf, k->zinlen);
#else // OLDFREAD
	    
#ifdef BINARYSAFE
        k->zincnt = rl_sread(k->zinbuf, k->zinlen);
#else /* BINARYSAFE */
        k->zincnt = base64_enc(k->zinbuf, k->zinlen);
#endif /* BINARYSAFE */

#endif // OLDFREAD
#endif // DBG1

	k->zinbuf[k->zincnt] = '\0';	/* Terminate. (Is this safe in binary?)*/
	if (k->zincnt == 0) {		/* Check for EOF */
#ifdef DBG1
cons_puts("ReadFile rl_fread(EOF)\n");
cons_puts("head: ");
cons_hex((char*)rlst_ptr->head,2,0);
cons_puts("sector: ");
cons_hex((char*)rlst_ptr->sector,2,0);
cons_puts("cylinder: ");
cons_hex((char*)rlst_ptr->cylinder,2,0);
cons_puts("chridx: ");
cons_hex((char*)rlst_ptr->chridx,4,0);
cons_puts(rlst_ptr->err_msg);
#endif
	  return(-1);
	}
	k->zinptr = k->zinbuf;		/* Not EOF - reset pointer */
    }

#ifdef DBG1
cons_puts("ReadFile rl_fread()\n");
cons_num("zincnt = \n",(unsigned int)k->zincnt);
cons_hex((char*)k->zinptr,(unsigned int)k->zincnt,1);
#endif
    (k->zincnt)--;			/* Return first byte. */
    return(*(k->zinptr)++ & 0xff);
}


/*  W R I T E F I L E  --  Write data to file  */
/*
  Call with:
    Kermit struct
    String pointer
    Length
  Returns:
    X_OK on success
    X_ERROR on failure, such as i/o error, space used up, etc
*/
int
writefile(struct k_data * k, UCHAR * s, int n) {
    int rc;
    rc = X_OK;

#ifdef MORE_TODO
    debug(DB_LOG,"writefile binary",0,k->binary);

    if (k->binary) {			/* Binary mode, just write it */
	if (write(ofile,s,n) != n)
	  rc = X_ERROR;
    } else {				/* Text mode, skip CRs */
	UCHAR * p, * q;
	int i;
	q = s;

	while (1) {
	    for (p = q, i = 0; ((*p) && (*p != (UCHAR)13)); p++, i++) ;
	    if (i > 0)
	      if (write(ofile,q,i) != i)
		rc = X_ERROR;
	    if (!*p) break;
	    q = p+1;
	}
    }
#endif // MORE_TODO
    return(rc);
}

/*  C L O S E F I L E  --  Close output file  */
/*
  Mode = 1 for input file, mode = 2 or 3 for output file.

  For output files, the character c is the character (if any) from the Z
  packet data field.  If it is D, it means the file transfer was canceled
  in midstream by the sender, and the file is therefore incomplete.  This
  routine should check for that and decide what to do.  It should be
  harmless to call this routine for a file that that is not open.
*/
int
closefile(struct k_data * k, UCHAR c, int mode) {
    int rc = X_OK;			/* Return code */

    return(rc);
#ifdef MORE_TODO
    switch (mode) {
      case 1:				/* Closing input file */
	if (fclose(ifile) < 0)
	  rc = X_ERROR;
	break;
      case 2:				/* Closing output file */
      case 3:
	if (ofile < 0)			/* If not open */
	  break;			/* do nothing but succeed */
	debug(DB_LOG,"closefile (output) name",k->filename,0);
	debug(DB_LOG,"closefile (output) keep",0,k->ikeep);
	if (close(ofile) < 0) {		/* Try to close */
	    rc = X_ERROR;
	} else if ((k->ikeep == 0) &&	/* Don't keep incomplete files */
		   (c == 'D')) {	/* This file was incomplete */
	    if (k->filename) {
		debug(DB_LOG,"deleting incomplete",k->filename,0);
		unlink(k->filename);	/* Delete it. */
	    }
	}
	break;
      default:
	rc = X_ERROR;
    }
    return(rc);
#endif // MORE_TODO
}

