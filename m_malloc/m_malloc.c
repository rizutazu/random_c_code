#include "m_malloc.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

// #define m_malloc_debug

// minimal memory chunk size
#define MIN_CHUNK_SIZE 16

// minimal size available to user, and the alignment 
#define MIN_USER_SIZE 8

// roundup 
#define userSizeRoundUp(x) ( (x + MIN_USER_SIZE - 1) & ~(MIN_USER_SIZE - 1) )

// mmap page size unit
#define PAGE_SIZE 4096

// round up to page size
#define pageSizeRoundUp(x) ( (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1) )
#define pageSizeRoundDown(x) ( (x) & ~(PAGE_SIZE - 1) )

#define isPageAligned(x) (((x) & (PAGE_SIZE - 1)) == 0)

// free memory size threshold, if exceed => give back to system 
#define FREE_SIZE_THRESHOLD (PAGE_SIZE * 25)    // 100 KB 

// allocated indicator
#define CHUNK_ALLOCATED 1

// chunk header 
typedef struct ChunkHeader_t {
    // the size of this chunk
    // since chunk size alignment, the last 3 bits are not used for size, instead, for indicating allocation status
    // if chunk is allocated, the CHUNK_ALLOCATED bit is set
    size_t size;
    
    // next chunk of memory, if this chunk is allocated, this field contains user data.
    struct ChunkHeader_t *next;
} ChunkHeader_t;

// make sure chunk header alignment
static_assert(sizeof(ChunkHeader_t) == MIN_CHUNK_SIZE);
static_assert(offsetof(ChunkHeader_t, next) == sizeof(size_t));

// free memory chunks, a linked list
// the bucket is always sorted by address
static ChunkHeader_t *bucket;

// mmap address upperbound, for continuous memory area
static void *upperBound;

// insert a new chunk into memory chunk list
// the size field must not contain CHUNK_ALLOCATED bit 
static void insertChunk(ChunkHeader_t **bucket, ChunkHeader_t *new) {
    ChunkHeader_t *curr, *next;

    next = *bucket;

    // []
    // the list is empty
    if (!next) {
        *bucket = new;
        new->next = NULL;
        return;
    }

    // [|x x x ...]
    // list is not empty: check if insert before first element
    if (new < next) {
        // continuous, merge
        if ((size_t)new + new->size == (size_t)next) {
            new->next = next->next;
            new->size += next->size;
            *bucket = new;
        } else {    // not merge
            new->next = next;
            *bucket = new;
        }
        return;
    }

    // [x x | x ...]
    // list is not empty
    curr = *bucket;
    while (curr) {
        next = curr->next;

        // [x x ... x|]
        // next is NULL: list has reached the end, check if merge last element
        if (next == NULL) {
            goto merge_last;
        }

        // [x x | x ...]
        // next is not NULL: check whether merge curr or next
        if ((size_t)curr < (size_t)new 
                && (size_t)new < (size_t)next) {
            goto merge_middle;
        }
        curr = curr->next;
    }

merge_middle:
    
    // after the new is merged into curr/next, check whether 
    // curr and next forms continuous memory. 
    int check_curr_next = 1;

    // merge curr 
    if ((size_t)curr + curr->size == (size_t)new) {
        curr->size += new->size;

    // merge next
    } else if ((size_t)new + new->size == (size_t)next) {
        new->next = next->next;
        new->size += next->size;
        curr->next = new;
    
    // insert only
    } else {
        new->next = next;
        curr->next = new;
        check_curr_next = 0;
    }

    if (check_curr_next) {
        if ((size_t)curr + curr->size == (size_t)next) {
            curr->next = next->next;
            curr->size += next->size;
        }
    }
    return;

merge_last:
    // merge into curr
    // curr + curr.size == new: continuous memory, merge
    if ((size_t)curr + curr->size == (size_t)new) {
        curr->size += new->size;
    } else {    // not continuous: add to linked list
        curr->next = new;
        new->next = NULL;
    }
    return;
    
}

// return sum of size of all free chunks
static size_t getBucketTotalSize(ChunkHeader_t **bucket) {
    size_t r = 0;
    ChunkHeader_t *curr = *bucket;
    while (curr) {
        r += curr->size;
        curr = curr->next;
    }
    return r;
}

// ask system for more memory
static ChunkHeader_t *moreCore(size_t n) {
    n = pageSizeRoundUp(n);
    if (!upperBound) {
        upperBound = sbrk(0);
    }
    ChunkHeader_t *chunk = (ChunkHeader_t *)mmap(upperBound, n, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (chunk) {
        chunk->next = NULL;
        chunk->size = n;

        #ifdef m_malloc_debug
        printf("moreCore: %p [%d]\n", chunk, n);
        #endif

        // update upper bound
        size_t u = (size_t)chunk + chunk->size;
        if (u > (size_t)upperBound) {
            upperBound = (void *)u;
        }
        
    }
    return chunk;
}

// give back memory to system
static void lessCore(ChunkHeader_t **bucket) {
    ChunkHeader_t *curr,*prev = NULL;

    #ifdef m_malloc_debug
    int nothing = 1;
    #endif
    
    curr = *bucket;
    while (curr) {

        // start address is aligned && chunk can be split, or chunk is page-aligned size
        size_t rounded_size = pageSizeRoundDown(curr->size);
        if (isPageAligned((size_t)curr) 
                && ((curr->size >= rounded_size + MIN_CHUNK_SIZE) || curr->size == rounded_size)
        ) {

            #ifdef m_malloc_debug
            printf("lessCore: chunk condition match %p --> %p [%d]\n", curr, (size_t)curr + curr->size, curr->size);
            nothing = 0;
            #endif

            ChunkHeader_t *temp;

            // optimization?: if unmap size is too big (e.g. single huge free chunk), only free half
            size_t give_back_size = rounded_size;
            if (give_back_size >= FREE_SIZE_THRESHOLD) {
                give_back_size = pageSizeRoundDown(give_back_size / 2);
            }
                 
            // page size align: give back the whole chunk
            if (give_back_size == curr->size) {
                
                // update bucket
                if (prev) {
                    prev->next = curr->next;
                } else {
                    *bucket = curr->next;
                }

                temp = curr->next;

                #ifdef m_malloc_debug
                printf("lessCore: %p [%d]\n", curr, give_back_size);
                #endif

                if (munmap(curr, give_back_size)) {
                    printf("munmap: %p [%d] failed\n", curr, give_back_size);
                }

                // curr++, prev unchanged
                curr = temp;

            // not align: split and give back
            } else {
                ChunkHeader_t *newCurr = (ChunkHeader_t *)((size_t)curr + give_back_size);
                newCurr->size = curr->size - give_back_size;
                newCurr->next = curr->next;
                if (prev) {
                    prev->next = newCurr;
                } else {
                    *bucket = newCurr;
                }

                #ifdef m_malloc_debug
                printf("lessCore: %p [%d]\n", curr, give_back_size);
                #endif

                if (munmap(curr, give_back_size)) {
                    printf("munmap: %p [%d] failed\n", curr, give_back_size);
                }

                // curr++, prev++
                prev = newCurr;
                curr = newCurr->next;
            }

            if (getBucketTotalSize(bucket) < FREE_SIZE_THRESHOLD) {
                break;
            }
        } else {
            // curr++, prev++
            prev = curr;
            curr = curr->next;
        }
    }

    #ifdef m_malloc_debug
    if (nothing) {
        printf("lessCore: nothing unmapped\n");
    }
    #endif

}

// find chunk that satisfies given size n, will try to split and remove it from the bucket
// do not set CHUNK_ALLOCATED bit
static ChunkHeader_t *findFirstFit(ChunkHeader_t **bucket, size_t n) {
    ChunkHeader_t *curr = *bucket, *prev = NULL;

    while (curr) {
        // found fit chunk
        if (curr->size >= n) {

            // the size are exactly the same, or cannot split into smaller one 
            if (curr->size - n < MIN_CHUNK_SIZE) {
                // remove chunk: not first element
                if (prev) {
                    prev->next = curr->next;
                } else {    
                // remove chunk: is the first element
                    *bucket = curr->next;
                }
                return curr;
            
            // split the chunk
            } else {
                // first half is the part that returns
                ChunkHeader_t *newCurr = (ChunkHeader_t *)((size_t)curr + n);
                newCurr->size = curr->size - n;
                newCurr->next = curr->next;
                if (prev) {
                    prev->next = newCurr;
                } else {
                    *bucket = newCurr;
                }

                curr->size = n;
                return curr;
            }
        }

        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

#ifdef m_malloc_debug
static void printBucket(ChunkHeader_t **bucket) {
    ChunkHeader_t *curr = *bucket;
    if (!curr) {
        printf("  = None bucket =\n");
        return;
    }
    printf("  = bucket =\n");
    while (curr) {
        printf("    %p --> %p [%d]\n", curr, (size_t)curr + curr->size, curr->size);
        curr = curr->next;
    }
    printf("  = End bucket =\n");
}
#endif

void *m_malloc(size_t n_user) {
    if (n_user == 0) {
        n_user = MIN_USER_SIZE;
    } else {
        n_user = userSizeRoundUp(n_user);
    }

    // n_user is user requested memory size, it does not equals to n, which defined as chunk size
    // because of chunk header size  
    size_t n = n_user + offsetof(ChunkHeader_t, next);

    ChunkHeader_t *c;
    
    #ifdef m_malloc_debug
    printf("==> Start malloc user req %d\n", n_user);
    printf("before find first fit: \n");
    printBucket(&bucket);
    #endif

    c = findFirstFit(&bucket, n);
    if (!c) {

        #ifdef m_malloc_debug
        printf("no first fit, morecore(%d)\n", n);
        #endif

        insertChunk(&bucket, moreCore(n));

        #ifdef m_malloc_debug
        printf("after add morecore:\n");
        printBucket(&bucket);
        #endif

        c = findFirstFit(&bucket, n);
    }

    // if this branch failed it must be mmap failed, or code bug
    if (c) {

        #ifdef m_malloc_debug
        printf("take first fit: %p -> %p [%d]\n", c, (size_t)c + c->size, c->size);
        printf("after take first fit:\n");
        printBucket(&bucket);
        #endif

        c->size |= CHUNK_ALLOCATED;
    } else {
        return NULL;
    }

    #ifdef m_malloc_debug
    printf("<== End malloc\n");
    #endif

    return (void *)&c->next;
}

void m_free(void *ptr) {
    if (!ptr) {
        return;
    }

    #ifdef m_malloc_debug
    printf("==> Start free user ptr %p\n", ptr);
    #endif

    ChunkHeader_t *c = (ChunkHeader_t *)(ptr - offsetof(ChunkHeader_t, next));
    if (c->size & CHUNK_ALLOCATED) {
        c->size = c->size & (~CHUNK_ALLOCATED);

        #ifdef m_malloc_debug
        printf("insert chunk: %p -> %p [%d]\n", c, (size_t)c + c->size, c->size);
        printf("before insert\n");
        printBucket(&bucket);
        #endif

        insertChunk(&bucket, c);

        #ifdef m_malloc_debug
        printf("after insert\n");
        printBucket(&bucket);
        #endif

        if (getBucketTotalSize(&bucket) >= FREE_SIZE_THRESHOLD) {
            
            #ifdef m_malloc_debug
            printf("free memory size exceed threshold\n");
            #endif

            lessCore(&bucket);

            #ifdef m_malloc_debug
            printf("after lessCore\n");
            printBucket(&bucket);
            #endif
        }

    } else {
        printf("Error: %p: not allocated memory\n", c);
    }

    #ifdef m_malloc_debug
    printf("<== End free\n");
    #endif
}
