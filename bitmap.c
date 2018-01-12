// free bitmap for OS file systems assignment

#include "bitmap.h"

#include <strings.h>    // for `ffs`

/* constants */
// how far to loop in array
#define SIZE (NUM_BLOCKS/8) //128

/* globals */
// the actual data. initialize all bits to high
uint8_t freebitmap[SIZE] = { [0 ... SIZE-1] = UINT8_MAX };

/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

void force_set_index(uint32_t index) {
    // Used to force indicies to used 
    // this is the opposite of rm_index. 
    uint32_t i = index / 8;
    uint8_t bit = index % 8;
    USE_BIT(freebitmap[i], bit);
}

uint32_t get_index() {
    uint32_t i = 0;

    // find the first section with a free bit
    // let's ignore overflow for now...
    while (freebitmap[i] == 0) { i++; }

    // now, find the first free bit
    /*
        The ffs() function returns the position of the first (least
       significant) bit set in the word i.  The least significant bit is
       position 1 and the most significant position is, for example, 32 or
       64.  
    */
    // Since ffs has the lsb as 1, not 0. So we need to subtract
    uint8_t bit = ffs(freebitmap[i]) - 1;

    // set the bit to used //Got rid of this functionality, use force_set_index() to set the bits
//    USE_BIT(freebitmap[i], bit);

    //return which block we used
    return i*8 + bit;
}

void rm_index(uint32_t index) {

    // get index in array of which bit to free
    uint32_t i = index / 8;

    // get which bit to free
    uint8_t bit = index % 8;

    // free bit
    FREE_BIT(freebitmap[i], bit);
}

