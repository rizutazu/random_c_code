#ifndef c_try_catch_h
#define c_try_catch_h

#include <setjmp.h>
#include <stdlib.h>

#define try(region_id_, try_block_)   \
{   \
    extern void *register_try(void *region_identifier, void *try_start, void *try_end);    \
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
    extern void register_catch(void *region_identifier, jmp_buf *env, int type_identifier);    \
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
    extern void unregister_region(void *region_identifier);    \
    goto _try_start ## region_id_;   \
    _finally ## region_id_:  \
    unregister_region(&&_region_identifier ## region_id_);   \
}

#define throw(type_, data_)   \
{   \
    extern void throw_exception(int type_identifier, void *data);  \
    throw_exception(type_, data_); \
}


#endif