#include <stdio.h>

/* A large iteration counter value to be able to filter out the initialization effects */
const int loop_iters = 100000000;

int main(void) {
    
    int r1, r2, r3, r4, i;

    /* Main loop of interest: 
    * I1   addu	$17,$16,2
	* I2   addu	$16,$16,6
	* I3   addu	$18,$17,5
	* I4   addu	$3,$3,1
	* I5   slt	$2,$3,$4
	* I6   bne	$2,$0,$L17
    */

   /*                       1  2  3  4  5  6  7  8
    * I1   addu	$17,$16,2 | F  D  X  M  W
	* I2   addu	$16,$16,6 |    F  D  X  M  W
	* I3   addu	$18,$17,5 |       F  d* D  X  M  W
    *
    * 1 1-cycle data stall caused by RAW hazard of reg $17
    * 
    * Reg $17 is written at I1 and read at I3; this creates a RAW hazard
    * since there is no bypassing or forwarding for the 5 stage pipeline
    * we need to stall for 1 data cycle during I3 to have reg $17 written 
    * in the first half of cycle 5 in WriteBack stage, and ready in the 2nd half 
    * of the cycle in the Decode stage of I3
    *
    *                        1  2  3  4  5  6  7  8  9  10  11 
    * I4   addu	$3,$3,1    | F  D  X  M  W
	* I5   slt	$2,$3,$4   |    F  d* d* D  X  M  W
	* I6   bne	$2,$0,$L17 |       p* p* F  d* d* D  X  M   W
    * 
    * 2 RAW hazards requiring 2-cycle data stalls caused by the read-after-write
    *  of reg $3 between I4 and I5, and reg $2 between I5 and I6
    * 
    * SLT insn: if $3 (i) < $4 (loop_iters), then $2 is set to 1, otherwise set to 0
    * BNE insn: if $2 is not equal to 0 (which means we are still in the for loop), we 
    * branch to $L17 which is the beginning of our main block of interest
    * 
    */

    for( i = 0; i < loop_iters; i++) {
        r2 = r3;
        r1 = r2 + 2;
        r3 = r1 + 4;
        r4 = r1 + 5;
    }

    printf("r1 = %d\n", r1);
    printf("r3 = %d\n", r3);
    printf("r4 = %d\n", r4);


    return 0;

}