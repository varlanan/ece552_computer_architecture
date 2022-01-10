#include <stdio.h>

/* Large iteration counter value is able to filter out the initialization effects */
const int loop_iters = 1000000;

int main(void) {

    int loop6 = 0;
    int loop8 = 0;

    int i,j,k;
    for( i = 0; i < loop_iters; i++) {
        /* ASSUMPTION: We assume a branch is taken when the if statement is TRUE, and conversely
         *             branch is not-taken when the if statement is FALSE */

        /* Loop runs 6 times with and exits (branches) on the 7th 
         * the sequence of T/N:  T N N N N N T N N N N N T ...
         * since this configuration requires at least 5 history bits, 
         * our 2Level BP can handle it easily */
        if( i%6 == 0 ) {
            loop6 = 6;
        }

        /* Loop runs 8 times with and exits (branches) on the 9th 
         * the sequence of T/N:  T N N N N N N N N T N N N N N N N N T ...
         * since this configuration requires at least 8 history bits, 
         * our 2Level BP cannot handle since it only uses 6 history bits */
        if( i%8 == 0 ) {
            loop8 = 8;
        }

        loop6++;
        loop8++;
    }

}

/*
$L2:
	lw	$2,24($fp)
	lw	$3,loop_iters
	slt	$2,$2,$3
	bne	$2,$0,$L5       // For-loop enters if i < loop_iters; branches to L5 which continue
	j	$L3             // otherwise it jumps to end of loop
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
	bne	$2,$0,$L6           // if (i%6) == 0 - branch
	li	$2,0x00000006		# 6
	sw	$2,16($fp)
$L6:
	lw	$2,24($fp)
	andi	$3,$2,0x0007
	bne	$3,$0,$L7           // if (i%8) == 0 - branch
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

*/
