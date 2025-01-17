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

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

team_t team = {
    /* Team name */
    "zteam",
    /* First member's full name */
    "Hau-Wei Lin",
    /* First member's email address */
    "linhoway@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*********************************************************
* Macros for data structures
 ********************************************************/

#define IMPLICIT                                    0
#define EXPLICIT                                    1
#define SEGREGATE                                   2
#define STRUCTURE                                   SEGREGATE

/*********************************************************
* Macros for replacement policies
 ********************************************************/

#define FIRST_FIT                                   0
#define BEST_FIT                                    1
#define NEXT_FIT                                    2
#define REPLACEMENT                                 FIRST_FIT

/*********************************************************
* Macros for block operation
 ********************************************************/

#define WSIZE                                       4
#define DSIZE                                       8
#define CHUNKSIZE                                   (1 << 10)

#define MAX(x, y)                                   ((x) > (y) ? (x) : (y))

/* create header or footer message */
#define PACK(size, alloc)                           ((size) | (alloc))

/* read & write a word at address p */
#define GET(p)                                      (*(unsigned int*)(p))
#define PUT(p, val)                                 (*(unsigned int*)(p) = (unsigned int)(val))

/* get size and allocated bit of a block */
#define GET_SIZE(p)                                 (GET(p) & ~0x7)
#define GET_ALLOC(p)                                (GET(p) & 0x1)

/* get header and footer from data block pointer */
#define HDRP(bp)                                    ((char *)(bp) - WSIZE)
#define FTRP(bp)                                    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* get previous and next data block by adding or subtracting block size */
#define NEXT_BLKP(bp)                               ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)                               ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* get previous block's footer */
#define PREV_FTRP(bp)                               ((char *)(bp) - DSIZE)

/* strategies for maintaining free list */
#if (STRUCTURE == EXPLICIT)
#define INSERT(bp)                                  explicit_insert_free_blk(bp)
#define REMOVE(bp)                                  explicit_remove_free_blk(bp)
#elif (STRUCTURE == SEGREGATE)
#define INSERT(bp)                                  segregate_insert_free_blk(bp)
#define REMOVE(bp)                                  segregate_remove_free_blk(bp)
#endif

/*********************************************************
* Macros for getting and setting explicit block's pointers
 ********************************************************/

#if (STRUCTURE != IMPLICIT)
#define GET_NEXT_FREE_BLKP(bp)                      ((void *)(*(unsigned int *)(bp)))
#define GET_PREV_FREE_BLKP(bp)                      ((void *)(*((unsigned int *)(bp) + 1)))
#define PUT_NEXT_FREE_BLKP(bp, val)                 (*(unsigned int *)(bp) = (unsigned int)(val))
#define PUT_PREV_FREE_BLKP(bp, val)                 (*((unsigned int *)(bp) + 1) = (unsigned int)(val))
#endif

/*********************************************************
* Macros for alignment operations
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT                                   DSIZE

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)                                 (((size) + (ALIGNMENT-1)) & ~0x7)

/* alignment size define */
#define SIZE_T_SIZE                                 (ALIGN(sizeof(size_t)))

/* minimun alignment block size */
#define MINIMUN_BLOCK                               DSIZE + SIZE_T_SIZE

/*********************************************************
* Macros for maintaining segregated list
 ********************************************************/

#if (STRUCTURE == SEGREGATE)
#define SEGREGATE_CASE_NUM                          7   /* 1, 3, 5, 7, 9, 11, 13 */
#define SEGREGATE_LIST_PADDING                      WSIZE * (SEGREGATE_CASE_NUM - 1)
#define GET_CASE_HEAD(case)                         ((void *)((char *)heap_listp - (SEGREGATE_CASE_NUM + 1)*WSIZE + (case)*WSIZE))
#define GET_CASE_HEAD_CONTENT(case)                 ((void *)(*((unsigned int *)GET_CASE_HEAD(case))))
#else
#define SEGREGATE_LIST_PADDING                      0
#endif

/*********************************************************
* Macros for debugging message
 ********************************************************/

// #define DEBUG_LIST_CHECKER
// #define DEBUG_MSG

#ifdef DEBUG_MSG
#define SHOW_WARNING()                              printf("[Warning] [File: %s] [Func: %s] [Line: %u]\n", __FILE__, __FUNCTION__, __LINE__)
#define PRINT_BLOCK(bp)                             printblock((bp))
#else
#define SHOW_WARNING()
#define PRINT_BLOCK(bp)
#endif

#if defined(DEBUG_LIST_CHECKER) && (STRUCTURE == EXPLICIT)
#define FREE_LIST_DETAIL()                          freelist_checker()
#elif defined(DEBUG_LIST_CHECKER) && (STRUCTURE == SEGREGATE)
#define FREE_LIST_DETAIL()                          caselist_checker()
#else
#define FREE_LIST_DETAIL()
#endif

/*********************************************************
* Global variables
 ********************************************************/

static char* heap_listp = NULL;

#if (STRUCTURE == IMPLICIT) && (REPLACEMENT == NEXT_FIT)
static char* rover = NULL;
#endif

#if (STRUCTURE == EXPLICIT)
static char* explicit_free_list_head = NULL;
#endif

/*********************************************************
* Function prototypes for internal helper for routines
 ********************************************************/

static void* extend_heap(size_t words);
static void place(void* bp, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* bp);
static void checkheap(int verbose);
static void checkblock(void* bp);
__inline static int checkheap_boundary(void* bp);
#ifdef DEBUG_MSG
static void printblock(void* bp);
#endif
#if (STRUCTURE == EXPLICIT)
static void explicit_insert_free_blk(void* bp);
static void explicit_remove_free_blk(void* bp);
#ifdef DEBUG_LIST_CHECKER
static void freelist_checker(void);
#endif
#elif (STRUCTURE == SEGREGATE)
static int segregate_case_chooser(int size);
static void segregate_insert_free_blk(void* bp);
static void segregate_remove_free_blk(void* bp);
#ifdef DEBUG_LIST_CHECKER
static void caselist_checker(void);
#endif
#endif
#if (STRUCTURE != IMPLICIT) && defined(DEBUG_LIST_CHECKER)
void cyclic_checker(void* bp);
#endif

/**
 * @brief extend heap with free block and return its block pointer 
 * note that one word represent 4 bytes
 * 
 * @param words how many words does the kernel should extend the heap
 */ 
static void* extend_heap(size_t words){

    char* bp;
    ssize_t size;
 
    /* since the block is 8 bytes alignment, we need to make sure that
     * the space we intend to enlarge is 2*words   
     */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if((long)(bp = (mem_sbrk(size))) == -1){

        SHOW_WARNING();
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));           // Header
    PUT(FTRP(bp), PACK(size, 0));           // Footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // Epilogue block
#if (STRUCTURE != IMPLICIT)
    PUT_NEXT_FREE_BLKP(bp, 0);              // next pointer
    PUT_PREV_FREE_BLKP(bp, 0);              // previous pointer
#endif

    return coalesce(bp);    // the end of the previous block might be free
}

/** 
 * @brief place - Place block of asize bytes at start of free block bp 
 * and split if remainder would be at least minimum block size
 * 
 * @param bp data block pointer
 * @param asize asize of block to be allocated
 */
static void place(void* bp, size_t asize){

    if(bp == NULL || asize <= 0){

        SHOW_WARNING();
        return;
    }

    size_t size = GET_SIZE(HDRP(bp));

#if (STRUCTURE != IMPLICIT)
        REMOVE(bp);
#endif     

    /* split */
    if((size - asize) >= MINIMUN_BLOCK){

        /* current block */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        /* new block */
        PUT(HDRP(bp), PACK((size-asize), 0));
        PUT(FTRP(bp), PACK((size-asize), 0));

#if (STRUCTURE != IMPLICIT)
        INSERT(bp);
#endif
    }
    /* no need for spliting */
    else{

        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }
}

/**
 * @brief find a free block that meets the asize requirment
 * 
 * @param asize asize of block that is intended to find
 */ 
static void* find_fit(size_t asize){

#if (REPLACEMENT == FIRST_FIT) || (STRUCTURE == SEGREGATE)
#if (STRUCTURE == IMPLICIT)
    char* bp = heap_listp;
    for(; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
#elif (STRUCTURE == EXPLICIT)
    char* bp = heap_listp;
    bp = explicit_free_list_head;
    for(; bp != NULL; bp = GET_NEXT_FREE_BLKP(bp)){
#elif (STRUCTURE == SEGREGATE)
    int case_range = segregate_case_chooser(asize);
    char* bp = NULL;

    for(; case_range<SEGREGATE_CASE_NUM; case_range++){
        bp = GET_CASE_HEAD_CONTENT(case_range);
        for(; bp != NULL; bp = GET_NEXT_FREE_BLKP(bp)){
#endif
            if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize){

                return bp;
            }
        }
#if (STRUCTURE == SEGREGATE)
    }
#endif

    return NULL; /* No fit found */
#elif (STRUCTURE == IMPLICIT) && (REPLACEMENT == NEXT_FIT)
    char *oldrover = rover;

    /* Search from the rover to the end of list */
    for (; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover)){

        if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover)))){
            
            return rover;
        }
    }

    /* search from start of list to old rover */
    for (rover = heap_listp; rover<oldrover; rover = NEXT_BLKP(rover)){

        if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover)))){
        
            return rover;
        }
    }

    return NULL;  /* no fit found */
#else /* best fit */
    char* bp = heap_listp;
    char* minibp = NULL;

#if (STRUCTURE == IMPLICIT)
    for(; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
#elif (STRUCTURE == EXPLICIT)
    bp = explicit_free_list_head;
    for(; bp != NULL; bp = GET_NEXT_FREE_BLKP(bp)){
#endif
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            
            if(minibp == NULL){

                minibp = bp;
            }else{

                minibp = (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(minibp))) ? bp : minibp;
            }
        }
    }
    return minibp;
#endif

    SHOW_WARNING();
    return NULL;
}

/**
 * @brief merge blocks when conducts free operation
 * 
 * @param bp data block pointer
 */ 
static void* coalesce(void* bp){

    if(bp == NULL){

        SHOW_WARNING();
        return bp;
    }

    size_t prev_alloc = GET_ALLOC(PREV_FTRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* case 1: none of the previos and next blocks aren free */
    if(prev_alloc && next_alloc){

    }
    /* case 2: next block is free */
    else if(prev_alloc && !next_alloc){

#if (STRUCTURE != IMPLICIT)
        REMOVE(NEXT_BLKP(bp));
#endif           
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(bp), PACK(size, 0));       
    }
    /* case 3: previous block is free */
    else if(!prev_alloc && next_alloc){

#if (STRUCTURE != IMPLICIT)
        REMOVE(PREV_BLKP(bp));
#endif         
        size += GET_SIZE(PREV_FTRP(bp));

        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);        
    }    
    /* case 4: both previous and next block are free */
    else if(!prev_alloc && !next_alloc){

#if (STRUCTURE != IMPLICIT)
        REMOVE(PREV_BLKP(bp));
        REMOVE(NEXT_BLKP(bp));
#endif
        size += GET_SIZE(PREV_FTRP(bp)) + GET_SIZE(HDRP(NEXT_BLKP(bp)));

        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);   
    }

#if (STRUCTURE == IMPLICIT) && (REPLACEMENT == NEXT_FIT)
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp))){

        rover = bp;
    }
#endif

#if (STRUCTURE != IMPLICIT)
    INSERT(bp);
#endif 

    return bp;
}

#ifdef DEBUG_MSG
/**
 * @brief print specific block's header and footer info
 * 
 * @param bp data block pointer
 */ 
static void printblock(void* bp){

    if(bp == NULL){

        return;
    }

    size_t hsize, halloc, fsize, falloc;

    checkheap(0);

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0) {

        SHOW_WARNING();
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, 
           hsize, (halloc ? 'a' : 'f'), 
           fsize, (falloc ? 'a' : 'f')); 
}
#endif

/**
 * @brief checks each block's correctness on the heap
 * 
 * @param verbose a flag to show extra info of block
 */
static void checkheap(int verbose){

    char* bp = heap_listp;

    if(verbose){

        printf("Heap (%p):\n", heap_listp);
    }

    if(GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp))){

        SHOW_WARNING();
        printf("Bad prologue header\n");
    }

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){

        if(verbose){

            PRINT_BLOCK(bp);
        }

        checkblock(bp);
    }

    if(verbose){

        PRINT_BLOCK(bp);
    }

    if(GET_SIZE(HDRP(bp)) || !GET_ALLOC(HDRP(bp))){

        SHOW_WARNING();
        printf("Bad epilouge header\n");
    }
}

/**
 * @brief check block's validity
 */ 
static void checkblock(void* bp){

    if(bp == NULL){

        SHOW_WARNING();
        exit(-1);
    }
    
    if((size_t)bp % 8){

        SHOW_WARNING();
        printf("Error: %p is not doubleword aligned\n", bp);
        exit(-1);
    }

    if(GET(HDRP(bp)) != GET(FTRP(bp))){

        SHOW_WARNING();
        printf("Error: header does not match footer\n");
        exit(-1);
    }    
}

/**
 * @brief function for checking if the pointer is valid
 * 
 * @param bp data block pointer
 * @return 0 if pointer bp is within the heap, -1 if it's not
 */
__inline static int checkheap_boundary(void* bp){

    return (bp < mem_heap_lo()) ? -1 : 
    ((char*)bp > (char*)mem_heap_hi() - 3 - SIZE_T_SIZE + WSIZE) ? -1 : 0;
}

#if (STRUCTURE == EXPLICIT)
/**
 * @brief insert block at the top of the explicit free list (LIFO)
 * 
 * @param bp data block address
 */
static void explicit_insert_free_blk(void* bp){

    if(bp == NULL){

        SHOW_WARNING();
        return;
    }

    /* LIFO */
    PUT_PREV_FREE_BLKP(bp, 0);

    if(explicit_free_list_head != NULL){

        PUT_PREV_FREE_BLKP(explicit_free_list_head, bp);
        PUT_NEXT_FREE_BLKP(bp, explicit_free_list_head);
    }else{

        PUT_NEXT_FREE_BLKP(bp, 0);
    }

    explicit_free_list_head = bp;

    FREE_LIST_DETAIL();
}

/**
 * @brief remove block from the explicit free list
 * 
 * @param bp data block address
 */
static void explicit_remove_free_blk(void* bp){

    if(bp == NULL){

        SHOW_WARNING();
        return;
    }

    if(explicit_free_list_head == NULL){

        SHOW_WARNING();
        return;
    }   

    /* LIFO */
    char* prev = GET_PREV_FREE_BLKP(bp);
    char* next = GET_NEXT_FREE_BLKP(bp);

    if(prev != NULL){

        PUT_NEXT_FREE_BLKP(prev, next);
    }

    if(next != NULL){

        PUT_PREV_FREE_BLKP(next, prev);
    }

    if(bp == explicit_free_list_head){

        explicit_free_list_head = GET_NEXT_FREE_BLKP(bp);
    }

    FREE_LIST_DETAIL();
}

#ifdef DEBUG_LIST_CHECKER
/**
 * @brief print and check all free blocks in explicit free list
 * 
 */
static void freelist_checker(void){

    cyclic_checker(explicit_free_list_head);

    char* ptr = explicit_free_list_head;

#ifdef DEBUG_MSG
    size_t hsize, halloc, fsize, falloc;
#endif
    char* nblk = NULL, *pblk = NULL;

    for(; ptr != NULL; ptr = GET_NEXT_FREE_BLKP(ptr)){

#ifdef DEBUG_MSG
        hsize = GET_SIZE(HDRP(ptr));
        halloc = GET_ALLOC(HDRP(ptr));  
        fsize = GET_SIZE(FTRP(ptr));
        falloc = GET_ALLOC(FTRP(ptr));
#endif

        nblk = GET_NEXT_FREE_BLKP(ptr);  
        pblk = GET_PREV_FREE_BLKP(ptr);

#ifdef DEBUG_MSG
        printf("%p: header: [%d:%c] footer: [%d:%c] next_block: [%p] previous_block: [%p]\n", 
                ptr, 
                hsize, (halloc ? 'a' : 'f'), 
                fsize, (falloc ? 'a' : 'f'),
                nblk, pblk);
#endif

        if(pblk != NULL && GET_NEXT_FREE_BLKP(pblk) != ptr){

            SHOW_WARNING();
            exit(-1);
        }

        if(nblk != NULL && GET_PREV_FREE_BLKP(nblk) != ptr){

            SHOW_WARNING();
            exit(-1);
        }

        checkblock(ptr);
    }

}
#endif
#elif (STRUCTURE == SEGREGATE)
/**
 * @brief choose list category base on size
 * 
 * @param size required block size
 * @return 
 */
 static int segregate_case_chooser(int size){

    if(size < MINIMUN_BLOCK){

        return -1;
    }

    int count = 0;
    
    size >>= (12 - SEGREGATE_CASE_NUM + 2);

    for(int i=1; i<SEGREGATE_CASE_NUM; i++){
           
        if(size > 0){
            
            count++;
            size >>= 1;  
        }else{

            break;
        }      
    }

    return count;
}

/**
 * @brief insert block to categorized list - block should be placed in ascending order
 *  
 * @param bp data block address
 */
static void segregate_insert_free_blk(void* bp){

    if(bp == NULL){

        SHOW_WARNING();
        return;
    }

    int case_range = segregate_case_chooser(GET_SIZE(HDRP(bp)));

    if(case_range == -1){

        SHOW_WARNING();
        return;
    }

    unsigned int** head = GET_CASE_HEAD(case_range);
    unsigned int* next = GET_CASE_HEAD_CONTENT(case_range);
    unsigned int* prev = NULL;    
    size_t size = GET_SIZE(HDRP(bp));

    while(next){

        if(size <= GET_SIZE(HDRP(next))){

            break;
        }else{

            prev = next;
            next = GET_NEXT_FREE_BLKP(next);
        }
    }

    if(next == *head){

        PUT_PREV_FREE_BLKP(bp, 0);
        PUT_NEXT_FREE_BLKP(bp, next);

        if(next != NULL){

            PUT_PREV_FREE_BLKP(next, bp);
        }

        PUT(head, bp);
    }else{

        PUT_PREV_FREE_BLKP(bp, prev);
        PUT_NEXT_FREE_BLKP(bp, next);
        PUT_NEXT_FREE_BLKP(prev, bp);

        if(next != NULL){

            PUT_PREV_FREE_BLKP(next, bp);
        }
    }

    FREE_LIST_DETAIL();
}

/**
 * @brief remove block from the segregated free list
 * 
 * @param bp data block address
 */
static void segregate_remove_free_blk(void* bp){

    if(bp == NULL){

        SHOW_WARNING();
        return;
    }

    int case_range = segregate_case_chooser(GET_SIZE(HDRP(bp)));

    if(case_range == -1){

        SHOW_WARNING();
        return;
    }

    unsigned int** list = GET_CASE_HEAD(case_range);    

    char* prev = GET_PREV_FREE_BLKP(bp);
    char* next = GET_NEXT_FREE_BLKP(bp);

    if(prev != NULL){

        PUT_NEXT_FREE_BLKP(prev, next);
    }else{

        PUT(list, next);
    }

    if(next != NULL){

        PUT_PREV_FREE_BLKP(next, prev);
    }

    FREE_LIST_DETAIL();
}

#ifdef DEBUG_LIST_CHECKER
/**
 * @brief  print and check all free blocks in each segregated free list
 * 
 */
static void caselist_checker(void){

#ifdef DEBUG_MSG
    size_t halloc, fsize, falloc;
#endif
    size_t hsize;
    char* nblk = NULL, *pblk = NULL;
    
    for(int i=0; i<SEGREGATE_CASE_NUM; i++){

        unsigned int** list = GET_CASE_HEAD(i);
        unsigned int* bp = *list;

#ifdef DEBUG_MSG
        printf("[case %d]:\n", i);
#endif
        cyclic_checker(bp);

        for(; bp != NULL; bp = GET_NEXT_FREE_BLKP(bp)){
            
            hsize = GET_SIZE(HDRP(bp));
#ifdef DEBUG_MSG
            halloc = GET_ALLOC(HDRP(bp));  
            fsize = GET_SIZE(FTRP(bp));
            falloc = GET_ALLOC(FTRP(bp));
#endif
            nblk = GET_NEXT_FREE_BLKP(bp);  
            pblk = GET_PREV_FREE_BLKP(bp);

#ifdef DEBUG_MSG
            printf("%p: header: [%d:%c] footer: [%d:%c] next_block: [%p] previous_block: [%p]\n", 
                    bp, 
                    hsize, (halloc ? 'a' : 'f'), 
                    fsize, (falloc ? 'a' : 'f'),
                    nblk, pblk); 
#endif
            if((nblk != NULL) && (GET_PREV_FREE_BLKP(nblk) != bp || hsize > GET_SIZE(HDRP(nblk)))){

                SHOW_WARNING();
                exit(-1);
            }

            if((pblk != NULL) && (GET_NEXT_FREE_BLKP(pblk) != bp || hsize < GET_SIZE(HDRP(pblk)))){

                SHOW_WARNING();
                exit(-1);
            }                     
            checkblock(bp);
        }
#ifdef DEBUG_MSG
        printf("\n");
#endif
    }
#ifdef DEBUG_MSG
    printf("\n----------------------end of segregated list check----------------------\n\n");
#endif
}
#endif
#endif

#if (STRUCTURE != IMPLICIT) && defined(DEBUG_LIST_CHECKER)
/**
 * @brief check if the list contain cyclic nodes
 * 
 */ 
void cyclic_checker(void* bp){

    char* hare = bp;
    char* tortoise = bp;

    while(hare){

        if(GET_NEXT_FREE_BLKP(hare) == NULL){

            break;
        }

        hare = GET_NEXT_FREE_BLKP(GET_NEXT_FREE_BLKP(hare));
        tortoise = GET_NEXT_FREE_BLKP(tortoise);

        if(hare == tortoise){

            SHOW_WARNING();
            exit(-1);
        }
    }
}
#endif

/*********************************************************
* this section contains major finctions:
* int mm_init(void)
* void* mm_malloc(size_t size)
* void mm_free(void* ptr)
* void* realloc(void* ptr, size_t size)
* void mm_checkheap(void)
 ********************************************************/

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void){

    if((heap_listp = mem_sbrk(4*WSIZE + SEGREGATE_LIST_PADDING)) == (void*)-1){

        SHOW_WARNING();
        return -1;
    }

#if (STRUCTURE == SEGREGATE)
    for(int i=0; i<SEGREGATE_CASE_NUM; i++){

        PUT(heap_listp + (i*WSIZE), 0);
    }
#else
    PUT(heap_listp + SEGREGATE_LIST_PADDING, PACK(0, 0));                       /* Alignment padding */
#endif

    PUT(heap_listp + (1*WSIZE) + SEGREGATE_LIST_PADDING, PACK(DSIZE, 1));       /* Prologue header */
    PUT(heap_listp + (2*WSIZE) + SEGREGATE_LIST_PADDING, PACK(DSIZE, 1));       /* Prologue footer */
    PUT(heap_listp + (3*WSIZE) + SEGREGATE_LIST_PADDING, PACK(0, 1));           /* Epilogue header */

    /* move pointer heap_listp */
    heap_listp += (2*WSIZE + SEGREGATE_LIST_PADDING);

#if (STRUCTURE == EXPLICIT)
    explicit_free_list_head = NULL;
#endif

    /* next fit pointer */
#if (STRUCTURE == IMPLICIT) && (REPLACEMENT == NEXT_FIT)
    rover = heap_listp;
#endif

    /* each block will be at least 8 bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL){

        SHOW_WARNING();
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

        SHOW_WARNING();
        mm_init();
    }

    if(size <= 0){

        SHOW_WARNING();
        return NULL;
    }

    size_t asize;
    size_t extendsize;
    char* bp;    

    /* 8 bytes alignment */
    asize = ALIGN(size);

    /* payload size plus header and footer */
    asize += DSIZE;

    /* check alignment */
    if(asize < size || asize % SIZE_T_SIZE != 0){

        SHOW_WARNING();
        return NULL;
    }

    /* find qualified free block */
    if((bp = find_fit(asize)) != NULL){

        place(bp, asize);
        return bp;
    }

    /* if no fit found, then get external memory form kernel and extend heap */
    extendsize = MAX(asize, CHUNKSIZE);

    if((bp = extend_heap(extendsize/WSIZE)) != NULL){

        place(bp, asize);
        return bp;
    }

    SHOW_WARNING();
    return NULL;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void* bp){

    if(bp == NULL){

        SHOW_WARNING();
        return;
    }

    if(heap_listp == NULL){

        SHOW_WARNING();
        mm_init();
    }

    size_t size = GET_SIZE(HDRP(bp));
    size_t alloc = GET_ALLOC(HDRP(bp));

    /* block must be previously allocated and within the range of heap */
    if(alloc == 0 || checkheap_boundary(bp)){

        SHOW_WARNING();
        return;
    }

    /* adjust header and footer allocated bit */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    /* coalescing blocks(if any) */
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    /* if size equal to zero then conduct mm_free() and return NULL */
    if(size == 0){

        mm_free(ptr);
        return NULL;
    }

    /* if ptr equal to NULL then this is just mm_malloc() */
    if(ptr == NULL){

        return mm_malloc(size);
    }

    size_t oldsize = GET_SIZE(HDRP(ptr));

    /* same size, doesn't have to do anything */
    if(oldsize - DSIZE >= size){

        return ptr;
    }

    char* next = NEXT_BLKP(ptr);

    if(next && GET_ALLOC(HDRP(next)) == 0){

        size_t nextsize = GET_SIZE(HDRP(next));

        if(oldsize + nextsize - DSIZE >= size){
#if (STRUCTURE != IMPLICIT)
            REMOVE(next);
#endif               
            PUT(HDRP(oldptr), PACK(oldsize+nextsize, 1));
            PUT(FTRP(next), PACK(oldsize+nextsize, 1));
              
            return oldptr;
        }
    }

    /* allocate a new block */
    newptr = mm_malloc(size);

    if (newptr == NULL){
    
        SHOW_WARNING();
        return NULL;
    }

    copySize = GET_SIZE(HDRP(oldptr)); // get old block size

    copySize = (copySize - DSIZE > size) ? size : copySize - DSIZE;
    
    /* copy content to the new block */
    memcpy(newptr, oldptr, copySize);

    /* free the old block */
    mm_free(oldptr);
    
    return newptr;
}

/* 
 * mm_checkheap - Check the heap for correctness
 */
void mm_checkheap(int verbose){

    checkheap(verbose);
}

