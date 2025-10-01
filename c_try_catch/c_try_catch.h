#ifndef c_try_catch_h
#define c_try_catch_h

#include <setjmp.h>
#include <stdlib.h>

// exception type identifier
typedef int ExceptionType_t;

// no exception happened
#define ExceptionNone (-1)

typedef void (*CleanFunc_t) (void *);

#ifdef __OPTIMIZE__

#define try(group_index, try_block)   \
{   \
_region_identifier ## group_index:    \
    jmp_buf *_env ## group_index = malloc(sizeof(jmp_buf));    \
    extern void register_try(void *region_identifier, void *try_start, void *try_end, jmp_buf *env);    \
    register_try(&&_region_identifier ## group_index, &&_try_start ## group_index, &&_try_end ## group_index, _env ## group_index);    \
    goto _catch_init ## group_index; \
_try_start ## group_index:    \
    void __attribute((noinline)) _bf ## group_index() {   \
        try_block   \
    }   \
    _bf ## group_index();   \
_try_end ## group_index:  \
    goto _finally ## group_index; \
_catch_init ## group_index:    \
    int _stage_catch ## group_index = setjmp(*_env ## group_index);

#else

#define try(group_index, try_block)   \
{   \
_region_identifier ## group_index:    \
    jmp_buf *_env ## group_index = malloc(sizeof(jmp_buf));    \
    extern void register_try(void *region_identifier, void *try_start, void *try_end, jmp_buf *env);    \
    register_try(&&_region_identifier ## group_index, &&_try_start ## group_index, &&_try_end ## group_index, _env ## group_index);    \
    goto _catch_init ## group_index; \
_try_start ## group_index:    \
    {   \
        try_block   \
    }   \
_try_end ## group_index:  \
    goto _finally ## group_index;   \
_catch_init ## group_index:    \
    int _stage_catch ## group_index = setjmp(*_env ## group_index);

#endif


#define catch(group_index, type, data, catch_block) \
    if (!_stage_catch ## group_index) {  \
        extern void register_catch(void *region_identifier, ExceptionType_t type_identifier);   \
        register_catch(&&_region_identifier ## group_index, type);  \
    } else {    \
        ExceptionType_t _t;  \
        void *data; \
        extern void get_exception_info(const void *region_identifier, ExceptionType_t *type_, void **data);  \
        get_exception_info(&&_region_identifier ## group_index, &_t, &data); \
        if (_t == type) {   \
            {   \
                catch_block \
            }   \
            goto _finally ## group_index;   \
        }   \
    }

#define finally(group_index)  \
    goto _try_start ## group_index;   \
_finally ## group_index:  \
}   \

#define throw(type_, data_)   \
{   \
    extern void throw_exception(ExceptionType_t type_identifier, void *data);  \
    throw_exception(type_, data_); \
}

// register a clean func, which will be called with given arg
// this is used when the following code might throw an exception and interrupt expected clean up code routine
// clean func will be executed only once after exception occurred, then it will be unregistered
// the return value is used as the identifier to unregister it
void *register_clean_func(CleanFunc_t func, void *arg);

// unregister clean func
// if the following code do not throw exception and the program enters expected clean up code routine, you shall
// unregister all previously registered clean function
void unregister_clean_func(const void *identifier);

#endif