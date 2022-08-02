/*
Course: CS 518
Team Members:
    * Abhinay Reddy Vongur (av730)
    * Anmol Sharma (as3593)
Machine used:
    * ilabu3 (ilabu3.cs.rutgers.edu) 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_TOP_BITS 4 //top bits to extract
#define BITMAP_SIZE 4 //size of the bitmap array
#define SET_BIT_INDEX 17 //bit index to set 
#define GET_BIT_INDEX 17 //bit index to read

static unsigned int myaddress = 4026544704;   // Binary  would be 11110000000000000011001001000000


/* 
 * Function 1: EXTRACTING OUTER (TOP-ORDER) BITS
 */
static unsigned int get_top_bits(unsigned int value,  int num_bits)
{
	//Implement your code here
    // Right shift the value till it becomes 0 while incrementing the count to get the length of its binary representation
    int binary_len = 0;
    unsigned int val_copy = value;
    while (val_copy != 0) {
        val_copy = val_copy >> 1;
        binary_len++;
    }
    return value >> (binary_len - num_bits);
	
}


/* 
 * Function 2: SETTING A BIT AT AN INDEX 
 * Function to set a bit at "index" bitmap
 */
static void set_bit_at_index(char *bitmap, int index)
{
    //Implement your code here
    // get the index of the byte where the bit is in
    int byte_pos = (int) index/8;
    // get the index of the bit in the particular byte
    int bit_pos = index%8;
    // move the bit 1 to required position using right shift
    // use bitwise or operation to set the bit
    bitmap[byte_pos] = bitmap[byte_pos] | (128 >> bit_pos);
    return;
}


/* 
 * Function 3: GETTING A BIT AT AN INDEX 
 * Function to get a bit at "index"
 */
static int get_bit_at_index(char *bitmap, int index)
{
    //Get to the location in the character bitmap array
    //Implement your code here
    // get the index of the byte where the bit is in
    int byte_pos = (int) index/8;
    // get the index of the bit in the particular byte
    int bit_pos = index%8;
    // get the bit by using right shift and bitwise and
    return (bitmap[byte_pos] >> (8 - bit_pos - 1)) & 1;
    
}


int main () {

    /* 
     * Function 1: Finding value of top order (outer) bits Now let’s say we
     * need to extract just the top (outer) 4 bits (1111), which is decimal 15  
    */
    unsigned int outer_bits_value = get_top_bits (myaddress , NUM_TOP_BITS);
    printf("Function 1: Outer bits value %u \n", outer_bits_value); 
    printf("\n");

    /* 
     * Function 2 and 3: Checking if a bit is set or not
     */
    char *bitmap = (char *)malloc(BITMAP_SIZE);  //We can store 32 bits (4*8-bit per character)
    memset(bitmap,0, BITMAP_SIZE); //clear everything
    
    /* 
     * Let's try to set the bit 
     */
    set_bit_at_index(bitmap, SET_BIT_INDEX);
    
    /* 
     * Let's try to read bit)
     */
    printf("Function 3: The value at %dth location %d\n", 
            GET_BIT_INDEX, get_bit_at_index(bitmap, GET_BIT_INDEX));
            
    return 0;
}
