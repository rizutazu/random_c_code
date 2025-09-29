#ifndef c_try_catch_h
#define c_try_catch_h

#include <setjmp.h>
#include <stdlib.h>

// exception type identifier
typedef int ExceptionType_t;

typedef void (*CleanFunc_t) (void *);

#define try(region_id_, try_block_)   \
{   \
    extern void register_try(void *region_identifier, void *try_start, void *try_end);    \
    _region_identifier ## region_id_:    \
    register_try(&&_region_identifier ## region_id_, &&_try_start ## region_id_, &&_try_end ## region_id_);    \
    goto _catch_start ## region_id_; \
    _try_start ## region_id_:    \
    {   \
        try_block_   \
    }   \
    _try_end ## region_id_:  \
    goto _finally ## region_id_; \
    _catch_start ## region_id_:  \
}

#define catch(region_id_, type_, data_, catch_block_) \
{   \
    extern void register_catch(void *region_identifier, jmp_buf *env, ExceptionType_t type_identifier);    \
    extern void *get_exception_data(void *region_identifier);  \
    jmp_buf *_env = malloc(sizeof(jmp_buf));    \
    register_catch(&&_region_identifier ## region_id_, _env, type_);  \
    if (setjmp(*_env)) {    \
        void *data_ = get_exception_data(&&_region_identifier ## region_id_); \
        {   \
            catch_block_ \
        }   \
        goto _finally ## region_id_; \
    }   \
}

#define finally(region_id_)  \
{   \
    goto _try_start ## region_id_;   \
    _finally ## region_id_:  \
}

#define throw(type_, data_)   \
{   \
    __attribute((noreturn)) extern void throw_exception(ExceptionType_t type_identifier, void *data);  \
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