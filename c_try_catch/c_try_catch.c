#include "c_try_catch.h"
#include <stdlib.h>
#include <stdio.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

typedef struct HandlerRegistry_t {
    int type_identifier;
    jmp_buf *env;

    struct HandlerRegistry_t *next;
} HandlerRegistry_t;

typedef struct RegionRegistry_t {
    void *region_identifier;

    void *try_start;
    void *try_end;
    HandlerRegistry_t sentinel_handlers;

    void *data;

    struct RegionRegistry_t *next;
} RegionRegistry_t;

static struct {
    RegionRegistry_t sentinel;
} registered_regions;

static void *current_region_identifier;

static RegionRegistry_t *allocateRegionRegistry() {
    RegionRegistry_t *t = calloc(sizeof(RegionRegistry_t), 1);
    if (t) {
        return t;
    }
    fprintf(stderr, "allocate RegionRegistry_t failed\n");
    exit(1);
}

static HandlerRegistry_t *allocateHandlerRegistry() {
    HandlerRegistry_t *c = malloc(sizeof(HandlerRegistry_t));
    if (c) {
        return c;
    }
    fprintf(stderr, "allocate HandlerRegistry_t failed\n");
    exit(1);
}

static void freeRegionRegistry(RegionRegistry_t *t) {
    if (!t->sentinel_handlers.next) {
        free(t);
        return;
    }

    HandlerRegistry_t *curr = t->sentinel_handlers.next;
    while (curr) {
        HandlerRegistry_t *next = curr->next;
        free(curr->env);
        free(curr);
        curr = next;
    }
}

static void pushRegionRegistry(RegionRegistry_t *t) {
    t->next = registered_regions.sentinel.next;
    registered_regions.sentinel.next = t;
}

static void pushHandlerRegistry(RegionRegistry_t *t, HandlerRegistry_t *c) {
    HandlerRegistry_t *curr = &t->sentinel_handlers;
    while (curr->next) {
        curr = curr->next;
    }
    curr->next = c;
}

static RegionRegistry_t *searchRegionRegistryByIP(void *ip, RegionRegistry_t *prev) {
    ip = ip - 1;
    RegionRegistry_t *curr;
    if (!prev) {
         curr = registered_regions.sentinel.next;
    } else {
        curr = prev;
    }

    // printf("[search try registry for ip %p]\n", ip);
    while (curr) {
        if (ip >= curr->try_start && ip <= curr->try_end && curr != prev) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static RegionRegistry_t *searchRegionRegistryByIdentifier(void *region_identifier) {
    RegionRegistry_t *curr = registered_regions.sentinel.next;
    while (curr) {
        if (curr->region_identifier == region_identifier) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static HandlerRegistry_t *searchHandlerRegistry(const RegionRegistry_t *t, int type_identifier) {
    HandlerRegistry_t *c = t->sentinel_handlers.next;
    // printf("[search handler in %p for type %d]\n", t->region_identifier, type_identifier);
    while (c) {
        if (c->type_identifier == type_identifier) {
            return c;
        }
        c = c->next;
    }
    return NULL;
}


void *register_try(void *region_identifier, void *try_start, void *try_end) {
    RegionRegistry_t *r = allocateRegionRegistry();
    r->region_identifier = region_identifier;
    r->try_start = try_start;
    r->try_end = try_end;
    pushRegionRegistry(r);
    // printf("[register try %p]\n", region_identifier);
    return NULL;
}

void register_catch(void *region_identifier, jmp_buf *env, int type_identifier) {
    if (!env) {
        fprintf(stderr, "allocated catch env failed\n");
        exit(1);
    }
    RegionRegistry_t *t = searchRegionRegistryByIdentifier(region_identifier);
    if (t) {
        HandlerRegistry_t *c = allocateHandlerRegistry();
        c->type_identifier = type_identifier;
        c->env = env;
        pushHandlerRegistry(t, c);
        // printf("[register catch type %d for %p]\n", type_identifier, region_identifier);
        return;
    }
    fprintf(stderr, "try to register catch for unknown try %p\n", region_identifier);
    exit(1);
}

void unregister_region(void *region_identifier) {
    RegionRegistry_t *curr = registered_regions.sentinel.next;
    RegionRegistry_t *prev = &registered_regions.sentinel;
    while (curr) {
        if (curr->region_identifier == region_identifier) {
            prev->next = curr->next;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    if (curr) {
        freeRegionRegistry(curr);
        // // printf("[unregister %p]\n", region_identifier);
        if (current_region_identifier && current_region_identifier != region_identifier) {
            fprintf(stderr, "unexpected exception context %p\n", current_region_identifier);
            exit(1);
        }
        current_region_identifier = NULL;
    } else {
        fprintf(stderr, "unregister unknown region %p\n", region_identifier);
        exit(1);
    }
}

void throw_exception(int type_identifier, void *data) {
    if (current_region_identifier) {
        unregister_region(current_region_identifier);
        current_region_identifier = NULL;
    }
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip;
    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    
    RegionRegistry_t *t = NULL;
    HandlerRegistry_t *c;
    while (unw_step(&cursor) > 0) {

        // get return address(in caller's region) and frame address of caller
        unw_get_reg(&cursor, UNW_REG_IP, &ip);

        // found registered handler, they might be nested, so we need to do multiple times
        while (1) {
            if ((t = searchRegionRegistryByIP((void *)ip, t)) != NULL) {
                if ((c = searchHandlerRegistry(t, type_identifier)) != NULL) {
                    t->data = data;
                    current_region_identifier = t->region_identifier;
                    longjmp(*c->env, 0);
                }
                continue;
            }
            break;
        }
    }
    fprintf(stderr, "Uncaught exception %d\n", type_identifier);
    exit(1);
}

void *get_exception_data(void *region_identifier) {
    RegionRegistry_t *t = searchRegionRegistryByIdentifier(region_identifier);
    if (t) {
        return t->data;
    }
    return NULL;
}