#include <unistd.h>   // for sbrk
#include <sys/mman.h> // for mmap
#include <string.h>   // for memmove and memset
#include <stdint.h>   // for uintptr_t 

#define NO_SIZE 0
#define TOO_MUCH_SIZE 100000000         // 10^8
#define MAX_ORDER 10                    // for array of sbrk
#define MAX_BLOCK_SIZE 128*1024         // 128kB = 2^17B
#define INIT_NUM_BLOCKS 32
#define BLOCK_UNIT 128
#define ALLIGNMEMT_FACTOR 32*128*1024

typedef struct MallocMetaData {         //header stuff
    size_t size;
    bool is_free;
    MallocMetaData *next;
    MallocMetaData *prev;
}MetaData;
#define MMD_SIZE sizeof(MetaData)       //its 32 btw if anyones interested

/*
----------------------------------------------------------------------------------------
                                    BlockList
 * first search
 * splitting - block buddies system
 * allocations should be only upon first malloc request and only then!
 * size of block in sbrk_blocks - order 0:  2^0 *  128 = 128 ( -metadata)
 *                                order 1:  2^1 *  128 = 256
 *                                ..
 *                                order 10: 2^10 * 128 = 128kb
 *  - meaning we should devide by 128 first and then handle the order
----------------------------------------------------------------------------------------
*/
class BlockList {
private:
    MetaData* sbrk_blocks[MAX_ORDER + 1];
    MetaData* mmap_blocks;

    bool      is_first_malloc;
    size_t    number_used_blocks_sbrk;
    size_t    bytes_used_blocks_sbrk;

public:
    BlockList() : mmap_blocks(NULL), is_first_malloc(true), number_used_blocks_sbrk(0), bytes_used_blocks_sbrk(0) {
        for (int i = 0; i <= MAX_ORDER; i ++) {
            sbrk_blocks[i] = NULL;
        }
    }

    bool isFirstMalloc() { return this->is_first_malloc; }

    MetaData* getMetaData(void* p) {
        return (MetaData*)((char*)p - MMD_SIZE);
    }

    int blockOrder(size_t size) {                                           // devide by 128, then shift right (devide by 2 iteratively) until zero
        size_t size_with_metadata = (MMD_SIZE + size) / BLOCK_UNIT;         // without the 128 unit
        int order = 0;
        while(size_with_metadata > 1)
        {
            size_with_metadata >>= 1;
            order++;
        }
        return order;
    }

    /* sbrk implementation */
    
    void addBlock_Sbrk(MetaData* block) {                                   // add allocated block to list
        int block_order = blockOrder(block->size);
        if (sbrk_blocks[block_order] == NULL) {                             // no block in list
            sbrk_blocks[block_order] = block;
            return;
        }

        MetaData* current = sbrk_blocks[block_order], *prev = NULL;         // we try to find space in the list
        while (current != NULL && current < block) {
            prev = current;
            current = current->next;
        }

        if (prev == NULL) {                                                 // first in list
            sbrk_blocks[block_order]->prev = block;
            block->next = sbrk_blocks[block_order];
            sbrk_blocks[block_order] = block;
        }
        else if (current == NULL) {                                         // last in list
            prev->next = block;
            block->prev = prev;
        }
        else {                                                              // oh no we in the middle
            prev->next = block;                                             // prev->next | prev<-block->next | prev<-current
            current->prev = block;
            block->prev = prev;
            block->next = current;
        }
    }

    bool allignmentForHeap() {                                              // allignment inside heap so we'll have pretty alignment
        void* pointer_break = sbrk(0);
        intptr_t address = (intptr_t)pointer_break;
        intptr_t alligned_address = (address + ALLIGNMEMT_FACTOR - 1) & ~(ALLIGNMEMT_FACTOR - 1);
        if(sbrk(alligned_address - address) == (void*)-1) {
            return false;                                                   // can't handle what comes next - which is a horror show
        }
        return true;
    }

    void firstAlloc_Sbrk() {
        is_first_malloc = false;
        if(!allignmentForHeap())                                            // aw the memory
            return;
        
        size_t alloc_size = MAX_BLOCK_SIZE;
        size_t data_size = alloc_size - MMD_SIZE;
        for (int i = 0; i < INIT_NUM_BLOCKS; i++) {
            void* p_break = sbrk(alloc_size);                               // sbrk brk (only 32 times besides the allignment)
            if(p_break == (void*)-1)
                return;
            MetaData* new_block = (MetaData*)p_break;                       // give it some pezaz
            new_block->size = data_size;
            new_block->is_free = true;
            new_block->next = NULL;
            new_block->prev = NULL;
            addBlock_Sbrk(new_block);
        }
    }

    void removeFromList_Sbrk(MetaData* block) {
        int order = blockOrder(block->size);
        if (block->prev == NULL) {                                          // first in list
            sbrk_blocks[order] = sbrk_blocks[order]->next;
            if (sbrk_blocks[order])                                         // to whomever forgot to remind me i need to NULL the start of the list when removing - :(
                sbrk_blocks[order]->prev = NULL;
            block->next = NULL;
        }
        else if (block->next == NULL) {                                     // last in list
            (block->prev)->next = NULL;
            block->prev = NULL;
        }
        else {                                                              // in the middle
            (block->prev)->next = block->next;
            (block->next)->prev = block->prev;
            block->prev = NULL;
            block->next = NULL;
        }
    }

    void splitBlock_Sbrk(MetaData* block) {
        size_t new_total_size = (block->size + MMD_SIZE) / 2;               // whole size of block / 2 (bc now we need 2)
        size_t new_mem_size = new_total_size - MMD_SIZE;                    // we dont need the metadata size

        MetaData* buddy_block = (MetaData*)((char*)block + new_total_size); // | block metadata | new_mem_size | buddy metadata | new_mem_size |
        buddy_block->size = new_mem_size;                                   // ^---------total_size-----------^ shifted to start of buddy
        buddy_block->is_free = true;
        buddy_block->next = NULL;
        buddy_block->prev = NULL;

        block->size = new_mem_size;
        addBlock_Sbrk(buddy_block);                                         // return the buddy to the list bc he's a free spirit
    }

    void checkIfNeedToSplit_Sbrk(MetaData* block, size_t size) {            // checks if we need to split
        size_t half_block_size = ((block->size + MMD_SIZE) / 2);            // is half the size still bigger or equal to the size we need?
        while ((half_block_size >= size + MMD_SIZE) && (half_block_size >= BLOCK_UNIT)) {
            splitBlock_Sbrk(block);                                         // do the splits
            half_block_size = (block->size + MMD_SIZE) / 2;
        }
    }

    size_t tryToMerge_Sbrk(MetaData** block, size_t size) {
        //stats
        number_used_blocks_sbrk -= 1;
        bytes_used_blocks_sbrk -= (*block)->size;

        while ((*block)->size < size) {                                     // supposed to always be true when merging on freeBlock
            MetaData* buddy = findBuddy_Sbrk(*block);                       // upon realloc only merge until size is enough or condition not met
            if (buddy->is_free == false || buddy->size != (*block)->size ||
                (*block)->size >= MAX_BLOCK_SIZE - MMD_SIZE) {
                break;                                                      // stop merging if buddy isn't free, sizes don't match or size too big than the maximun allowed
            }
            removeFromList_Sbrk(buddy);                                     // we don't need him again in the list

            if ((uintptr_t)buddy < (uintptr_t)(*block)) {                   // determine the lower address of the two
                *block = buddy;                                             // merge at lower address
            }
            (*block)->size = (((*block)->size + MMD_SIZE) * 2) - MMD_SIZE;  // set the size to be 2 times than it was and minus the metadata
        }
        return (*block)->size;
    }

    void incrementUsed_Sbrk(MetaData* block) {
        //stats
        number_used_blocks_sbrk += 1;
        bytes_used_blocks_sbrk += block->size;
    }

    MetaData* findPotentialBlock_Sbrk(size_t size) {
        int block_order = blockOrder(size);
        for (int i = block_order; i <= MAX_ORDER; i++) {
            MetaData* current = sbrk_blocks[i];
            while (current != NULL) {
                if (current->size >= size) {                                // dont need to check if free bc this is a free country (and also a free list <3)
                    removeFromList_Sbrk(current);
                    return current;
                }
                current = current->next;
            }
        }
        return NULL;                                                        // aw no block too sad oh well ba bye
    }

    MetaData* findBuddy_Sbrk(MetaData* block) {
        return (MetaData*)((uintptr_t)block ^ (block->size + MMD_SIZE));    // XOR trick from pdf (using the address of the current block and the total size)
    }

    void freeBlock_Sbrk(void* block) {                                      // free allocated block
        MetaData* p = getMetaData(block);
        if (p->is_free == true)                                             // if already free i don't care
            return;
        
        //removeFromInUse_Sbrk(p);
        if(p->size >= MAX_BLOCK_SIZE - MMD_SIZE)                            // the size is bigger than max size - leave it be (too big to merge a ba bye)
        {
            p->is_free = true;
            addBlock_Sbrk(p);

            //stats
            number_used_blocks_sbrk -= 1;
            bytes_used_blocks_sbrk -= p->size;
            return;
        }
        tryToMerge_Sbrk(&p, TOO_MUCH_SIZE);                                 // giving it "too much size" to make the while condition inside true

        p->is_free = true;                                                  // fINALLY
        addBlock_Sbrk(p);
    }

    void* allocateBlock_Sbrk(size_t size) {                                 // allocate block using sbrk
        MetaData* potential_block = findPotentialBlock_Sbrk(size);
        if (potential_block == NULL)                                        // theres no big enough block srry babe
            return NULL;
        
        checkIfNeedToSplit_Sbrk(potential_block, size);                     // there is?!?!?!? is it too big though - also takes down the size
        potential_block->is_free = false;                                   // in prison again

        //stats
        number_used_blocks_sbrk += 1;
        bytes_used_blocks_sbrk += potential_block->size;
        return potential_block;
    }
    
    /* mmap implementation */
    void addBlock_Mmap(MetaData* block) {                                   // add allocated mmap block to list
        if (mmap_blocks == NULL)
            mmap_blocks = block;
        else {                                                              // no need to keep mmap list organised
            mmap_blocks->prev = block;                                      // block->next | prev<-mmap_blocks
            block->next = mmap_blocks;
            mmap_blocks = block;
        }
    }

    void removeBlock_Mmap(MetaData* block) {                                // this is basically the removal of sbrk block from the list but make it mmap
        if (block->prev == NULL) {                                          // first in list
            mmap_blocks = mmap_blocks->next;
            if (mmap_blocks)
                mmap_blocks->prev = NULL;
            block->next = NULL;
        }
        else if (block->next == NULL) {                                     // last in list
            (block->prev)->next = NULL;
            block->prev = NULL;
        }
        else {                                                              // in the middle
            (block->prev)->next = block->next;
            (block->next)->prev = block->prev;
            block->prev = NULL;
            block->next = NULL;
        }
    }

    void freeBlock_Mmap(void* block) {
        MetaData* p = getMetaData(block);
        removeBlock_Mmap(p);
        munmap(p, p->size + MMD_SIZE);                                      // kill it with fire
    }

    void* allocateBlock_Mmap(size_t size) {                                 // mmaping up in the biznatch
        void* mmap_block = mmap(NULL, size + MMD_SIZE, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(mmap_block == MAP_FAILED)
            return NULL;                                                    // aw the huge size asked from the user

        MetaData* new_block = (MetaData*)mmap_block;                        // pretty straight forward
        new_block->size = size;
        new_block->is_free = false;
        new_block->next = NULL;
        new_block->prev = NULL;

        addBlock_Mmap(new_block);
        return mmap_block;
    }

    size_t numFreeBlock() {                                                 // should only run on the array of the sbrk, since we hold only free blocks there
        size_t counter = 0;
        for (int i = 0; i <= MAX_ORDER; i++) {
            MetaData* current = sbrk_blocks[i];
            while(current != NULL) {
                counter++;
                current = current->next;
            }
        }
        return counter;
    }
    
    size_t numFreeBytes() {                                                 // same as up func but this time we count size of free blocks 
        size_t free_size = 0;
        for (int i = 0; i <= MAX_ORDER; i++) {
            MetaData* current = sbrk_blocks[i];
            while(current != NULL) {
                free_size += current->size;
                current = current->next;
            }
        }
        return free_size;
    }
    
    size_t numAllocatedBlocks() {                                           // need to count the sbrk, mmap and the *not free* blocks
        size_t counter = 0;
        for (int i = 0; i <= MAX_ORDER; i++) {
            MetaData* current = sbrk_blocks[i];
            while(current != NULL) {
                counter++;
                current = current->next;
            }
        }

        MetaData* current = mmap_blocks;
        while(current != NULL) {
            counter++;
            current = current->next;
        }
        return counter + this->number_used_blocks_sbrk;
    }
    
    size_t numAllocatedBytes() {                                            // same but make it bytes
        size_t alloc_size = 0;
        for (int i = 0; i <= MAX_ORDER; i++) {
            MetaData* current = sbrk_blocks[i];
            while(current != NULL) {
                alloc_size += current->size;
                current = current->next;
            }
        }

        MetaData* current = mmap_blocks;
        while(current != NULL) {
            alloc_size += current->size;
            current = current->next;
        }
        return alloc_size + this->bytes_used_blocks_sbrk;
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
 * searches for a free block with at least ‘size’ bytes or allocates (mmap) if the size is too big. 
 * return value: 
 * i.	Success – returns pointer to the first byte in the allocated block (excluding the meta-data) 
 * ii.	Failure –  
 *      a.	If size is 0 returns NULL. 
 *      b.	If ‘size’ is more than 108, return NULL. 
 *      c.	If fails in allocating the needed space, return NULL.  
 */
void* smalloc(size_t size) {
    if (block_list.isFirstMalloc())                           // according to pdf - should allocate the first time malloc is called
            block_list.firstAlloc_Sbrk();

    if (size == NO_SIZE || size > TOO_MUCH_SIZE)              // if size is 0 or bigger than 10^8
        return NULL;

    void* new_block;
    if( size >= MAX_BLOCK_SIZE)                               // size too big for sbrk
        new_block = block_list.allocateBlock_Mmap(size);
    else                                                      // size if fine for sbrk
        new_block = block_list.allocateBlock_Sbrk(size);

    if (new_block == NULL)
        return NULL;
    return (char*)new_block + MMD_SIZE;                       // return the allocated block with the address pointing to the memory asked for
                                                              // |--MetaData--|--Memory--|
                                                              //  ------------^ we're here
}

/*
 * find/allocate size * num bytes and set all bytes to 0. 
 * Return value: 
 * i.	Success - returns pointer to the first byte in the allocated block. 
 * ii.	Failure –  
 *      a.	If size or num is 0 returns NULL. 
 *      b.	If ‘size * num’ is more than 108, return NULL. 
 *      c.	If fails in allocating the needed space, return NULL. 
 */
void* scalloc(size_t num, size_t size) {
    if (num == NO_SIZE || num > TOO_MUCH_SIZE || size == NO_SIZE || size > TOO_MUCH_SIZE)
        return NULL;
    size_t total_size = size * num;
    void* new_block = smalloc(total_size);                    // using our lovely assistant smalloc we get a brand new spanking free block
    if (new_block == NULL)
        return NULL;
    memset(new_block, 0, total_size);                         // initialize memory to zero bc we callocing here
    return new_block;                                         // return the block (already starts from memory bc of smalloc)
                                                              // |--MetaData--|--Memory--|
                                                              //  ------------^ we're here
}

/*
 * Releases the usage of the block that starts with the pointer ‘p’.  
 * If ‘p’ is NULL or already released, simply returns.  
 * Presume that all pointers 'p' truly points to the beginning of an allocated block.
 * if p is mmapped, unmaps it
 */
void sfree(void* p) {
    if(p == NULL)
        return;
    MetaData* block = block_list.getMetaData(p);
    if(block->is_free)
        return;                                                //why did you bring this to me its free
    
    if (block->size >= MAX_BLOCK_SIZE)
        block_list.freeBlock_Mmap(p);                          // a little bit less easy peasy but its easier than sbrk
    else
        block_list.freeBlock_Sbrk(p);                          //easy peasy lemon squeezy
}

/*
 * finds/allocates ‘size’ bytes for a new space, copies content of oldp into the new allocated space and frees the oldp. 
 * mmap - if equal then return the same. otherwise change
 * sbrk - if less than or equal then return the same. otherwise change
 * Return value:  
 * i.	Success –  
 *   a.	Returns pointer to the first byte in the (newly) allocated space.  
 *   b.	If ‘oldp’ is NULL, allocates space for ‘size’ bytes and returns a pointer to it. 
 * ii. Failure –  
 *   a.	If size is 0 returns NULL. 
 *   b.	If ‘size’ if more than 108, return NULL. 
 *   c.	If fails in allocating the needed space, return NULL.  
 *   d.	Do not free ‘oldp’ if srealloc() fails. 
 * 
 * can assume that they will not test cases where they will reallocate a normally allocated
 * block to be resized to a block (excluding the meta-data) that’s more than 128kb (meaning no sbrk and mmap correlation)
 */
void* srealloc(void* oldp, size_t size) {
    if (size == NO_SIZE || size > TOO_MUCH_SIZE)               // if size is 0 or bigger than 10^8
        return NULL;
    else if (oldp == NULL)
        return smalloc(size);

    MetaData* old_block = block_list.getMetaData(oldp);
    size_t old_size = old_block->size;

    // need to check which type of block is he
    if(old_block->size > MAX_BLOCK_SIZE) {                      // mmap handling
        if(size == old_size)                                    // if new size == old size - no need to allocate again
            return oldp;
        void *new_block = smalloc(size);
        if (new_block == NULL)
            return NULL;
        
        if (size <= old_block->size) {
            memmove(new_block, oldp, size);
        } else {
            memmove(new_block, oldp, old_block->size);
        }
        sfree(oldp);
        return new_block;
    }
    else {                                                      // sbrk handling - so much bigger than expected (tis what she proclaimed)
        if (size <= old_size) {                                 // no need to split according to pdf
            return oldp;
        }
        size_t size_after_merge = block_list.tryToMerge_Sbrk(&old_block, size);
        block_list.incrementUsed_Sbrk(old_block);
        void* address_of_data = (char*)old_block + MMD_SIZE;
        if (size_after_merge >= size) {
            old_block->is_free = false;
            memmove(address_of_data, oldp, old_size);           // oldp still holds a pointer to the content of the old malloc
            return address_of_data;                             // return the pointer after the metadata - oldp not supposed to be in the list
        }
                                                                // else - we couldn't merge a big enough slice - time to malloc a new ho
        void* new_block = smalloc(size);                        // using our lovely assistant smalloc we get a brand new spanking free block
        if (new_block == NULL)
            return NULL;                                        // or maybe not
            /* dst,       src,  size*/
        memmove(new_block, oldp, old_size);                     // according to google it never fails so i don't check nothing
        sfree(oldp);   
        return new_block;
    }
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
    return block_list.numFreeBlock();
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