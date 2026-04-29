# Code readability

To increase readability in the code there are some proposed norms to guide the reader.

## Functions

- local_ is used to require that all threads in a local group are going to call it at some time.

otherwise, function does not require any type of synchronization.

## Pointers

- g_ is used to reference a pointer to global memory buffer.
- a_ is used to reference a pointer to a global memory value, often used for atomics.
- c_ is used to reference a pointer to constant memory.
- l_ is used to reference a pointer to local memory.
- lk_ is used to reference a pointer to local memory, where k limits the reach dimisions.

otherwise, pointer is expected to be private for each thread.

