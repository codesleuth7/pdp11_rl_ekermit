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
/* pdp11 platform.h for EK */


#define HAVE_UCHAR 1
#define HAVE_ULONG 1
#define HAVE_USHORT 1


#ifndef IBUFLEN
#define IBUFLEN  4096			/* File input buffer size */
#endif /* IBUFLEN */

#ifndef OBUFLEN
#define OBUFLEN  8192                   /* File output buffer size */
#endif /* OBUFLEN */

