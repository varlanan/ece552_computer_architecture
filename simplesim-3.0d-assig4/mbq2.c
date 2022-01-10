#include <stdio.h>

/* Large iteration counter value is able to filter out the initialization effects */
const int loop_iters = 1000000;
const int cache_max_size = 1000000;

int main(void) {

    int i,j;
    /* array of INTs : each INT is 4 bytes
     * the q1 configuration files is set up the following way:
     * - num_sets = 64, block size = 8 bytes, associativity = 1 , num_RPT_entries = 64
     * 
     */

    int access_step_size = 10;
    int* arr = (int *)malloc(sizeof(int) * cache_max_size);

    /* FIRST CASE: Tests the performance of the stride prefetcher 
     *              on array accesses with constant strides
     * - We set the access_step_size at a constant value (here 10)
     *   and thus we observe a low miss rate of just 7.8% on L1 data cache
     */
    //for( i = 0; i < loop_iters; i++) {
    //    access_step_size = 5;
    //    for( j = 0; j < cache_max_size; j = j + access_step_size) {
    //        arr[j] = 7;
    //    }
    //}

    /* SECOND CASE: Tests the performance of the stride prefetcher 
     *              on array accesses with non-constant strides
     * - We set the access_step_size at value that changes itself 
     *   on every iteration of the loop through the array in increments of 10
     *   and thus we observe a high miss rate of 99.98% on L1 data cache, as the
     *   stride prefetcher cannot capture any patterns
     */

    //for( i = 0; i < loop_iters; i++) {
    //    access_step_size = 5;
    //    for( j = 0; j < cache_max_size; j = j + access_step_size) {
    //        access_step_size += 10;
    //        arr[j] = 7;
    //    }
    //}

    /* THIRD CASE: Tests the performance of the stride prefetcher 
     *              on array accesses that change their stride every 
                    n (here n=22) element accesses
     * - We set the access_step_size at value that changes itself 
     *   on every 22nd iteration of the loop through the array also in increments of 10
     *   and thus we observe a high miss rate of 94.76% on L1 data cache, which means 
     *   the stride prefetcher can capture some patterns when they stay constant.
     *   However, since the size of array is much larger than the interval chosen for changing
     *   the stride, we still see a high miss rate, but visibly smaller than for a completely
     *   non-constant stride
     */
    for( i = 0; i < loop_iters; i++) {
        access_step_size = 5;
        for( j = 0; j < cache_max_size; j = j + access_step_size) {
            if ((j % 22) == 0) access_step_size += 10;
            arr[j] = 7;
        }
    }



}