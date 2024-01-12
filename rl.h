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

// RL, address=17774400-17774411, vector=160

#ifndef _RL_H
#define _RL_H 1

// RL Controller Register Addresses
#define RL_CS 0174400 // CS=Control Status
#define RL_BA 0174402 // BA=Bus Address
#define RL_DA 0174404 // DA=Disk Address
#define RL_MP 0174406 // MP=Multipurpose
#define RL_BAE 0174410 // BAE=Bus Addresses Extension (RLV12 only, upper 6 bits of 22)
#define RL_VEC 0160 // Vector

#define RL_CS_DRDY 0x0001 // CS mask for DRDY (Drive Ready)
#define RL_CS_CMD 0x000E // CS mask for Function Command
#define RL_CMD_NOOP 0x0000 // CMD = NoOp or Maint mode
#define RL_CMD_WCHK 0x0002 // CMD = Write Check
#define RL_CMD_STAT 0x0004 // CMD = Get Status
#define RL_CMD_SEEK 0x0006 // CMD = Seek
#define RL_CMD_RHDR 0x0008 // CMD = Read Header
#define RL_CMD_WDAT 0x000A // CMD = Write Data
#define RL_CMD_RDAT 0x000C // CMD = Read Data
#define RL_CMD_RDNC 0x000E // CMD = Read Data No Header Check
#define RL_CS_BA17 0x0030 // CS mask for Bus Address BA17 & BA16
#define RL_CS_INT 0x0040 // CS mask for Interrupt Enable bit
#define RL_CS_CRDY 0x0080 // CS mask for CRDY (Controller Ready)
#define RL_CS_DSEL 0x0300 // CS mask for Disk Select DS1 & DS0
#define RL_DSK_0 0x0000 // Disk 0
#define RL_DSK_1 0x0100 // Disk 1
#define RL_DSK_2 0x0200 // Disk 2
#define RL_DSK_3 0x0300 // Disk 3
#define RL_CS_ERR 0x3C00 // Error Code
#define RL_ERR_OPI 0x0400 // ERR = OPI Operation Incomplete
#define RL_ERR_CRC 0x0800 // ERR = Read CRC or Write Check
#define RL_ERR_HCRC 0x0C00 // ERR = Header CRC
#define RL_ERR_LATE 0x1000 // ERR = Data Late (DLT)
#define RL_ERR_HNF 0x1400 // ERR = Header Not Found
#define RL_ERR_MEM 0x2000 // ERR = Non-Existant Memory (NXM)
#define RL_ERR_PARI 0x2400 // ERR = Mem Parity Error (MPE RLV12 only)
#define RL_CS_DERR 0x4000 // Drive error (Clear using Get Status)
#define RL_CS_CERR 0x8000 // Composite error (Error code exists, can trigger interrupt)
#define RL_MP_HDR_SEC 0x003F // After Read Header, mask for Sector SA0-SA5 (6bits)
#define RL_MP_HDR_HEAD 0x0040 // After Read Header, mask for Head bit
#define RL_MP_HDR_CYL 0xFF80 // After Read Header, mask for Cylinder CA0-CA8 (9bits)
#define RL_MP_STA_DRV 0x0007 // After Status, mask for Drive Major State code (3bits)
#define RL_MP_STA_BH 0x0008 // After Status, mask for Brush Home
#define RL_MP_STA_HO 0x0010 // After Status, mask for Heads Out
#define RL_MP_STA_CO 0x0020 // After Status, mask for Cover Open
#define RL_MP_STA_HS 0x0040 // After Status, mask for Head Select
#define RL_MP_STA_DT 0x0080 // After Status, mask for Drive Type 0=RL01 1=RL02
#define RL_MP_STA_DSE 0x0100 // After Status, mask for Drive Select Error
#define RL_MP_STA_VC 0x0200 // After Status, mask for Volume Check
#define RL_MP_STA_WGE 0x0400 // After Status, mask for Write Gate
#define RL_MP_STA_SPE 0x0800 // After Status, mask for Spin Error
#define RL_MP_STA_SKTO 0x1000 // After Status, mask for Seek Time Out
#define RL_MP_STA_WL 0x2000 // After Status, mask for Write Lock
#define RL_MP_STA_CHE 0x4000 // After Status, mask for Current Head Error
#define RL_MP_STA_WDE 0x8000 // After Status, mask for Write Data Error
#define RL_DA_SK_DIR 0x0004 // Seek Direction bit
#define RL_DA_SK_HS 0x0010 // Seek Head select
#define RL_DA_SK_DF 0xFF80 // Seek Difference

#define RL_SECTORS 40 // Total sectors per track
#define RL_SECTOR_WSIZE 128 // Total 128x16 bit words per sectors, or 256bytes
#define RL_SECTOR_BSIZE (RL_SECTOR_WSIZE*2) // Block size in bytes
#define RL1_CYL 256 // Total cylinders for RL01
#define RL2_CYL 512 // Total cylinders for RL02 cyl=0-40959 (0-0x4FFF)

typedef struct RLdsk {
   unsigned int drive_num;
   unsigned int type;   // 1=RL02  0=RL01
   unsigned int cmd;
   unsigned int sector; // 0-39 (40 sectors) (128 16 bit words per sector)
   unsigned int head; // 0 or 1
   unsigned int cylinder; // RL01=0-255 (256 total), RL02=0-511 (512 total)
   unsigned int last_sector;
   unsigned int last_head;
   unsigned int last_cylinder;
   unsigned int last_error;
   unsigned int rw_word_cnt;
   unsigned int rw_word_2s;
   unsigned int cs_cmd_rtn;
   unsigned int mp_status;
   unsigned int da_reg;
   unsigned long chridx;
   char *err_msg;
   char *drv_state;
   char last_blk[RL_SECTOR_BSIZE+2]; // One block of 128words or 256bytes
} RLDSK;

RLDSK *logical_sec2shc(unsigned int logical_sec);
RLDSK *rl_cmd(unsigned int, unsigned int);
RLDSK *rl_wait_cready(void);
RLDSK *rl_wait_dready(unsigned int drv, int status_fg, int reset_fg);
RLDSK *rl_seek(unsigned int drv, unsigned int cyl);
RLDSK *rl_read(unsigned int drv, unsigned int sec, unsigned int hed, unsigned int cyl, unsigned int words_cnt);
RLDSK *rl_read_hdr(void);
RLDSK *rl_status(unsigned int drv, int reset_fg);
char *rl_decode_err(unsigned int);
char *rl_decode_state(unsigned int);
int rl_fread(char* outptr,unsigned int len);

// RLDSK *rltr; /* Pointer to RLdsk struct */
#endif

