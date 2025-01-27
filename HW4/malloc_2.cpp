#include <unistd.h> // for sbrk
#include <stdio.h>  // for stat print

#define NO_SIZE 0
#define TOO_MUCH_SIZE 100000000 // 10^8

typedef struct MallocMetaData { //header stuff
    size_t size;
    bool is_free;
    MallocMetaData *next;
    MallocMetaData *prev;
}MetaData;
#define MMD_SIZE sizeof(MetaData)

/*
----------------------------------------------------------------------------------------
                                    BlockList
 * first search
 * no splitting of blocks inside the list
 * block list should be sorted
----------------------------------------------------------------------------------------
*/
class BlockList {
private:
    MetaData* blocks;

    size_t num_free_blocks;
    size_t num_free_bytes;
    size_t num_allocated_blocks;
    size_t num_allocated_bytes;

public:
    BlockList() : blocks(NULL), num_free_blocks(0), num_free_bytes(0), num_allocated_blocks(0), num_allocated_bytes(0) {}

    MetaData* getMetaData(void* p) {
        return (MetaData*)((char*)p - MMD_SIZE);
    }

    // the heap grows towards the higher addrsses so adding from the last member should suffice in keeping it sorted
    void addBlock(MetaData* block, MetaData* prev) {       // add allocated block to list
        if(prev == NULL)                                     // if list empty set it to be the block
            blocks = block;
        else {
            prev->next = block;                              // else - set the prevs next to and the blocks next to each other
            block->prev = prev;                              // thats called friendship
        }
    }

    void freeBlock(void* block) {                           // free allocated block
        MetaData* p = getMetaData(block);
        if (p->is_free == true)                            // if already free i don't care
            return;
        
        //add to statistics
        this->num_free_blocks += 1;
        this->num_free_bytes += p->size;

        p->is_free = true;
    }

    void* allocateBlock(size_t size) {                       // allocate block using sbrk
        MetaData *current = blocks, *prev = NULL;

        while(current){                                      // try to find a free block in the list
            if(current->is_free
               && size <= current->size){                    // like in atam but readable
                current->is_free = false;
                
                //add to statistics
                this->num_free_blocks -= 1;
                this->num_free_bytes -= current->size;

                return current;                              // return found block
            }
            prev = current;
            current = current->next;
        }

        size_t alloc_size = size + MMD_SIZE;                 // new size = size asked + metadata size
        void* prog_break = sbrk(alloc_size);                 // did not find free block - time to make a new one
        if(prog_break == (void*)-1)
            return NULL;

        MetaData* new_block = (MetaData*)prog_break;
        new_block->size = size;
        new_block->is_free = false;
        new_block->next = NULL;
        new_block->prev = NULL;

        addBlock(new_block, prev);                           // prev can be NULL but addBlock makes sure we don't reach prev if list is empty
        
        //add to statistics
        this->num_allocated_blocks += 1;
        this->num_allocated_bytes += size;

        return prog_break;                                   // return the brand new spanking block
    }

    size_t numFreeBlocks() { return num_free_blocks; }
    size_t numFreeBytes() { return num_free_bytes; }
    size_t numAllocatedBlocks() { return num_allocated_blocks; }
    size_t numAllocatedBytes() { return num_allocated_bytes; }

    void printHeapStatistics() {
        printf("Heap Statistics:\n");
        printf("Total Allocated Blocks: %zu\n", num_allocated_blocks);
        printf("Total Allocated Bytes: %zu\n",  num_allocated_bytes);
        printf("Free Blocks: %zu\n",            num_free_blocks);
        printf("Free Bytes: %zu\n",             num_free_bytes);
        printf("Metadata Bytes: %zu\n",         num_free_blocks * MMD_SIZE);
        printf("Size of Metadata Block: %zu\n", MMD_SIZE);
    }
};

/*
----------------------------------------------------------------------------------------
                                IMPLEMENTATION
 * first search
 * no splitting of blocks inside the list
 * block list should be sorted
----------------------------------------------------------------------------------------
*/
BlockList block_list = BlockList();

/*
 * Searches for a free block with at least ‘size’ bytes or allocates (sbrk()) one if none are found. 
 * Return value: 
 *  i.	Success – returns pointer to the first byte in the allocated block (excluding the meta-data of course) 
 *  ii.	Failure –  
 *      a.	If size is 0 returns NULL. 
 *      b.	If ‘size’ is more than 108, return NULL. 
 *      c.	If sbrk fails in allocating the needed space, return NULL.  
 */
void* smalloc(size_t size) {
    if (size == NO_SIZE || size > TOO_MUCH_SIZE)              // if size is 0 or bigger than 10^8
        return NULL;
    void* new_block = block_list.allocateBlock(size);
    if (new_block == NULL)
        return NULL;
    return (char*)new_block + MMD_SIZE;                      // return the allocated block with the address pointing to the memory asked for
                                                             // |--MetaData--|--Memory--|
                                                             //  ------------^ we're here
}

/*
 * Searches for a free block of at least ‘num’ elements, each ‘size’ bytes that are all
 * set to 0 or allocates if none are found. In other words, find/allocate size * num bytes and set all bytes to 0. 
 * Return value: 
 *  i.	Success - returns pointer to the first byte in the allocated block. 
 *  ii.	Failure –  
 *      a.	If size or num is 0 returns NULL. 
 *      b.	If ‘size * num’ is more than 108, return NULL. 
 *      c.	If sbrk fails in allocating the needed space, return NULL. 
 */
void* scalloc(size_t num, size_t size) {
    if (num == NO_SIZE || num > TOO_MUCH_SIZE || size == NO_SIZE || size > TOO_MUCH_SIZE)
        return NULL;
    size_t total_size = size * num;
    void* new_block = smalloc(total_size);                   // using our lovely assistant smalloc we get a brand new spanking free block
    if (new_block == NULL)
        return NULL;
    memset(new_block, 0, total_size);                        // initialize memory to zero bc we callocing here
    return new_block;                                        // return the block (already starts from memory bc of smalloc)
                                                             // |--MetaData--|--Memory--|
                                                             //  ------------^ we're here
}

/*
 * Releases the usage of the block that starts with the pointer ‘p’.  
 * If ‘p’ is NULL or already released, simply returns.  
 * Presume that all pointers 'p' truly points to the beginning of an allocated block. 
 */
void sfree(void* p) {
    if(p == NULL)                           // freeBlock doesn't do shit anyway so no need to check if already free idc
        return;
    block_list.freeBlock(p);                //easy peasy lemon squeezy
}

/*
 * If ‘size’ is smaller than or equal to the current block’s size, reuses the same block.
 * Otherwise, finds/allocates ‘size’ bytes for a new space, copies content of oldp into the new allocated space and frees the oldp. 
 * Return value:  
 * i.	Success –  
 *   a.	Returns pointer to the first byte in the (newly) allocated space.  
 *   b.	If ‘oldp’ is NULL, allocates space for ‘size’ bytes and returns a pointer to it. 
 * ii. Failure –  
 *   a.	If size is 0 returns NULL. 
 *   b.	If ‘size’ if more than 108, return NULL. 
 *   c.	If sbrk fails in allocating the needed space, return NULL.  
 *   d.	Do not free ‘oldp’ if srealloc() fails. 
 */
void* srealloc(void* oldp, size_t size) {
    if (size == NO_SIZE || size > TOO_MUCH_SIZE)                  // if size is 0 or bigger than 10^8
        return NULL;
    else if (oldp == NULL)
        return smalloc(size);
    size_t old_size = block_list.getMetaData(oldp)->size;
    if(size <= old_size)                                          // if new size < old size - no need to allocate again
        return oldp;

    void* new_block = smalloc(size);                              // using our lovely assistant smalloc we get a brand new spanking free block
    if (new_block == NULL)
        return NULL;                                              // or maybe not
         /* dst,       src,  size*/
    memmove(new_block, oldp, old_size);                           // according to google it never fails so i don't check nothing
    sfree(oldp);   
    return new_block;
}

/*
----------------------------------------------------------------------------------------
                                    STATISTICS
 * need to set helper functions
----------------------------------------------------------------------------------------
*/

/* 
 * returns the number of allocated blocks in the heap that are
 * currently free.
 */
size_t _num_free_blocks() {
    return block_list.numFreeBlocks();
}

/* 
 * returns the number of bytes in all allocated blocks in the heap that
 * are currently free, excluding the bytes used by the meta-data structs
 */
size_t _num_free_bytes() {
    return block_list.numFreeBytes();
}

/* 
 * returns the overall (free and used) number of allocated blocks in
 * the heap
 */
size_t _num_allocated_blocks() {
    return block_list.numAllocatedBlocks();
}

/* 
 * returns the overall number (free and used) of allocated bytes in
 * the heap, excluding the bytes used by the meta-data structs
 */
size_t _num_allocated_bytes() {
    return block_list.numAllocatedBytes();
}

/* 
 * returns the overall number of meta-data bytes currently in the heap
 */
size_t _num_meta_data_bytes() {
    return block_list.numAllocatedBlocks() * MMD_SIZE;
}

/* 
 * returns the number of bytes of a single meta-data
 * structure in your system
 */
size_t _size_meta_data() { 
    return MMD_SIZE;
}