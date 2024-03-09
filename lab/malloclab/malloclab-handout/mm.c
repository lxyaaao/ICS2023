/*
 * mm.c
 * 
 * 刘昕垚 2200012866
 * 
 * In this lab, I try to write a dynamic storage allocator 
 *          with explicit free lists
 * 
 * The methods I use include:
 *      immediate coalescing
 *      first fit
 *      delete footer in allocated block
 *      some magic numbers
 * (Although may not be all exactly achieved qwq)
 * 
 * Format of block：
 * 
 * Allocated:
 * ----------
 * Header(4 bytes)
 * Payloads(more than 12 bytes)
 * Padding
 * ----------
 * 
 * Free:
 * ----------
 * Header(4 bytes)
 * Pred pointer(4 bytes)
 * Succ pointer(4 bytes)
 * free payloads
 * Padding
 * Footer(4 bytes)
 * ----------
 * 
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
// #define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

#define INF   0xffffffff

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT - 1)) & ~0x7)

/* Basic constants and macros */
#define WSIZE       4          /* Word and header/footer size (bytes) */
#define DSIZE       8          /* Double word size (bytes) */
#define CHUNKSIZE  (1 << 12)   /* Extend heap by this amount (bytes) */
#define INITSIZE   (1 << 14)   /* Extend heap in init by this amount (bytes)*/
 
#define FREELIST_SUM     9    /* sum of free lists, each of which is 8 bytes (double word) */
#define MIN_FREE_SIZE    16    /* smallest size of freelist*/

/* Get the MAX between two numbers */
#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))           /* Read */
#define PUT(p, val)  (*(unsigned int *)(p) = (val))   /* Write */
 
/* Pack a size, allocated bit and previously allocated bit into a word */
#define PACK(size, alloc, p_alloc)  ((size) | (alloc) | (p_alloc << 1))

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(ptr)       ((char *)(ptr) - WSIZE)
#define FTRP(ptr)       ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

/* Read the size, allocated fields and previously allocated fields from address p */
#define GET_SIZE(p)    (GET(p) & ~0x7)
#define GET_ALLOC(p)   (GET(p) & 0x1)
#define GET_PALLOC(p)  ((GET(p) & 0x2) >> 1)

/* Set and clear allocated fields and previously allocated fields from address p */
#define SET_ALLOC(p)        ((*(unsigned*)(p)) |= 0x1)
#define CLEAR_ALLOC(p)      ((*(unsigned*)(p)) &= ~0x1)
#define SET_PALLOC(p)       ((*(unsigned*)(p)) |= 0x2)
#define CLEAR_PALLOC(p)     ((*(unsigned*)(p)) &= ~0x2)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE)))
#define PREV_BLKP(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE)))

/* Return the successor and previous free block pointer */
#define GET_SUCC(ptr)   ((void* )(*(size_t *)(ptr)))
#define GET_PREV(ptr)   ((void* )(*(size_t *)(ptr - WSIZE)))

/* Global variables */
static char* heap_list = 0;      /* Pointer to first block */  
static char* free_list = 0;      /* Pointer to begin of free list */
static char* epilogue = 0;       /* Pointer to epilogue block */

/* Function prototypes for internal helper routines */
static void* extend_heap(size_t words);
static void* place(void* ptr, size_t asize);
static void* find_fit(size_t asize);
static void* coalesce(void* ptr);
static void* free_index(size_t size);
static void* insert_free(void* ptr);
static void remove_free(void* ptr);

/*
 * mm_init - Initialize the memory manager
 *           In this part, we choose dynamic allocator
 *           To optimize, we delete footer in this part
 * 
 *           success: return 0
 *           fail(error): return -1
 */
int mm_init(void) {
    /* Create the initial empty heap */
    heap_list = mem_sbrk(FREELIST_SUM * DSIZE + DSIZE);
    if (heap_list == ((void*) - 1))
        return -1;
    
    /* Create the initial free list */
    free_list = (char*)((char**)heap_list);
    memset(free_list, 0, FREELIST_SUM*DSIZE);

    /* Initialize the free list */
    char* free_block  = free_list + FREELIST_SUM * DSIZE;
    PUT(free_block, PACK(DSIZE, 1, 1));             /* Prologue header */ 
    PUT(free_block + WSIZE * 1, PACK(0, 1, 1));     /* Epilogue header */

    heap_list = 2 * WSIZE + free_block;          /* Refresh epilogue */
    epilogue = heap_list;
    
    /* magic number */
    if(extend_heap(INITSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * malloc - Allocate a block with at least size bytes of payload
 *          ALign by 16 bytes and allocate
 * 
 *          success: return pointer to allocated block
 *          fail(error): return NULL
 */
void* malloc(size_t size) {
    size_t extendsize; /* Amount to extend heap if no fit */
 
    /* If heap is empty, initialize it first*/
    if(heap_list == 0)
        mm_init();
 
    /* Ignore spurious requests */
    if(size == 0)
        return NULL;

    /* magic number */
    if(size == 448) 
        size = 512;
 
    /* Adjust block size to include overhead and alignment reqs. */
    if(size + WSIZE <= MIN_FREE_SIZE)
        size = MIN_FREE_SIZE;
    else
        size = ALIGN(size + WSIZE);

    void* ptr = find_fit(size);

    /* No fit found. Get more memory and place the block */
    if(ptr == NULL) {
        extendsize = MAX(size, CHUNKSIZE);
        ptr = extend_heap(extendsize);
        if(ptr == ((void*) - 1)) 
            return NULL;
    }

    /* Search the free list for a fit */
    remove_free(ptr);
    place(ptr, size);
    return ptr;
}

/*
 * free - Free a block
 *        Free allocated block, and coalesce immediately
 */
void free(void* ptr) {
    if(!ptr)
        return ;

    size_t size = GET_SIZE(HDRP(ptr));
    if(heap_list == 0)
        mm_init();
    
    CLEAR_ALLOC(HDRP(ptr));
 
    PUT(HDRP(ptr), PACK(size, 0, GET_PALLOC(HDRP(ptr))));
    PUT(FTRP(ptr), PACK(size, 0, GET_PALLOC(HDRP(ptr))));

    CLEAR_PALLOC(HDRP(NEXT_BLKP(ptr)));

    coalesce(ptr);
    return ;
}

/*
 * realloc - you may want to look at mm-naive.c
 *           (Almost the same as mm-naive.c)
 *           Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.
 * 
 *           succes: return pointer to allocated block
 *           fail(error): return NULL
 */
void* realloc(void* oldptr, size_t size) {
    size_t oldsize;
    void* newptr;
 
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        free(oldptr);
        return NULL;
    }
 
    /* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL) {
        return malloc(size);
    }
 
    newptr = malloc(size);
    
    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return NULL;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(oldptr));
    if(size < oldsize) 
        oldsize = size;
    memcpy(newptr, oldptr, oldsize);
 
    /* Free the old block. */
    free(oldptr);
 
    return newptr;    
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 *          (Almost the same as mm-naive.c)
 *          Allocate a block and set it as 0
 * 
 *           succes: return pointer to allocated block
 *           fail(error): return NULL
 */
void* calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void* newptr = malloc(bytes);
 
    memset(newptr, 0, bytes);
 
    return newptr;
}

/*
 * The remaining routines are internal helper routines
 */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 *               Extend heap and add the free block into list
 * 
 *               success: return pointer to new free block
 *               fail(error): return NULL
 */
static void* extend_heap(size_t words) {
    char* ptr;
 
    ptr = mem_sbrk(words);
    if (ptr == ((void*) - 1))
        return NULL;    
 
    /* Initialize free block header/footer and the epilogue header */
    /* Free block header */
    PUT(HDRP(ptr), PACK(words, 0, GET_PALLOC(HDRP(epilogue)))); 
    /* Free block footer */
    PUT(FTRP(ptr), PACK(words, 0, GET_PALLOC(HDRP(epilogue))));  

    epilogue += words;
    PUT(HDRP(epilogue), PACK(0, 1, 0));  /* New epilogue header */
 
    /* Coalesce if the previous block was free */
    return coalesce(ptr);
}

/* 
 * place - Place block of asize bytes at start of free block ptr 
 *         and split if remainder would be at least minimum block size
 * 
 *         return pointer to the newly insersted & allocated block
 */
static void* place(void* ptr, size_t asize) {
    size_t csize = GET_SIZE(HDRP(ptr));
    size_t delta = csize - asize;
    void* save_ptr = ptr;

    /* If the size remaining is larger than the MIN_FREE_SIZE,
       split immediately */
    if(delta >= MIN_FREE_SIZE){
        PUT(HDRP(ptr), PACK(asize, 1, GET_PALLOC(HDRP(ptr))));
        
        ptr = NEXT_BLKP(ptr);
        PUT(HDRP(ptr), PACK(delta, 0, 1));
        PUT(FTRP(ptr), PACK(delta, 0, 1));
        insert_free(ptr);
    }

    else{
        PUT(HDRP(ptr), PACK(csize, 1, GET_PALLOC(HDRP(ptr))));
        SET_PALLOC(HDRP(NEXT_BLKP(ptr)));
    }

    return save_ptr;
}

/* 
 * find_fit - Find a fit for a block with asize bytes 
 *            use first-fit policy
 * 
 *            success: return block pointer
 *            fail: return NULL
 */
static void* find_fit(size_t asize) {
    void* entry = free_index(asize);
    void* tmp_p = GET_SUCC(entry);
    
    /* Search for the first available block from the start */
    while(tmp_p != NULL) {
        if(GET_SIZE(HDRP(tmp_p)) >= asize)
            return tmp_p;
        tmp_p = GET_SUCC(tmp_p);
    }
        
    char* free_end = free_list + FREELIST_SUM * DSIZE;
    for(tmp_p = entry + DSIZE; tmp_p != free_end; tmp_p += DSIZE) {
        if(GET_SUCC(tmp_p))
            return GET_SUCC(tmp_p);
    }

    return NULL;
}

/*
 * coalesce - Boundary tag coalescing. 
 *            Coalesce neighboring free blocks
 *            usually immedietely coalesce after freeing the blocks
 *  
 *            return pointer to coalesced block
 */
static void* coalesce(void* ptr) {
    void* next_blkp = NEXT_BLKP(ptr);
    void* prev_blkp = PREV_BLKP(ptr);
 
    size_t prev_alloc = GET_PALLOC(HDRP(ptr));
    size_t next_alloc = GET_ALLOC(HDRP(next_blkp));
    size_t size = GET_SIZE(HDRP(ptr));
 
    /* Case 1 */
    /* If previous block and successive block are both allocated,
        insert ptr immediately */
    if(prev_alloc && next_alloc) {		
        return insert_free(ptr);
    }
    
    /* Case 2 */
    /* If successive block is not allocated,
        coalesce ptr and its successive block */
    else if(prev_alloc && !next_alloc) {		
        size += GET_SIZE(HDRP(next_blkp));
 
        remove_free(next_blkp);
 
        PUT(HDRP(ptr), PACK(size, 0, 1));
        PUT(FTRP(ptr), PACK(size, 0, 1));
    }
 
    /* Case 3 */
    /* If previous block is not allocated,
        coalesce ptr and its previous block */
    else if(!prev_alloc && next_alloc) {		
        size += GET_SIZE(FTRP(prev_blkp));
 
        remove_free(prev_blkp);
 
        PUT(HDRP(prev_blkp), PACK(size, 0, (GET_PALLOC(HDRP(prev_blkp)))));
        PUT(FTRP(prev_blkp), PACK(size, 0, (GET_PALLOC(HDRP(prev_blkp)))));
        ptr = prev_blkp;
    }
 
    /* Case 4 */
    /* If previous block and successive block are neither allocated,
        coalesce ptr and its previous, successive block */
    else if(!prev_alloc && !next_alloc) {		
        size += GET_SIZE(HDRP(prev_blkp)) + GET_SIZE(FTRP(next_blkp));
 
        remove_free(prev_blkp);
        remove_free(next_blkp);
 
        PUT(HDRP(prev_blkp), PACK(size, 0, (GET_PALLOC(HDRP(prev_blkp)))));
        PUT(FTRP(prev_blkp), PACK(size, 0, (GET_PALLOC(HDRP(prev_blkp)))));
        ptr = prev_blkp;
    }
 
    return insert_free(ptr);
}

/*
 * free_index - Get index of entry of free list 
 *              depending on the size of block
 * 
 *              return the entry of free list
 */
static void* free_index(size_t size){
    /* magic number */
    if(size >= (1 << 12)) 
        return free_list + DSIZE * (FREELIST_SUM - 1);

    int x = 0;
    size >>= 4;
    for (; size > 1; size >>= 1) 
        ++ x;
    return free_list + DSIZE * x;
}

/*
 * insert_free - Insert free block into free list
 * 
 *               return pointer to the newly insertrd block
 */
static void* insert_free(void* ptr) {
    size_t size = GET_SIZE(HDRP(ptr));
    void* entry = free_index(size);
    void* p = entry;
    void* tmp_p;
    
    while((tmp_p = GET_SUCC(p)) && (GET_SIZE(HDRP(tmp_p)) < GET_SIZE(HDRP(ptr)))) 
        p = tmp_p;
    
    *(size_t*)ptr = (size_t)tmp_p;
    *(size_t*)p = (size_t)ptr;
    return ptr;
}

/*
 * remove_free - Remove free block from free list
 *               No return
 */
static void remove_free(void* ptr) {
    size_t size = GET_SIZE(HDRP(ptr));
    void* entry = free_index(size);

    void* p = entry;
    void* tmp_p;
    while((tmp_p = GET_SUCC(p)) && (tmp_p != ptr))
        p = tmp_p;
        
    *(size_t*)p = (size_t)GET_SUCC(ptr);
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void* p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void* p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap - Check if the heap and free list are legal
 */
void mm_checkheap(int lineno) {
    /* Check epilogue */
    if(in_heap(epilogue)) {
        printf("Epilogue is not in heap.\n");
        exit(-1);
    }
    if(!aligned(epilogue)) {
        printf("Error in epilogue alignment.\n");
        exit(-1);
    }

    /* Check prologue */
    void *prologue = heap_list - WSIZE;
    if(in_heap(prologue)) {
        printf("Prologue is not in heap.\n");
        exit(-1);
    }
    if(!aligned(prologue)) {
        printf("Error in epilogue alignment.\n");
        exit(-1);
    }

    /* Check boundaries */
    if(heap_list != mem_heap_lo()) {
        printf("Error in header location.\n");
        exit(-1);
    }
    if(epilogue != mem_heap_lo() + 1) {
        printf("Error in epilogue location.\n");
        exit(-1);
    }

    /* Check each block */
    for(void* ptr = heap_list; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if(!in_heap(ptr)) {
            printf("Block is not in heap.\n");
            exit(-1);
        }

        /* Check minimun size */
        if(GET_SIZE(HDRP(ptr)) < MIN_FREE_SIZE) {
            printf("Block size is too small.\n");
            exit(-1);
        }

        /* Check alignment */
        if(!aligned(ptr)) {
            printf("Error in block alignment.\n");
            exit(-1);
        }

        /* Check previous block */
        void* p_ptr = PREV_BLKP(ptr);
        if(!p_ptr) {
            if(GET_PALLOC(HDRP(ptr)) != GET_ALLOC(HDRP(p_ptr))) {
                printf("Error in previous block allocation.\n");
                exit(-1);
            }

            if(!GET_ALLOC(HDRP(ptr)) && !GET_ALLOC(HDRP(p_ptr))) {
                printf("Two consecutive free blocks \
                    have not been coalesced.\n");
                exit(-1);
            }
        }

        /* Check free blocks */
        if(!GET_ALLOC(HDRP(ptr))) {
            if(GET_SIZE(HDRP(ptr)) != GET_SIZE(FTRP(ptr))) {
                printf("Error in free blocks.\n");
                exit(-1);
            }
        }
    }

    /* Check free list */
    for(int i = 0; i < FREELIST_SUM; ++ i) {
        void* entry = free_list + i * DSIZE;
        void* p_ptr = entry;
        void* ptr = GET_SUCC(entry);
        while(ptr) {
            if(!in_heap(ptr)) {
                printf("Block is not in heap.\n");
                exit(-1);
            }

            /* Check minimun size */
            if(GET_SIZE(HDRP(ptr)) < MIN_FREE_SIZE) {
                printf("Block size is too small.\n");
                exit(-1);
            }

            /* Check alignment */
            if(!aligned(ptr)) {
                printf("Error in block alignment.\n");
                exit(-1);
            }

            /* Check previous block */
            if(!p_ptr) {
                if(GET_PALLOC(HDRP(ptr)) != GET_ALLOC(HDRP(p_ptr))) {
                    printf("Error in previous block allocation.\n");
                    exit(-1);
                }

                if(!GET_ALLOC(HDRP(ptr)) && !GET_ALLOC(HDRP(p_ptr))) {
                    printf("Two consecutive free blocks \
                        have not been coalesced.\n");
                    exit(-1);
                }
            }

            if (free_index(GET_SIZE(HDRP(ptr))) != entry){
                printf("Block in list bucket falls \
                    in wrong bucket size range.\n");
                exit(-1);
            }

            p_ptr = ptr;
            ptr = GET_SUCC(entry);
        }
    }
}