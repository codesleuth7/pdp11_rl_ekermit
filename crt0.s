    	.text
    	.even
    	.globl	_main
    	.globl	___main
    	.globl	_start
    	.globl	_ckintr
    	.globl	_dlrintr
    	.globl	_dlxintr
    	.globl	_cksec	# External C subroutine
    	.globl	_ckint	# External C subroutine
    	.globl	_cktick	# External unsigned int in the C code
    	.globl	_spnow	# External unsigned int in the C code
    	.globl	_rcvintr	# External C subroutine
    	.globl	_xmtintr	# External C subroutine

#############################################################################*
##### _start: initialize stack pointer,
#####         clear vector memory area,
#####         save program entry in vector 0
#####         call C main() function
#############################################################################*
_start:
	#mov	$00776,sp
	mov	$01776,sp
	clr	r0
L_0:
	clr	(r0)+
	cmp	r0, $400
	bne	L_0
        mov	$000137,*$0     # Store JMP _start in vector 0
        mov	$_start,*$02
	mov	$_dlrintr,*$000060  # Vector 060 -> dlintr
	mov	$000200,*$000062   # SP = 00200 BR4 Priority=4
	mov	$_dlxintr,*$000064  # Vector 060 -> dlintr
	mov	$000200,*$000066   # SP = 00200 BR4 Priority=4
        mov	$_ckintr,*$000100  # Vector 100 -> ckintr
        mov	$000300,*$000102   # SP = 00300
	mov	$0177546,r1	# CLK SR
	mov	$0000100,(r1)   # Enable CLK interrupts
	mov	$0177560,r1	# DL11 CSR
	mov	$0000100,(r1)   # Enable DL11 Rcv interrupts
	jsr 	pc,_main
	halt
        br	_start

#############################################################################*
##### ___main: called by C main() function. Currently does nothing
#############################################################################*
___main:
	rts	pc
_ckintr:
	mov	r0, -(sp)      # Push R0
	mov	$0177546,r0	# CLK SR
	mov	$0000100,(r0)	# Enable CLK interrupt
	mov	r1, -(sp)      # Push R1
	mov	r2, -(sp)      # Push R2
	mov	r3, -(sp)      # Push R3
	mov	r4, -(sp)      # Push R4
	mov	r5, -(sp)      # Push R5
	mov	$_spnow, r0    # Address of spnow
	mov	sp, (r0)       # Save value of SP into spnow
	jsr	pc,_ckint
	mov	(sp)+, r5      # Pop R5
	mov	(sp)+, r4      # Pop R4
	mov	(sp)+, r3      # Pop R3
	mov	(sp)+, r2      # Pop R2
	mov	(sp)+, r1      # Pop R1
	mov	(sp)+, r0      # Pop R0
	rti

_dlrintr:
	mov	r0, -(sp)      # Push R0
	mov	r1, -(sp)      # Push R1
	mov	r2, -(sp)      # Push R2
	mov	r3, -(sp)      # Push R3
	mov	r4, -(sp)      # Push R4
	mov	r5, -(sp)      # Push R5
	jsr	pc,_rcvintr
	mov	(sp)+, r5      # Pop R5
	mov	(sp)+, r4      # Pop R4
	mov	(sp)+, r3      # Pop R3
	mov	(sp)+, r2      # Pop R2
	mov	(sp)+, r1      # Pop R1
	mov	(sp)+, r0      # Pop R0
	rti

_dlxintr:
	mov	r0, -(sp)      # Push R0
	mov	r1, -(sp)      # Push R1
	mov	r2, -(sp)      # Push R2
	mov	r3, -(sp)      # Push R3
	mov	r4, -(sp)      # Push R4
	mov	r5, -(sp)      # Push R5
	jsr	pc,_xmtintr
	mov	(sp)+, r5      # Pop R5
	mov	(sp)+, r4      # Pop R4
	mov	(sp)+, r3      # Pop R3
	mov	(sp)+, r2      # Pop R2
	mov	(sp)+, r1      # Pop R1
	mov	(sp)+, r0      # Pop R0
	rti
