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

#include "rl.h"
#include "console.h"

extern void cons_num(char *msg,unsigned int x);
#define BUF (rlst.last_blk)

volatile RLDSK rlst;


long
twos_comp(int bits, long n)
{  int i;
   long c;
   c=(long)0;
   for(i=0;i<bits;i++) {
      long x;
      x = ((n>>i) & 1);
      if( x == 0 ) {
         x=1;
      } else {
         x=0;
      }
      x = x<<i;
      c = c | x;
   }
   c++;
   return(c);
}


#ifdef NOTUSED
// This subroutine depends on rlst.type which is set by rl_stats()
RLDSK*
logical_sec2shc(unsigned int logical_sec)
{  volatile int s, h, c;
   s=h=c=(unsigned int)0;
   if( rlst.type ) {
      if( logical_sec >= 40960 ) { logical_sec = 40959; } // Max
   } else {
      if( logical_sec >= 20480 ) { logical_sec = 20479; } // Max
   }

   c = 0; s = (int)(logical_sec>>4);
   while( s >= 0x5 ) { s -= 0x5; c++; } // 0x50 = 80 (sec per cyl)
   s = (int)(logical_sec - ((unsigned int)c*(unsigned int)80));
   h=0;
   while( s >= (unsigned int)40 ) { s -= (unsigned int)40; h++; }

   rlst.sector = s;
   rlst.head = h;
   rlst.cylinder = c;
   return(&rlst);
}
#endif // NOTUSED

RLDSK*
rl_seek(unsigned int drv, unsigned int cyl)
{
   volatile unsigned int *ptr = (unsigned int*)RL_DA;
   int target, current, offset;
   unsigned int dir;
   target = (int)cyl;
   rl_read_hdr();
   current = rlst.cylinder;
   offset = target - current; // Signed result for direction +=toward spindle
   if( offset == 0 ) { return(&rlst); }
   if( offset >= 0 ) {
      dir = 1; // toward spindle
   } else {
      dir = 0; // away from spindle
   }
   *ptr = (((unsigned int)offset<<7) & RL_DA_SK_DF) | 
          (((unsigned int)rlst.head<<4) & RL_DA_SK_HS) | 
          (((unsigned int)dir<<2) & RL_DA_SK_DIR) |
          ((unsigned int)0x01);
   rl_wait_dready(drv, 0, 0);
   rl_cmd((unsigned int)drv,(unsigned int)RL_CMD_SEEK);
   rl_read_hdr(); // Update our struct
   return(&rlst);
}

RLDSK*
rl_read(unsigned int drv, unsigned int sec, unsigned int hed, unsigned int cyl, unsigned int words_cnt)
{
   volatile unsigned int *ptr = (unsigned int*)RL_BA;
   unsigned int r, i;
   long x;

//cons_puts("rl_read: start\n");
   if( words_cnt > RL_SECTOR_WSIZE ) {
      cons_puts("ERROR:Larger then one block currently not supported\n\r");
      return(&rlst); // The limit is our buffer size
   }
   rl_status(drv, 1);
   rlst.head = hed;
//cons_puts("rl_read: seek:");cons_hex((char*)&cyl,2,0);
   rl_seek(drv,cyl); // Verify cyl location
   x = twos_comp(13,(long)words_cnt);
   rlst.rw_word_cnt = (unsigned int)words_cnt;
   rlst.rw_word_2s = x;
   //cons_num("MP-X2: ",(unsigned int)x);

   *ptr = (unsigned int)BUF;

//cons_puts("rl_read: cyl: ");cons_hex((char*)&cyl,2,0);
//cons_puts("rl_read: hed: ");cons_hex((char*)&hed,2,0);
//cons_puts("rl_read: sec: ");cons_hex((char*)&sec,2,0);
   rl_wait_cready();
   ptr = (unsigned int*)RL_DA;
   i = ((cyl<<7) & RL_MP_HDR_CYL) | ((hed<<6) & RL_MP_HDR_HEAD) | (sec & RL_MP_HDR_SEC);
   *ptr = (unsigned int)i;  // Where to read!
   //cons_num("Read DA: ",i);
   rlst.da_reg = *ptr;

   ptr = (unsigned int*)RL_MP;
   *ptr = (unsigned int)0160000 | ((unsigned int)(x & 017777));
   rlst.mp_status = *ptr;
   ptr = (unsigned int*)RL_CS;
   rl_wait_dready(drv,0,0); // Wait for it
   rl_cmd((unsigned int)drv,(unsigned int)RL_CMD_RDAT);
   rlst.cs_cmd_rtn = *ptr;
   rlst.last_sector = sec;
   rlst.last_head = hed;
   rlst.last_cylinder = cyl;
   rlst.last_error = rlst.cs_cmd_rtn & RL_CS_ERR;
//cons_puts("rl_read: return\n");
   return(&rlst);
}

#ifndef NEWCODE
// Read next buffer from current disk, and record position
// 2nd version of rl_fread()
volatile int rl_hed=0;
volatile int rl_sec=0;
volatile int rl_cyl=0;
volatile int rl_off=0;
void
rl_sread_init()
{
   rl_status(0, 1); // DBG 2 end tracks, last cyl
   rl_hed=0;
   rl_sec=0;
   rl_cyl=0;
   rl_off=0;
}

// Check for current active sector data in buffer
// Return 0=OK, 1=EOF
int
rl_sread_check()
{  int maxcyl = 512; // Default RL02 with 512 cyl
   int max_retry = 2;
   int i;
   if( rlst.type == 0 ) { // RL01?
      maxcyl = 256;  // RL01 has only 256 cyl
   }
#ifdef DBG1
cons_puts("rl_sread_check(A) rl_cyl\n");cons_hex((char*)&rl_cyl,2,0);
#endif
   if( rl_cyl >= maxcyl ) {
#ifdef DBG1
cons_puts("rl_sread_check(A) END return(1)\n");
#endif
      return(1);
   }
#ifdef DBG1
cons_puts("rl_sread_check(A) rl_hed\n");cons_hex((char*)&rl_hed,2,0);
cons_puts("rl_sread_check(A) rl_sec\n");cons_hex((char*)&rl_sec,2,0);
cons_puts("rl_sread_check(A) rl_off\n");cons_hex((char*)&rl_off,2,0);
#endif
      
   if( rl_off >= RL_SECTOR_BSIZE ) { // Have we reached the end of this block?
      rl_sec++; // Next sector
      rl_off=0; // Start of a new block in new sector
#ifdef DBG1
cons_puts("rl_sread_check(inc) rl_sec\n");cons_hex((char*)&rl_sec,2,0);
#endif
      if( rl_sec >= 40 ) { // Sector overflow?
         rl_hed++; // Carry to Next head-track
         rl_sec = 0; // Start of new track
#ifdef DBG1
cons_puts("rl_sread_check(inc) rl_hed\n");cons_hex((char*)&rl_hed,2,0);
#endif
         if( rl_hed > 1 ) { // Head overflow?
            rl_cyl++; // Carry to Next cylinder
            rl_hed = 0; // Start of new cyl on head zero
#ifdef DBG1
cons_puts("rl_sread_check(inc) rl_cyl\n");cons_hex((char*)&rl_cyl,2,0);
#endif
            if( rl_cyl >= maxcyl ) {
#ifdef DBG1
cons_puts("rl_sread_check(Ret EOF)\n");
#endif
               return(1); // Return EOF flag
            }
         }
      }
   }
   rlst.sector = rl_sec; rlst.head = rl_hed; rlst.cylinder = rl_cyl;

   // Now check if we have the correct block loaded
   if( (rlst.last_cylinder != rl_cyl) ||
             (rlst.last_head != rl_hed) ||
           (rlst.last_sector != rl_sec) ) { // Needed sector = current?
      for(i=0;i<RL_SECTOR_BSIZE;i++) { rlst.last_blk[i] = (char)0; }//zero block
      rlst.sector = rl_sec; rlst.head = rl_hed; rlst.cylinder = rl_cyl;
      rl_off=0; // Start of a new block
#ifdef DBG1
cons_puts("rl_sread_check(B) rl_hed\n");cons_hex((char*)&rl_hed,2,0);
cons_puts("rl_sread_check(B) rl_sec\n");cons_hex((char*)&rl_sec,2,0);
cons_puts("rl_sread_check(B) rl_cyl\n");cons_hex((char*)&rl_cyl,2,0);
cons_puts("rl_sread_check(B) rl_off\n");cons_hex((char*)&rl_off,2,0);
cons_puts("rl_sread_check(B) rl_read\n");
#endif
#ifndef DUMMYBLK
      rl_read(rlst.drive_num, rlst.sector, rlst.head, rlst.cylinder, (unsigned int) 128);
      while( rlst.last_error  && max_retry ) { // Any error?
#ifdef DBG1
cons_puts("rl_sread_check()ERROR Retry\n");
#endif
         rl_wait_dready(rlst.drive_num, 1, 1); // Reset & retry
         rl_read(rlst.drive_num, rlst.sector, rlst.head, rlst.cylinder, (unsigned int) 128);
         max_retry--;
      }
      if( rlst.last_error ) {
#ifdef DBG1
cons_puts("rl_sread_check()ERROR FAILURE\n");
#endif
         return(1); // Failure! Return EOF
      }
#else // DUMMYBLK
      for(i=0;i<RL_SECTOR_BSIZE;i++) {
         rlst.last_blk[i] = (char) ((i & 0xF) + 'A');
      }
      rlst.last_sector = rl_sec;
      rlst.last_head = rl_hed;
      rlst.last_cylinder = rl_cyl;
#endif // DUMMYBLK
   }
#ifdef DBG1
cons_puts("rl_sread_check() normal return\n");
#endif
   return(0); // Needed sector is now current
}

// Sequential read to replace the rl_fread()
// Return the number of char copied to output buffer
int
rl_sread(char* outptr,unsigned int len)
{  int i;
   volatile unsigned int cnt=0;
   volatile char *dst;
   dst = outptr;
   for(i=0;i<len;i++) { outptr[i]=(char)0; } // Zero buffer
   while( cnt < len ) {
      if( rl_sread_check() ) { // Check current, reached EOF?
#ifdef DBG1
cons_puts("rl_sread() Return EOF\n");cons_hex((char*)&cnt,2,0);
#endif
         return(cnt); // We have reached the EOF
      }

      *dst = rlst.last_blk[rl_off++]; // Copy one char, inc src pos
      dst++;
      cnt++;
   }
#ifdef DBG1
cons_puts("rl_sread() cnt\n");cons_hex((char*)&cnt,2,0);
#endif
   return(cnt);
}
#endif // NEWCODE

#ifdef NOTUSED
#ifdef OLDBUG
// Read next buffer from current disk, and record position
// This routine has a bug at the end of the disk/file
int
rl_fread(char* outptr,unsigned int len)
{  int i;
   volatile unsigned long lsec, loff;
   volatile unsigned int log_sec;
   volatile unsigned int cnt=0;
   volatile char *src;
   volatile char *dst;
   dst = outptr;
//cons_puts("rl_fread: start\n\r");
//cons_num("rl_fread: len= ",len);
//cons_num("rl_fread: outptr= ",(unsigned int)outptr);
   while( cnt < len ) {
      if( rlst.chridx >= (unsigned long)10485760 ) { return(cnt); } // Max
      // Right shift x>>8 =  int(x/256);
      lsec = (rlst.chridx>>8) & ((unsigned long)0xFFFF);
//cons_puts("rl_fread: chridx= ");cons_hex((char*)&rlst.chridx,4,0);
      log_sec = (unsigned int)lsec & (unsigned int)0xFFFF; // Find logical sector;
//cons_num("rl_fread: log_sec= ",log_sec);
      loff = (rlst.chridx - (lsec<<8)) & (unsigned long)0xFFFF; // remainder is offset into sector
//cons_num("rl_fread: loff= ",(unsigned int)loff);
      logical_sec2shc(log_sec); // Find sector-head-cyl
      if( (rlst.last_cylinder != rlst.cylinder) ||
                (rlst.last_head != rlst.head) ||
              (rlst.last_sector != rlst.sector) ) {
         for(i=0;i<RL_SECTOR_BSIZE;i++) { rlst.last_blk[i] = (char)0; }//zero
//cons_num("rl_freadA: log_sec= ",log_sec);
            rl_read(rlst.drive_num, rlst.sector, rlst.head, rlst.cylinder, (unsigned int) 128);
//cons_num("rl_freadB: log_sec= ",log_sec);
            if( rlst.last_error ) {
               rl_wait_dready(rlst.drive_num, 1, 1);
               rl_read(rlst.drive_num, rlst.sector, rlst.head, rlst.cylinder, (unsigned int) 128);
               if( rlst.last_error ) {
                  return(cnt); // Failure!
               }
            }
      }
      src = &rlst.last_blk[loff];
      //src += (unsigned int)loff;
//cons_num("rl_fread: loff= ",(unsigned int)loff);
//cons_num("rl_fread: src= ",(unsigned int)src);
//cons_num("rl_fread: dst= ",(unsigned int)dst);
      *dst = *src; // Copy one char
//cons_puts("rl src> "); cons_hex((char*)src,1,0);
//cons_puts("rl dst> "); cons_hex((char*)dst,1,0);
      dst++;
      rlst.chridx++;
      cnt++;
//cons_puts("rl_fread: cnt= ");
//cons_hex((char*)&cnt,2,0);
//cons_puts("rl_fread: len= ");
//cons_hex((char*)&len,2,0);
   }
//cons_puts("rl outptr> ");
//cons_hex((char*)outptr,16,0);
//cons_puts("rl_fread rtn= ");
//cons_hex((char*)&cnt,2,0);
   return(cnt);
}
#endif // OLDBUG
#endif // NOTUSED

RLDSK*
rl_read_hdr()
{
   volatile unsigned int *rp = (unsigned int*)RL_MP;
   unsigned int mpv;

   rl_wait_cready();
   rl_cmd(rlst.drive_num,(unsigned int)RL_CMD_RHDR);
   mpv = *rp;
   rlst.sector = mpv & (unsigned int)RL_MP_STA_DRV;
   rlst.head = ((mpv & (unsigned int)RL_MP_HDR_HEAD)>>6) & 1;
   rlst.cylinder = ((mpv & (unsigned int)RL_MP_HDR_CYL)>>7) & 0777;
   return(&rlst);
}

RLDSK*
rl_cmd(unsigned int drv, unsigned int cmd)
{
   volatile unsigned int *rp = (unsigned int*)RL_BA;
   *rp = (unsigned int)BUF;
   rp = (unsigned int*)RL_CS;
   rlst.drive_num = drv;
   rlst.cmd = cmd;
   rl_wait_cready();
   *rp = ((rlst.drive_num<<8) & (unsigned int)RL_CS_DSEL) | ( rlst.cmd & (unsigned int)RL_CS_CMD );
   rl_wait_cready();
   rlst.err_msg = rl_decode_err(rlst.cs_cmd_rtn);
   return(&rlst);
}

RLDSK*
rl_wait_cready()
{
   volatile unsigned int *rp=(volatile unsigned int *)RL_CS;
   while( ! (*rp & RL_CS_CRDY)  );
   rlst.cs_cmd_rtn = *rp;
   return(&rlst);
}


RLDSK*
rl_status(unsigned int drv, int reset_fg)
{
   volatile unsigned int *rp=(unsigned int *)RL_DA;
   if( reset_fg ) {
      *rp = 013; // Marker=1, GetStatus=1, 0, Reset=1
   } else {
      *rp = 003; // Marker=1, GetStatus=1, 0, Reset=0
   }
   rp = (unsigned int*)RL_CS;
   rl_cmd(drv,RL_CMD_STAT); // Select drive to wait for
   rp = (unsigned int*)RL_MP;
   rlst.mp_status = *rp;
   if( rlst.mp_status & RL_MP_STA_DT ) {
      rlst.type = 1;
   } else {
      rlst.type = 0;
   }
   rlst.drv_state = rl_decode_state(rlst.mp_status);
   return(&rlst);
}

RLDSK*
rl_wait_dready(unsigned int drv, int status_fg, int reset_fg)
{
   volatile unsigned int *rp=(unsigned int *)RL_CS;
   if( status_fg ) {
      rl_status(drv,reset_fg);
   }
   while( ! ( *rp & RL_CS_DRDY)  ) {
      if( (*rp & RL_CS_CERR) || (*rp & RL_CS_DERR) ) {
         rl_status(drv,1);
      }
   }
   rlst.mp_status = *rp;
   return(&rlst);
}

char *
rl_decode_err(unsigned int cs_val)
{
   char *msg;
   unsigned int er = (cs_val & (unsigned int)RL_CS_ERR);
   switch( er ) {
      case 0:
         msg = "No errors";
         break;
      case RL_ERR_OPI: //OPI Operation Incomplete
         msg = "OPI Operation Incomplete";
         break;
      case RL_ERR_CRC: //Read CRC or Write Check
         msg = "Read CRC or Write Check";
         break;
      case RL_ERR_HCRC: //Header CRC
         msg = "Header CRC";
         break;
      case RL_ERR_LATE: //Data Late
         msg = "Data Late";
         break;
      case RL_ERR_HNF: //Header Not Found
         msg = "Header Not Found";
         break;
      case RL_ERR_MEM: //Non-Existant Memory (NXM)
         msg = "Non-Existant Memory";
         break;
      case RL_ERR_PARI: //Mem Parity Error
         msg = "Mem Parity Error";
         break;
      default:
         msg = "Unknown Error";
         break;
   }
   return(msg);
}

char *
rl_decode_state(unsigned int mp_val)
{
   char *msg;
   unsigned int stat = (mp_val & (unsigned int)RL_MP_STA_DRV);
   switch( stat ) {
      case 0:
         msg = "Load cartridge";
         break;
      case 1:
         msg = "Spin-up";
         break;
      case 2:
         msg = "Brush cycle";
         break;
      case 3:
         msg = "Load heads";
         break;
      case 4:
         msg = "Seek";
         break;
      case 5:
         msg = "Lock on";
         break;
      case 6:
         msg = "Unload heads";
         break;
      case 7:
         msg = "Spin-down";
         break;
      default:
         msg = "Should-Not-Happen Unknown";
         break;
   }
   return(msg);
}
