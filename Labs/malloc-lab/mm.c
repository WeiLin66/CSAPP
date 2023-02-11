/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*********************************************************
* Macros for block operation
 ********************************************************/
#define WSIZE                                      4
#define DSIZE                                      8    // double size
#define CHUNKSIZE                                  (1 << 12)   // extended heap size: 4096

#define MAX(x, y)                                  ((x) > (y) ? (x) : (y))

/* create header or footer message */
#define PACK(size,  alloc)                         ((size) | (alloc))

/* read & write a word at address p */
#define GET(p)                                     (*(unsigned int*)(p))
#define PUT(p, val)                                (*(unsigned int*)(p) = (val))

/* get size and allocated bit of a block */
#define GET_SIZE(p)                                (GET(p) & ~0x7)
#define GET_ALLOC(p)                               (GET(p) & 0x1)

/* get header and footer */
#define HDRP(bp)                                    (uint32_t*)(((uint32_t*)bp) - DSIZE)
#define FTRP(bp)                                    (uint32_t*)(((uint8_t*)bp + GET_SIZE(HDRP(bp))) - (2 * DSIZE))

/* get previous and next meta block by caculating current block point */
#define NEXT_BLKP(bp)                               (void*)((uint8_t*)bp + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)                               (void*)((uint8_t*)bp - GET_SIZE((uint8_t*)bp - (2*DSIZE))
/*********************************************************
* Macros for replacement policies
 ********************************************************/

#define FIRST_FIT       0
#define NEXT_FIT        1
#define BEST_FIT        2

#define REPLACEMENT     FIRST_FIT
// #define NEXT_FITx

/*********************************************************
* Macros for alignment operations
 ********************************************************/
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* alignment size define */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*********************************************************
* Global variables
 ********************************************************/
static uint8_t* heap_listp = NULL;

#if (REPLACEMENT == NEXT_FIT)
static uint8_t* rover = NULL;
#endif

/*********************************************************
* Function prototypes for internal helper for routines
 ********************************************************/
static void* extend_heap(size_t words);
static void place(void* bp, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* bp);
static void printblock(void* bp);
static void checkheap(int verbose);
static void checkblock(void* bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void){

    if((heap_listp = mem_sbrk(4 * DSIZE)) == (void*)-1){

        return -1;  // return -1 if exceed the boundary of heap
    }

    /* initialize 4 blocks */
    PUT(heap_listp, 0);
    PUT(heap_listp + DSIZE, PACK(2*DSIZE, 1));
    PUT(heap_listp + (2*DSIZE), PACK(2*DSIZE, 1));
    PUT(heap_listp + (3*DSIZE), PACK(0, 1));

    /* move pointer heap_listp */
    heap_listp += (2*DSIZE);

    /* next fit pointer */
    #if (REPLACEMENT == NEXT_FIT)
         // rover = heap_listp;
    #endif

    /* each block will be at least 8 bytes */
    if(extend_heap(CHUNKSIZE/DSIZE) == NULL){

        return -1;
    }

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size){

    if(heap_listp == NULL){

        mm_init();
    }

    if(size <= 0){

        return NULL;
    }

    size_t asize;
    uint8_t* bp;    

    /* 8 bytes alignment */
    asize = ALIGN(size);

    /* payload size plus header and footer */
    asize += (2*DSIZE);

    if((bp = find_fit(asize)) != NULL){

        place(bp, asize);
        return bp;
    }

    return NULL;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void* bp){

    /* error handles */
    if(bp == NULL){

        return;
    }

    if(heap_listp == NULL){

        return;
    }

    if((uint8_t*)bp < (uint8_t*)heap_listp + DSIZE || (uint8_t*)bp > (uint8_t*)mem_heap_hi - DSIZE){

        return;
    }

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);    
}

/**
 * @bried merge blocks when conducts free operation
 */ 
static void* coalesce(void* bp){

    size_t free_senario = GET_ALLOC(HDRP(PREV_BLKP(bp))) | (GET_ALLOC(HDRP(NEXT_BLKP(bp))) << 1);   
    size_t size = GET_SIZE(HDRP(bp));
    uint8_t* ret_pointer = bp;

    free_senario &= 0x0000000f;

    switch(free_senario){

        /* case 0: none of the previos and next blocks aren free */
        case 0:{
            // no need for coalescing
        }break;
        
        /* case 1: previous block is free */
        case 1:{
            uint32_t total_size = size + GET_SIZE(HDRP(PREV_BLKP(bp)));
            ret_pointer = PREV_BLKP(bp);

            PUT(FTRP(bp), PACK(total_size, 0));
            PUT(HDRP(ret_pointer), PACK(total_size, 0));
        }break;

        /* case 2: next block is free */
        case 3:{
            uint32_t total_size = size + GET_SIZE(HDRP(NEXT_BLKP(bp)));

            PUT(FTRP(NEXT_BLKP(bp)), PACK(total_size, 0));
            PUT(HDRP(bp), PACK(total_size, 0));
        }break;

        /* case 3: both previous and next block are free */
        case 4:{
            uint32_t total_size = size +            \
                    GET_SIZE(HDRP(PREV_BLKP(bp))) + \
                    GET_SIZE(HDRP(NEXT_BLKP(bp)));

            ret_pointer = PREV_BLKP(bp);

            PUT(FTRP(NEXT_BLKP(bp)), PACK(total_size, 0));
            PUT(HDRP(ret_pointer), PACK(total_size, 0));
        }break;
    }

    #if (REPLACEMENT == NEXT_FIT)

    #endif

    return ret_pointer;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((uint8_t *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**
 * @brief extend heap with free block and return its block pointer 
 * note that one word represent 4 bytes
 */ 
static void* extend_heap(size_t words){

    uint8_t* bp;
    ssize_t size;
 
    size = word * DSIZE; // 8 bytes alignment

    if((long)(bp = (mem_sbrk(size))) == -1){

        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    reutrn coalesce(bp);
}

/** 
 * @brief place - Place block of asize bytes at start of free block bp 
 * and split if remainder would be at least minimum block size
 */
static void place(void* bp, size_t asize){

    if(bp == NULL || asize <= 0){

        return;
    }

    size_t size = GET_SIZE(HDRP(bp));

    if(size - asize >= (2*DSIZE) + ALIGNMENT){ // split

        /* current block */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        /* new block */
        PUT(HDRP(bp), PACK((size-asize), 0));
        PUT(FTRP(bp), PACK((size-asize), 0));
    }else{ // no need for spliting

        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }
}

/**
 * @brief find a free block that meets the asize requirment
 */ 
static void* find_fit(size_t asize){

    #if (REPLACEMENT == FIRST_FIT)
        void* bp = NULL;

        for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){

            if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize){

                return bp;
            }
        }

        return NULL; /* No fit */
    #elif (REPLACEMENT == NEXT_FIT)
        // need implementation
    #else /* best fit */
        // need implementation
    #endif

    return NULL;
}

/**
 * @brief print specific block's header and footer info
 */ 
static void printblock(void* bp){

    size_t hsize, halloc, fsize, falloc;

    checkheap(0);

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0) {

        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, 
           hsize, (halloc ? 'a' : 'f'), 
           fsize, (falloc ? 'a' : 'f')); 
}

/**
 * @brief check block's validity
 */ 
static void checkblock(void* bp){
    
    if((size_t)bp % 8){

        printf("Error: %p is not doubleword aligned\n", bp);
    }

    if(GET(HDRP(bp)) != GET(FTRP(bp))){

        printf("Error: header does not match footer\n");
    }    
}

void checkheap(int verbose){

    char* bp = heap_listp;

    if(verbose){

        printf("Heap (%p):\n", heap_listp);
    }

    if(GET_SIZE(HDRP(heap_listp)) != (2*DSIZE) ||
        !GET_ALLOC(HDRP(heap_listp))){

        printf("Bad prologue header\n");
    }

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){

        if(verbose){

            printblock(bp);
        }

        checkblock(bp);
    }

    if(verbose){

        printblock(bp);
    }

    if(GET_SIZE(HDRP(bp)) || !GET_ALLOC(HDRP(bp))){

        printf("Bad epilouge header\n");
    }
}