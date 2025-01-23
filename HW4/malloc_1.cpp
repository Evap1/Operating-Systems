#include <unistd.h> // for sbrk

#define NO_SIZE 0
#define TOO_MUCH_SIZE 100000000 // 10^8

/*
 * Tries to allocate ‘size’ bytes.
 * Return value: 
 *  i.	Success – a pointer to the first allocated byte within the allocated block. 
 *  ii.	Failure –  
 *      a.	If size is 0 returns NULL. 
 *      b.	If size is more than 10^8, return NULL. 
 *      c.	If sbrk fails in allocating the needed space, return NULL.  
 */
void* smalloc(size_t size) {
    if (size == NO_SIZE || size > TOO_MUCH_SIZE)        // if size is 0 or bigger than 10^8
        return NULL;
    void* prog_break = sbrk(size);                      // request memory from the system
    if (prog_break == (void*)-1)
        return NULL;                                    // no more memory/target address is bad/process out of heap memory
    return prog_break;
}