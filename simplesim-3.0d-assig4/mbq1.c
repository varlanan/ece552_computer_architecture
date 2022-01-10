#include <stdio.h>

/* Large iteration counter value is able to filter out the initialization effects */
const int loop_iters = 1000000;
const int cache_max_size = 1000000;
/* step size changes from 2 to 3 to observe changes in the next line prefetcher */
const int access_step_size = 3; 

int main(void) {

    int i,j;
    /* array of INTs : each INT is 4 bytes
     * the q1 configuration files is set up the following way:
     *      num_sets = 64, block size = 8 bytes, associativity = 1
     * upon accessing every 2nd array element (every 8 bytes) 
     * the next line prefetches never misses since it is always predicting 
     * that the very next block (made of 8 bytes) is gonna be accessed,
     * and it prefetches it every time 
     */
    int* arr = (int *)malloc(sizeof(int) * cache_max_size);
    for( i = 0; i < loop_iters; i++) {
        for( j = 0; j < cache_max_size; j = j + access_step_size) {
            arr[j] = 7;
        }
    }

}