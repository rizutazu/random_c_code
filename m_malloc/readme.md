# `m_malloc`: simple malloc implementation

## Notice
- 64 bit environment
- Not thread safe

## Ideas
- Use `mmap` to grab a bunch of memory from system.

- The minimal memory management unit is a *chunk*. Each chunk has a *chunk header* (`ChunkHeader_t`) at the beginning, which contains *chunk size*. 

- The chunk has a minimal size of at least the size of chunk header, so memory after the header may or may not belongs to the chunk.
  
- A chunk can be either free or not free (allocated to user), this is marked by low bits in chunk size field. The chunk size has alignment requirement, so it is safe to use them. 

- `ChunkHeader_t` contains pointer to the next free chunk.

- Maintain a global variable `bucket` to store all free chunks by utilizing pointer in the chunk header. The address of next chunk is always higher than current chunk.

- `m_malloc`: 

  - Grab **first free chunk** that has more chunk size than user requested in the `bucket` (`findFirstFit()`), either return the whole chunk or split it and return the first half. 

  - If `findFirstFit()` fails to find, ask system for more memory (`moreCore()`), and add it into `bucket` (`insertChunk()`), then try again.

  - Pointer in the `ChunkHeader_t` is no longer used as it is not a free chunk now, so it is used to store user data.

- `m_free`:

  - Mark chunk as free, add back to `bucket` (`insertChunk()`), merge nearby chunks if their memory is continuous.
  
  - If the sum of size of all free chunks exceed certain threshold, it will try to give some chunks back to the system (`lessCore()`).
