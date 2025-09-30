#include "c_try_catch.h"
#include <stdlib.h>
#include <stdio.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

// --- data structure declarations ---

// Registry for a catch block, must associate with a try block
typedef struct HandlerRegistry_t {
    // which type shall this catch block handle?
    ExceptionType_t type_identifier;

    // linked list
    struct HandlerRegistry_t *next;
} HandlerRegistry_t;

// Registry for a try block
typedef struct RegionRegistry_t {
    // a value that identifies this try block, usually it is the address of associated register_try() func call
    // it is unique regardless of nested try blocks
    void *region_identifier;

    // start & end address of this try block, obtained by label
    void *try_start;
    void *try_end;

    // associated registered catch handlers, handlers must unique for each catch type
    HandlerRegistry_t sentinel_handlers;

    // entry point of associated catch blocks
    jmp_buf *env;

    // if accessed properly by get_exception_data(), this two fields store thrown exception info
    ExceptionType_t type;
    void *data;

    // linked list
    struct RegionRegistry_t *next;
} RegionRegistry_t;

// Registry for a user specified clean func
typedef struct CleanFuncRegistry_t {
    // for identifying this registry
    void *identifier;

    // caller's return address, used to determine call frame
    void *ip;

    // user specified clean func and its arg
    CleanFunc_t func;
    void *arg;

    // linked list
    struct CleanFuncRegistry_t *next;
} CleanFuncRegistry_t;

// registered try regions, only removed and freed on exit
static struct {
    RegionRegistry_t sentinel;
} registered_regions;

// registered clean func, removed and freed after executing clean func
struct {
    CleanFuncRegistry_t sentinel;
} registered_clean;

// --- data structure manipulation functions ---

// allocate a zero-inited RegionRegistry_t, exit on failure
static RegionRegistry_t *allocateRegionRegistry() {
    RegionRegistry_t *t = calloc(sizeof(RegionRegistry_t), 1);
    if (t) {
        return t;
    }
    fprintf(stderr, "allocate RegionRegistry_t failed\n");
    exit(1);
}

// allocate a zero-inited HandlerRegistry_t, exit on failure
static HandlerRegistry_t *allocateHandlerRegistry() {
    HandlerRegistry_t *c = calloc(sizeof(HandlerRegistry_t), 1);
    if (c) {
        return c;
    }
    fprintf(stderr, "allocate HandlerRegistry_t failed\n");
    exit(1);
}

// allocate a zero-inited CleanFuncRegistry_t, exit on failure
static CleanFuncRegistry_t *allocateCleanFuncRegistry() {
    CleanFuncRegistry_t *c = calloc(sizeof(CleanFuncRegistry_t), 1);
    if (c) {
        return c;
    }
    fprintf(stderr, "allocate CleanFuncRegistry_t failed\n");
    exit(1);
}

// free memory owned by a HandlerRegistry_t
static void freeHandlerRegistry(HandlerRegistry_t *h) {
    free(h);
}

// free memory owned by a RegionRegistry_t, including its associated HandlerRegistry_t
static void freeRegionRegistry(RegionRegistry_t *t) {
    if (!t->sentinel_handlers.next) {
        free(t->env);
        free(t);
        return;
    }

    HandlerRegistry_t *curr = t->sentinel_handlers.next;
    while (curr) {
        HandlerRegistry_t *next = curr->next;
        freeHandlerRegistry(curr);
        curr = next;
    }

    free(t->env);
    free(t);
}

// free memory owned by a CleanFuncRegistry_t
static void freeCleanFuncRegistry(CleanFuncRegistry_t *c) {
    free(c);
}

// push a RegionRegistry_t to global list, newly pushed ones are always first elements
// do not check duplicate elements
static void pushRegionRegistry(RegionRegistry_t *t) {
    t->next = registered_regions.sentinel.next;
    registered_regions.sentinel.next = t;
}

// push a HandlerRegistry_t to given RegionRegistry_t, newly pushed ones are always first elements
// do not check duplicate elements
static void pushHandlerRegistry(RegionRegistry_t *t, HandlerRegistry_t *c) {
    c->next = t->sentinel_handlers.next;
    t->sentinel_handlers.next = c;
}

// search a RegionRegistry that its try block can cover given ip
// header are used to specify starting point: for nested try block, there might be multiple regions satisfying it.
// we use the staring point to make sure each region is searched/returned only once
// if header is NULL, search from scratch
// if remove is not zero, will dequeue found registry, if not NULL.
static RegionRegistry_t *searchRegionRegistryByIP(void *ip, RegionRegistry_t *header, int remove) {
    // ip might point to the NEXT instruction to execute, it might exceed try block if it is the last line of code:
    // --- try start ---
    // ...
    // ...
    // throw(...)
    // --- try end ---
    // nothing related to try       <-- ip points here
    ip = ip - 1;

    // setup iteration starting point
    RegionRegistry_t *curr, *prev;
    if (!header) {
        prev = &registered_regions.sentinel;
        curr = registered_regions.sentinel.next;
    } else {
        prev = header;
        curr = header->next;
    }
   
// #ifdef c_try_catch_debug
//     printf("[search try registry for ip %p, prev_ptr=%p, remove=%d]\n", ip, header, remove);
// #endif

    // find satisfied registry
    while (curr) {
        if (ip >= curr->try_start && ip < curr->try_end) {
            if (remove) {
                prev->next = curr->next;
            }
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

// search a RegionRegistry by identifier
// if remove is not zero, will dequeue found registry, if not NULL.
static RegionRegistry_t *searchRegionRegistryByIdentifier(const void *region_identifier, int remove) {
    RegionRegistry_t *curr = registered_regions.sentinel.next, *prev = &registered_regions.sentinel;

    while (curr) {
        if (curr->region_identifier == region_identifier) {
            if (remove) {
                prev->next = curr->next;
            }
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

// search a HandlerRegistry in given RegionRegistry by given type identifier
// since the handler is unique for each type identifier, the return is unique
// if remove is not zero, will dequeue found registry, if not NULL.
static HandlerRegistry_t *searchHandlerRegistry(RegionRegistry_t *t, ExceptionType_t type_identifier, int remove) {
    
// #ifdef c_try_catch_debug
//     printf("[search handler in %p for type %d, remove=%d]\n", t->region_identifier, type_identifier, remove);
// #endif

    HandlerRegistry_t *curr = t->sentinel_handlers.next, *prev = &t->sentinel_handlers;
    while (curr) {
        if (curr->type_identifier == type_identifier) {
            if (remove) {
                prev->next = curr->next;
            }
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

// push a CleanFuncRegistry_t to global list, newly pushed ones are always first elements
void pushCleanFuncRegistry(CleanFuncRegistry_t *c) {
    c->next = registered_clean.sentinel.next;
    registered_clean.sentinel.next = c;
}

// search a CleanFuncRegistry, if its caller's ip lie in given start_ip ~ end_ip address range
// note that multiple satisfied registries might exist, it is better to do while loop with remove=1 to get them all
CleanFuncRegistry_t *searchCleanFuncRegistryByIPRange(const void *start_ip, const void *end_ip, int remove) {
    CleanFuncRegistry_t *curr = registered_clean.sentinel.next, *prev = &registered_clean.sentinel;
    while (curr) {
        if (curr->ip >= start_ip && curr->ip < end_ip) {
            if (remove) {
                prev->next = curr->next;
            }
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

// --- functions exposed(?) to user ---

// register a try region which has address range try_start ~ try_end, with given region_identifier
// the region_identifier is defined and obtained in try macro, user shall not define it by themselves
void register_try(void *region_identifier, void *try_start, void *try_end, jmp_buf *env) {
    // check memory allocation failure
    if (!env) {
        fprintf(stderr, "allocated catch env failed\n");
        exit(1);
    }

    RegionRegistry_t *r;

    // check if registry with given region_identifier exists, if so, update env and push it to the first 
    if ((r = searchRegionRegistryByIdentifier(region_identifier, 1))) {

#ifdef c_try_catch_debug
        printf("[duplicate register try %p, remove old env]\n", region_identifier);
#endif

        free(r->env);
        r->env = env;
        pushRegionRegistry(r);
        return;
    }

#ifdef c_try_catch_debug
    printf("[register try %p: %p ~ %p]\n", region_identifier, try_start, try_end);
#endif

    // allocate new one, fill fields and push into global list
    r = allocateRegionRegistry();
    r->region_identifier = region_identifier;
    r->try_start = try_start;
    r->try_end = try_end;
    r->env = env;
    pushRegionRegistry(r);
}

// register a catch region which handles given type, it will be associated with the try region that has given
// region_identifier
// if the try region with region_identifier has already registered handler of given type, nothing is done
void register_catch(void *region_identifier, ExceptionType_t type_identifier) {
    // search for associated try region
    RegionRegistry_t *t = searchRegionRegistryByIdentifier(region_identifier, 0);
    if (t) {
        // check if duplicate, if so, do nothing
        if (searchHandlerRegistry(t, type_identifier, 0)) {

#ifdef c_try_catch_debug
            printf("[duplicate register catch type %d for %p]\n", type_identifier, region_identifier);
#endif

            return;
        }

#ifdef c_try_catch_debug
        printf("[register catch type %d for %p]\n", type_identifier, region_identifier);
#endif

        // allocate memory for new one && push list
        HandlerRegistry_t *c = allocateHandlerRegistry();
        c->type_identifier = type_identifier;
        pushHandlerRegistry(t, c);
        return;
    }
    // not found, error
    fprintf(stderr, "try to register catch for unknown try %p\n", region_identifier);
    exit(1);
}

// void unregister_region(void *region_identifier) {
//     RegionRegistry_t *r = searchRegionRegistryByIdentifier(region_identifier, 1);
//
//     if (r) {
//         freeRegionRegistry(r);
//         // printf("[unregister %p]\n", region_identifier);
//     } else {
//         fprintf(stderr, "unregister unknown region %p\n", region_identifier);
//         exit(1);
//     }
// }

// throw an exception to given type with data pointer
// this function will unwind the stack:
// 1. check if registered clean function exists, if so, do clean and unregister it.
// 2. then, check if there is handler can handle it, if so, jump into its catch block
// if handler is not found, the program will terminate
void throw_exception(ExceptionType_t type_identifier, void *data) {

#ifdef c_try_catch_debug
    printf("[throw type %d]\n", type_identifier);
#endif

    unw_cursor_t cursor;
    unw_context_t uc;

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    
    RegionRegistry_t *r = NULL;

    // step the call frame
    while (unw_step(&cursor) > 0) {
        unw_word_t ip;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);

        unw_proc_info_t info;
        unw_get_proc_info(&cursor, &info);
        void *start_ip = (void *)info.start_ip;
        void *end_ip = (void *)info.end_ip;
        
#ifdef c_try_catch_debug
        printf("[unwind frame of ip %p]\n", (void *)ip);
#endif

        // first, find and execute registered clean func
        // there might be multiple registries, so
        while (1) {
            CleanFuncRegistry_t *c = searchCleanFuncRegistryByIPRange(start_ip, end_ip, 1);
            if (c) {

#ifdef c_try_catch_debug
                printf("[do clean for %p]\n", c->identifier);
#endif

                c->func(c->arg);
                free(c);
            } else {
                break;
            }
        }

        // then, use return address of the frame to determine if it is in registered try block
        while (1) {
            // find satisfied try block, it might be nested, so we need to do multiple times
            // since newly registered try block is always push to the first, and the search starts from the first, this
            // guarantees inner try block is the first to be return
            if ((r = searchRegionRegistryByIP((void *)ip, r, 0)) != NULL) {

#ifdef c_try_catch_debug
                printf("[found registered try block %p]\n", r->region_identifier);
#endif

                // try block found, find catch block
                if (searchHandlerRegistry(r, type_identifier, 0)) {

#ifdef c_try_catch_debug
                    printf("[found type %d handler in %p]\n", type_identifier, r->region_identifier);
                    printf("[set exception info in %p: type %d, data %p]\n", r->region_identifier, type_identifier, data);
#endif

                    // catch block found, setup for catch block getting thrown data
                    r->type = type_identifier;
                    r->data = data;
                    // jump into catch block, do not return
                    longjmp(*r->env, 0);
                }
                // satisfied catch block not found, search next satisfied try block
                continue;
            }
            // no more satisfied try block in current frame
            break;
        }
    }
    fprintf(stderr, "Uncaught exception %d\n", type_identifier);
    exit(1);
}

// get thrown exception info if in catch block, the result is unknown if not called properly
void get_exception_info(const void *region_identifier, ExceptionType_t *type, void **data) {
    RegionRegistry_t *t = searchRegionRegistryByIdentifier(region_identifier, 0);
    if (t) {

#ifdef c_try_catch_debug
        printf("[read exception info in %p: type %d, data %p]\n", t->region_identifier, t->type, t->data);
#endif

        *type = t->type;
        *data = t->data;
        return;
    }
    fprintf(stderr, "try to read info of unknown region_identifier %p\n", region_identifier);
    exit(1);
}

// register a clean func, which will be called with given arg
// this is used when the following code might throw an exception and interrupt expected clean up code routine
// clean func will be executed only once after exception occurred, then it will be unregistered
// the return value is used as the identifier to unregister it
void *register_clean_func(CleanFunc_t func, void *arg) {
    CleanFuncRegistry_t *c = allocateCleanFuncRegistry();
    c->func = func;
    c->arg = arg;
    c->ip = __builtin_return_address(0);
    c->identifier = c;
    pushCleanFuncRegistry(c);
    return c;
}

// unregister clean func
// if the following code do not throw exception and the program enters expected clean up code routine, you shall
// unregister all previously registered clean function
void unregister_clean_func(const void *identifier) {
    CleanFuncRegistry_t *curr = registered_clean.sentinel.next, *prev = &registered_clean.sentinel;
    while (curr) {
        if (curr->identifier == identifier) {
            prev->next = curr->next;
            freeCleanFuncRegistry(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

// --- destructor ---

// free everything on exit
__attribute__((destructor))
void cleanUpALL() {
    RegionRegistry_t *curr = registered_regions.sentinel.next;
    while (curr) {
        RegionRegistry_t *next = curr->next;
        freeRegionRegistry(curr);
        curr = next;
    }

    CleanFuncRegistry_t *c = registered_clean.sentinel.next;
    while (c) {
        CleanFuncRegistry_t *next = c->next;
        c->func(c->arg);
        freeCleanFuncRegistry(c);
        c = next;
    }
}