# Some random C code

All the code is written and tested in x64 linux environment. 

- `init_reg_print`: show up initial registers' value when reaching program's userspace entry point. 
- `m_malloc`: a simple `malloc` / `free` implementation 
- `m_thread`: a simple userspace preemptive thread implementation by utilizing `ucontext_t`.
- `c_try_catch`: a simple try/catch style exception handling implementation, supports type specification and nested try-catch.
