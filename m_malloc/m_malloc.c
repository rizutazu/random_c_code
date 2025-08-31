#include "m_malloc.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

// #define m_malloc_debug

// minimal memory chunk size
#define MIN_CHUNK_SIZE (sizeof(size_t) + sizeof(uintptr_t))

// minimal size available to user, and the alignment 
#define MIN_USER_SIZE (sizeof(uintptr_t))

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
    // since chunk size alignment, lower bits are not used for size, instead, for indicating allocation status
    // if chunk is allocated, the CHUNK_ALLOCATED bit is set
    size_t size;
    
    // next chunk of memory, if this chunk is allocated, this field contains user data.
    struct ChunkHeader_t *next;
} ChunkHeader_t;

// make sure chunk header alignment
static_assert(sizeof(ChunkHeader_t) == MIN_CHUNK_SIZE);
static_assert(offsetof(ChunkHeader_t, next) == sizeof(size_t));
static_assert(sizeof(uintptr_t) == sizeof(size_t));

// free memory chunks, a linked list
// the bucket is always sorted by address
static ChunkHeader_t *bucket;

// mmap address upperbound, for continuous memory area
static void *upperBound;

// insert a new chunk into memory chunk list
// the size field must not contain CHUNK_ALLOCATED bit 
static void insertChunk(ChunkHeader_t **bucket, ChunkHeader_t *new) {

    if (!new) {
        return;
    }
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
        if ((void *)new + new->size == (void *)next) {
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
        if ((void *)curr < (void *)new
                && (void *)new < (void *)next) {
            goto merge_middle;
        }
        curr = curr->next;
    }

    printf("bug: insertChunk goes out of loop\n");
    return;

merge_middle:
    
    // after the new is merged into curr/next, check whether 
    // curr and next forms continuous memory. 
    int check_curr_next = 1;

    // merge curr 
    if ((void *)curr + curr->size == (void *)new) {
        curr->size += new->size;

    // merge next
    } else if ((void *)new + new->size == (void *)next) {
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
        if ((void *)curr + curr->size == (void *)next) {
            curr->next = next->next;
            curr->size += next->size;
        }
    }
    return;

merge_last:
    // merge into curr
    // curr + curr.size == new: continuous memory, merge
    if ((void *)curr + curr->size == (void *)new) {
        curr->size += new->size;
    } else {    // not continuous: add to linked list
        curr->next = new;
        new->next = NULL;
    }
    
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
        printf("moreCore: %p [%lu]\n", chunk, n);
#endif

        // update upper bound
        void *u = (void *)chunk + chunk->size;
        if (u > upperBound) {
            upperBound = u;
        }
    } else {
        printf("moreCore: warning: got NULL ptr\n");
    }
    return chunk;
}

// give back memory to system
// this implementation may be buggy, as a chunk is freed iff at least one of its address boundary is page-aligned
static void lessCore(ChunkHeader_t **bucket) {
    ChunkHeader_t *curr,*prev = NULL;

#ifdef m_malloc_debug
    int nothing = 1;
#endif
    
    curr = *bucket;
    while (curr) {

        // page-aligned address boundary
        size_t palign_low = pageSizeRoundUp((uintptr_t)curr);
        size_t palign_high = pageSizeRoundDown((uintptr_t)((void *)curr + curr->size));

        // page-aligned size
        size_t palign_size = palign_high - palign_low;

        // can shrink at least 1 page-size memory
        if (palign_size) {

#ifdef m_malloc_debug
            printf("lessCore: chunk condition might match %p --> %p [%lu]\n", curr, (void *)curr + curr->size, curr->size);
            nothing = 0;
#endif

            size_t give_back_size = palign_size;

            // check whether give back size is too big (i.e, a huge chunk)
            // if so, only give back half
            if (give_back_size >= FREE_SIZE_THRESHOLD) {
                give_back_size = pageSizeRoundDown(give_back_size / 2);
            }

            // the free chunk is perfectly page-aligned
            if (isPageAligned((uintptr_t)curr) && isPageAligned((uintptr_t)((void *)curr + curr->size))) {

                if (give_back_size == palign_size) {
                    goto give_back_whole;

                // case of which the chunk is too big
                } else {
                    goto give_back_first_half;
                }
            }

            // only start address is aligned
            if (isPageAligned((uintptr_t)curr)) {

                // after give back, the remaining size must greater than MIN_CHUNK_SIZE
                if (curr->size - give_back_size >= MIN_CHUNK_SIZE) {
                    goto give_back_first_half;
                }

                // or, if we can give back less 1 page to obey the restriction
                if (give_back_size > PAGE_SIZE) {
                    give_back_size -= PAGE_SIZE;
                    goto give_back_first_half;
                }

                // failed
                prev = curr;
                curr = curr->next;
                continue;
            }

            // only end address is aligned
            if (isPageAligned((uintptr_t)((void *)curr + curr->size))) {

                if (curr->size - give_back_size >= MIN_CHUNK_SIZE) {
                    goto give_back_last_half;;
                }

                if (give_back_size > PAGE_SIZE) {
                    give_back_size -= PAGE_SIZE;
                    goto give_back_last_half;;
                }

                // failed
                prev = curr;
                curr = curr->next;
                continue;
            }

            // nothing aligned
            // todo:
            prev = curr;
            curr = curr->next;
            continue;


give_back_whole:
            {
                ChunkHeader_t *newCurr = curr->next;

                // update bucket
                if (prev) {
                    prev->next = newCurr;
                } else {
                    *bucket = newCurr;
                }

#ifdef m_malloc_debug
                printf("lessCore: %p [%lu]\n", curr, give_back_size);
#endif

                if (munmap(curr, give_back_size)) {
                    printf("munmap: %p [%lu] failed\n", curr, give_back_size);
                }

                curr = newCurr;
                goto end;
            }

give_back_first_half:
            {
                ChunkHeader_t *newCurr = (ChunkHeader_t *)((void *)curr + give_back_size);
                newCurr->size = curr->size - give_back_size;
                newCurr->next = curr->next;

                if (prev) {
                    prev->next = newCurr;
                } else {
                    *bucket = newCurr;
                }

#ifdef m_malloc_debug
                printf("lessCore: %p [%lu]\n", curr, give_back_size);
#endif

                if (munmap(curr, give_back_size)) {
                    printf("munmap: %p [%lu] failed\n", curr, give_back_size);
                }

                // curr++, prev++
                prev = newCurr;
                curr = newCurr->next;

                goto end;
            }

give_back_last_half:
            {
                ChunkHeader_t *free = (ChunkHeader_t *)((void *)curr + (curr->size - give_back_size));
                curr->size -= give_back_size;

#ifdef m_malloc_debug
                printf("lessCore: %p [%lu]\n", free, give_back_size);
#endif

                if (munmap(free, give_back_size)) {
                    printf("munmap: %p [%lu] failed\n", free, give_back_size);
                }

                prev = curr;
                curr = curr->next;
                goto end;
            }

end:
            nothing = 0;
            if (getBucketTotalSize(bucket) < FREE_SIZE_THRESHOLD / 2) {
                break;
            }

        } else {
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
                ChunkHeader_t *newCurr = (ChunkHeader_t *)((void *)curr + n);
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
        printf("    %p --> %p [%lu]\n", curr, (void *)curr + curr->size, curr->size);
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

    // n_user is user requested memory size, it does not equal to n, which defined as chunk size
    // because of chunk header size  
    size_t n = n_user + offsetof(ChunkHeader_t, next);

    ChunkHeader_t *c;
    
#ifdef m_malloc_debug
    printf("\n==> Start malloc user req %lu\n", n_user);
    printf("before find first fit: \n");
    printBucket(&bucket);
#endif

    c = findFirstFit(&bucket, n);
    if (!c) {

#ifdef m_malloc_debug
        printf("no first fit, moreCore(%lu)\n", n);
#endif

        insertChunk(&bucket, moreCore(n));

#ifdef m_malloc_debug
        printf("after add moreCore:\n");
        printBucket(&bucket);
#endif

        c = findFirstFit(&bucket, n);
    }

    // if this branch failed it must be mmap failed, or code bug
    if (c) {

#ifdef m_malloc_debug
        printf("take first fit: %p -> %p [%lu]\n", c, (void *)c + c->size, c->size);
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
    printf("\n==> Start free user ptr %p\n", ptr);
#endif

    ChunkHeader_t *c = (ChunkHeader_t *)(ptr - offsetof(ChunkHeader_t, next));
    if (c->size & CHUNK_ALLOCATED) {
        c->size = c->size & (~CHUNK_ALLOCATED);

#ifdef m_malloc_debug
        printf("insert chunk: %p -> %p [%lu]\n", c, (void *)c + c->size, c->size);
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
