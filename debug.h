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
#ifndef NODEBUG				/* NODEBUG inhibits debugging */
#ifndef DEBUG				/* and if DEBUG not already defined */
#ifndef MINSIZE				/* MINSIZE inhibits debugging */
#ifndef DEBUG
#define DEBUG
#endif /* DEBUG */
#endif /* MINSIZE */
#endif /* DEBUG */
#endif /* NODEBUG */

#ifdef DEBUG				/* Debugging included... */
/* dodebug() function codes... */
#define DB_OPN 1			/* Open log */
#define DB_LOG 2			/* Write label+string or int to log */
#define DB_MSG 3			/* Write message to log */
#define DB_CHR 4			/* Write label + char to log */
#define DB_PKT 5			/* Record a Kermit packet in log */
#define DB_CLS 6			/* Close log */

void dodebug(int, UCHAR *, UCHAR *, long); /* Prototype */
/*
  dodebug() is accessed throug a macro that:
   . Coerces its args to the required types.
   . Accesses dodebug() directly or thru a pointer according to context.
   . Makes it disappear entirely if DEBUG not defined.
*/
#ifdef KERMIT_C
/* In kermit.c we debug only through a function pointer */
#define debug(a,b,c,d) \
if(*(k->dbf))(*(k->dbf))(a,(UCHAR *)b,(UCHAR *)c,(long)(d))

#else  /* KERMIT_C */
/* Elsewhere we can call the debug function directly */
#define debug(a,b,c,d) dodebug(a,(UCHAR *)b,(UCHAR *)c,(long)(d))
#endif /* KERMIT_C */

#else  /* Debugging not included... */

#define debug(a,b,c,d)
#endif /* DEBUG */
