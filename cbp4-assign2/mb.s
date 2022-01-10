	.file	1 "mb.c"

 # GNU C 2.7.2.3 [AL 1.1, MM 40, tma 0.1] SimpleScalar running sstrix compiled by GNU C

 # Cc1 defaults:
 # -mgas -mgpOPT

 # Cc1 arguments (-G value = 8, Cpu = default, ISA = 1):
 # -quiet -dumpbase -O0 -o

gcc2_compiled.:
__gnu_compiled_c:
	.globl	loop_iters
	.sdata
	.align	2
loop_iters:
	.word	100000000
	.text
	.align	2
	.globl	main

	.text

	.loc	1 6
	.ent	main
main:
	.frame	$fp,40,$31		# vars= 16, regs= 2/0, args= 16, extra= 0
	.mask	0xc0000000,-4
	.fmask	0x00000000,0
	subu	$sp,$sp,40
	sw	$31,36($sp)
	sw	$fp,32($sp)
	move	$fp,$sp
	jal	__main
	sw	$0,16($fp)
	sw	$0,20($fp)
	sw	$0,24($fp)
$L2:
	lw	$2,24($fp)
	lw	$3,loop_iters
	slt	$2,$2,$3
	bne	$2,$0,$L5
	j	$L3
$L5:
	lw	$2,24($fp)
	li	$6,0x2aaaaaab		# 715827883
	mult	$2,$6
	mfhi	$5
	mflo	$4
	srl	$6,$5,0
	move	$7,$0
	sra	$4,$2,31
	subu	$3,$6,$4
	move	$5,$3
	sll	$4,$5,1
	addu	$4,$4,$3
	sll	$3,$4,1
	subu	$2,$2,$3
	bne	$2,$0,$L6
	li	$2,0x00000006		# 6
	sw	$2,16($fp)
$L6:
	lw	$2,24($fp)
	andi	$3,$2,0x0007
	bne	$3,$0,$L7
	li	$2,0x00000008		# 8
	sw	$2,20($fp)
$L7:
	lw	$3,16($fp)
	addu	$2,$3,1
	move	$3,$2
	sw	$3,16($fp)
	lw	$3,20($fp)
	addu	$2,$3,1
	move	$3,$2
	sw	$3,20($fp)
$L4:
	lw	$3,24($fp)
	addu	$2,$3,1
	move	$3,$2
	sw	$3,24($fp)
	j	$L2
$L3:
$L1:
	move	$sp,$fp			# sp not trusted here
	lw	$31,36($sp)
	lw	$fp,32($sp)
	addu	$sp,$sp,40
	j	$31
	.end	main
