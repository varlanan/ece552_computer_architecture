	.file	1 "mbq1.c"

 # GNU C 2.7.2.3 [AL 1.1, MM 40, tma 0.1] SimpleScalar running sstrix compiled by GNU C

 # Cc1 defaults:
 # -mgas -mgpOPT

 # Cc1 arguments (-G value = 8, Cpu = default, ISA = 1):
 # -quiet -dumpbase -O1 -o

gcc2_compiled.:
__gnu_compiled_c:
	.globl	loop_iters
	.sdata
	.align	2
loop_iters:
	.word	100000000
	.rdata
	.align	2
$LC0:
	.ascii	"r1 = %d\n\000"
	.align	2
$LC1:
	.ascii	"r3 = %d\n\000"
	.align	2
$LC2:
	.ascii	"r4 = %d\n\000"
	.text
	.align	2
	.globl	main

	.extern	stdin, 4
	.extern	stdout, 4

	.text

	.loc	1 6
	.ent	main
main:
	.frame	$sp,40,$31		# vars= 8, regs= 4/0, args= 16, extra= 0
	.mask	0x80070000,-4
	.fmask	0x00000000,0
	subu	$sp,$sp,40
	sw	$31,36($sp)
	sw	$18,32($sp)
	sw	$17,28($sp)
	sw	$16,24($sp)
	jal	__main
	.set	noreorder
	lw	$2,loop_iters
	.set	reorder
	move	$3,$0
	move	$4,$2
	blez	$2,$L15
$L17:
	addu	$17,$16,2
	addu	$16,$16,6
	addu	$18,$17,5
	addu	$3,$3,1
	slt	$2,$3,$4
	bne	$2,$0,$L17
$L15:
	la	$4,$LC0
	move	$5,$17
	jal	printf
	la	$4,$LC1
	move	$5,$16
	jal	printf
	la	$4,$LC2
	move	$5,$18
	jal	printf
	move	$2,$0
	lw	$31,36($sp)
	lw	$18,32($sp)
	lw	$17,28($sp)
	lw	$16,24($sp)
	addu	$sp,$sp,40
	j	$31
	.end	main
