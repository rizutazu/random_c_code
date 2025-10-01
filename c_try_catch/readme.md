# c_try_catch: a simple sjij-based try/catch style exception handling implementation

## Features
- throw exception with user-defined exception type, also do catch block
- support nested try-catch block
- register a clean-up function in case the exception interrupted your clean-up routine

## Basic usage

### Overview: 

```c
try(0, {
    printf("i'm going to throw something!\n");
    throw(ExceptionType2, "hello");
}) catch(0, ExceptionType1, data, {
    printf("catch exception 1 %s\n", (char *)data);
}) catch(0, ExceptionType2, data, {
    printf("catch exception 2 %s\n", (char *)data);
}) finally(0)
```

You should use EXACTLY one `try` block at start, followed by one or more `catch` block in the middle, and EXACTLY one `finally` block at
the end. `finally` block is a must.

You may find out number `0` is the first argument of all three kind of blocks, this number is so-called **"group index"** , a **positive
integer** that used to bind `try`/`catch`/`finally` blocks together as a single unit, to support nested try-catch blocks.

User can select their own "group index" number as they want. **Notice that different try-catch blocks in the same function 
should have different "group index". Blocks of different functions are allowed, however.**

Ok:
```c
void func1() {
    try(0, {
        // ...
    }) catch(0, ExceptionType1, data, {
        // ...
    }) finally(0);
    
    try(1, {
        // ...
    }) catch(1, ExceptionType1, data, {
        // ...
    }) finally(1);
    
    try(2, {
        try(3, {
            // ...
        }) catch(3, ExceptionType1, data, {
            // ...
        }) finally(3);
    }) catch(2, ExceptionType1, data, {
        // ...
    }) finally(2);
}
void func2() {
    try(0, {
        // ...
    }) catch(0, ExceptionType1, data, {
        // ...
    }) finally(0);
}
```

Bad:
```c
void func1() {
    try(0, {
        // ...
    }) catch(0, ExceptionType1, data, {
        // ...
    }) finally(0);
    
    try(0, {
        // ...
    }) catch(0, ExceptionType1, data, {
        // ...
    }) finally(0);
}
void func2() {
    try(0, {
        try(0, {
            // ...
        }) catch(0, ExceptionType1, data, {
            // ...
        }) finally(0);
    }) catch(0, ExceptionType1, data, {
        // ...
    }) finally(0);
}
```


### `throw`: raise the exception

`throw(type, data)`
- `type` is a variable of type `ExceptionType_t`, currently it is an alias of `int` 
- you can define your own type by assigning different values, and catch different types in different catch blocks (see syntax below)
- `data` is a void pointer, you can pass some data to your catch block, notice that **do not use stack-based pointers** 

### `try` block syntax

`try(group_index, {try_block_code})`, pretty straight forward

### `catch` block syntax

`catch(group_index, type, data, {catch_block_code})`
- `type` is a variable of type `ExceptionType_t`, you can catch your intended type by specifying it
- `data` is the variable name of the thrown void pointer, feel free to use other names, just like variable name `e` in `catch(Exception e)`

### `finally` block syntax

`finally(group_index)`, that's it

### register a clean-up function

Example: 

```c
void func1() {
     // the target you want to clean
    int *ptr = malloc(sizeof(int));
    
    // you are going to call func2(), which might thow exception and interrupt your clean-up routine
    // the first argument is the clean-up function, and the second one is its argument when calling it
    // the return value can be used for unregistering it
    void *identifier = register_clean_func(free, ptr)
    
    // might throw exception!
    // if exception happened, free(ptr) will be executed
    func2();
    
    // however, if exception did not happen, you should unregister it manually
    unregister_clean_func(identifier);
    
    // and do your intended clean-up routine
    free(ptr);
}
```

## Examples

- `main`: simple examples
- `recursive`: exception handling in recursive function 

## Problems

- Compiler reliance: this implementation heavily relies on gcc's `labels as values` and `nested function` extension, other compilers might not work
- No `return` statement in `try` block: as it is implemented over `nested function` 
- No `goto` statement in `try` block: for reasons i don't know, the compiler keeps complaining it, even though labels are declared to be visible in `nested function`
- Your code analyzer might complain about lacking a `;`, because they do not treat `nested function` as valid syntax: 
  - `int func1() { int nested_func() {return 42;} }   <== 😱😱😱 Oh my god a syntax error!!!`
  - `int func1() { int nested_func(); {return 42;} }   <== 👍👍👍 Valid code, pass`