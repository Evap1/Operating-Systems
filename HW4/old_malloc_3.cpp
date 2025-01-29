#include <unistd.h>   // for sbrk
#include <sys/mman.h> // for mmap
#include <stdio.h>    // for stat print
#include <string.h>   // for memmove and memset
#include <stdint.h>   // for uintptr_t 

#define NO_SIZE 0
#define TOO_MUCH_SIZE 100000000 // 10^8
#define MAX_ORDER 10
#define MAX_SIZE_BLOCK 128*1024 // 128kB = 2^17B
#define INIT_NUM_BLOCKS 32
#define INIT_BLOCK_UNIT 128
#define ALLIGNMEMT_FACTOR 32*128*1024

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
 * splitting - block buddies system
 * allocations should be only upon first malloc request and only then!
----------------------------------------------------------------------------------------
*/
class BlockList {
private:
    MetaData* smols_list[MAX_ORDER + 1];     // sbrk
    MetaData* mmap_list;                     // mmap

    bool is_first_smalloc;
    size_t num_free_blocks;
    size_t num_free_bytes;
    size_t num_allocated_blocks;
    size_t num_allocated_bytes;

public:
    BlockList() : mmap_list(NULL), is_first_smalloc(true), num_free_blocks(0), num_free_bytes(0), num_allocated_blocks(0), num_allocated_bytes(0) {
        for(int i = 0; i < MAX_ORDER; i++) {                            //initialize the lists to NULL
            smols_list[i] = NULL;
        }
    }

    MetaData* getMetaData(void* p) {
        return (MetaData*)((char*)p - MMD_SIZE);
    }

    MetaData* calcXORForBuddies(void* address, size_t size)
        {
            uintptr_t first_reused = reinterpret_cast<uintptr_t>(address);
            uintptr_t second_reused = reinterpret_cast<uintptr_t>(size);
            uintptr_t result = first_reused ^ second_reused;                                        // XOR's between the addresses
            return (MetaData*)(result);
        }

    size_t calcOrderOfBlock(size_t size) {                      // need to check if i want to find the order while including the metadata
        size_t size_with_metadata = (MMD_SIZE + size) / INIT_BLOCK_UNIT;
        int order = 0;
        while(size_with_metadata > 1)
        {
            size_with_metadata >>= 1;
            order++;
        }
        return order;
    }

    size_t calcBlockSize(size_t index) {
        return 1 << index;
    }

    // the heap grows towards the higher addrsses so adding from the last member should suffice in keeping it sorted
    void addSbrkBlock(MetaData* block) {       // add allocated block to list
        size_t index = calcOrderOfBlock(block->size);
            
        if(smols_list[index] == NULL)                                     // if list empty set it to be the block
            smols_list[index] = block;
        else {                                                            // need to keep the list sorted lets go
            MetaData* current = smols_list[index], *prev = NULL;
            while(current != NULL) {
                if (block < current) {
                    prev = current;
                    break;
                }
                prev = current;
                current = current->next;
            }
            if(current == NULL) {                                   // end of list
                prev->next = block;
                block->prev = prev;
            }
            else if (prev == NULL){                           // start of list
                smols_list[index]->prev = block;
                block->next = prev;
                smols_list[index] = block;
            }
            else {                                                  // middle of list
                block->next = prev;
                block->prev = prev->prev;
                (block->prev)->next = block;
                prev->prev = block;
            }
        }
    }

    void splitBlock(MetaData* block)
    {
        size_t new_block_total_size = (block->size + MMD_SIZE) / 2;
        size_t new_mem_size = new_block_total_size - MMD_SIZE;
        
        MetaData* new_buddy_block = (MetaData*)((char*)block + new_block_total_size); // new free block
        new_buddy_block->size = new_mem_size;
        new_buddy_block->is_free = true;
        new_buddy_block->next = NULL;
        new_buddy_block->prev = NULL;

        block->size = new_mem_size;
        addSbrkBlock(new_buddy_block); // insert the free block back
    }

    MetaData* findBestBlockForSbrkAlloc(size_t size) {
        int order=0;
        size_t num=128;
        while(size + MMD_SIZE > num)                                    //just like calc order but we need the num too to calculate with MMD (maybe can merge)
        {
            num = num * 2;
            order++;
        }
        for(int i = order; i <= MAX_ORDER; i++) {
            MetaData* current = smols_list[i];
            while(current != NULL) {
                if(current->size >= size && current->is_free)
                    return current;
                current = current->next;
            }
        }
        return NULL;                                        // is this even possible lets find out
    }

    bool canMergeForRealloc(MetaData* p, size_t size) {
        size_t current_size = p->size;
        while(current_size < MAX_SIZE_BLOCK - MMD_SIZE)
        {
            MetaData* buddy = calcXORForBuddies(p, MMD_SIZE + current_size); // buddy
            if(buddy->is_free == false)
            {
                return false;
            }
            current_size = current_size +  MMD_SIZE + buddy->size;
            if(current_size >= size)                                         // we found a fitting size
            {
                return true;
            }
            if(p > buddy)
            {
                p = buddy;
            }
        }
        return false;
    }

    bool doItMerge(MetaData* p, size_t size) {
        
    }

    void addMmapBlock(MetaData* block) {                     // add allocated block to list
        if(mmap_list == NULL) {                                // if list empty set it to be the block
            mmap_list = block;
            return;
        }
        mmap_list->prev = block;                             // we don't need to make sure the mmap list is sorted
        block->next = mmap_list;                         
    }

    void removeSbrkBlockFromList(MetaData* block) {          // for split
        size_t index = calcOrderOfBlock(block->size);
        if(block->prev==NULL) {                              //if we want to delete head
            smols_list[index] = block->next;
            block->next=NULL;
            return;
        }
        else if(block->next==NULL) {                         //if we want to remove at last

            (block->prev)->next = NULL;
            block->prev = NULL;
            return;
        }
        else {                                               //regular delete
            (block->prev)->next = block->next;
            (block->next)->prev = block->prev;
            block->next = NULL;
            block->prev = NULL;
        }
    }

    void freeMmapBlock(void* block) {                      // for split / mmap
        MetaData* p = getMetaData(block);
        if(p->prev == NULL) {                              //if we want to delete head
            mmap_list = p->next;
            p->next = NULL;
            return;
        }
        else if(p->next==NULL) {                         //if we want to remove at last

            (p->prev)->next = NULL;
            p->prev = NULL;
            return;
        }
        else {                                               //regular delete
            (p->prev)->next = p->next;
            (p->next)->prev = p->prev;
            p->next=NULL;
            p->prev=NULL;
        }

        //stats
        num_allocated_blocks -= 1;
        num_allocated_bytes -= p->size;
        munmap(p, p->size + MMD_SIZE);
    }

    void setFreeFinalFinalLastFree(MetaData* p) {
        p->is_free=true;

        //add to statistics
        this->num_free_blocks += 1;
        this->num_free_bytes += p->size;
    }

    void tryToMerge(MetaData* p) {
        size_t mem_size = MAX_SIZE_BLOCK - MMD_SIZE;
        if(p->size >= mem_size)                             // the size is bigger than max size - leave it be (too big to merge a ba bye)
        {
            setFreeFinalFinalLastFree(p);
            return;
        }
        // we can try and merge hell yeah lets go
        MetaData* buddy = calcXORForBuddies(p, p->size + MMD_SIZE);
        if(buddy->is_free == false)                             // aw we can't merge :(
        {
            setFreeFinalFinalLastFree(p);
            return;
        }
        MetaData* first, *second; 
        while(buddy->is_free == true)
        {   // else - we can try and merge more yeee
            removeSbrkBlockFromList(buddy);

            if(p < buddy) {                                                      // decide who is first in terms of address
                first = p;
                second = buddy;
            }
            else {
                first = buddy;
                second = p;
            }
            first->size = first->size + MMD_SIZE + second->size;                 // the metadata is no longer here with us so add him back
            if(first->size >= mem_size)                                          // can't merge this big ass hoe
                break;
            p = first;

            //stats
            this->num_free_blocks -= 1;

            buddy = calcXORForBuddies(p, MMD_SIZE + p->size);                   // on to the next victim i mean block
        }
        addSbrkBlock(first);
        setFreeFinalFinalLastFree(p);  
    }

    void freeSbrkBlock(void* block) {                           // free allocated block
        MetaData* p = getMetaData(block);
        if (p->is_free == true)                                 // if already free i don't care
            return;
        tryToMerge(p);
        return;
    }

    void firstSbrkAllocs() {
        // allignment inside heap so we'll have pretty alignment
        void* current_p_break = sbrk(0);
        intptr_t prog_break_address = (intptr_t)current_p_break;
        intptr_t aligned_prog_break_address = (prog_break_address + ALLIGNMEMT_FACTOR - 1) & ~(ALLIGNMEMT_FACTOR - 1);
        if(sbrk(aligned_prog_break_address - prog_break_address) == (void*)-1)
            return;

        // heaping shit ahead of time (32 units)
        MetaData* current = NULL;                                // initialize the 32 blocks
        for(int i = 0; i < INIT_NUM_BLOCKS; i++) {
            void* prog_break = sbrk(MAX_SIZE_BLOCK);                   // did not find free block - time to make a new one
            if(prog_break == (void*)-1)
                return;
            
            MetaData* new_block = (MetaData*)prog_break;
            new_block->size = INIT_BLOCK_UNIT - MMD_SIZE;
            new_block->is_free = true;
            new_block->next = NULL;
            new_block->prev = NULL;

            if (current == NULL) {
                size_t index = calcOrderOfBlock(new_block->size);
                current = new_block;
                smols_list[index] = current;
            }
            else {
                current->next = new_block;
                new_block->prev = current;
                current = current->next;
            }
        }
        this->num_allocated_blocks = INIT_NUM_BLOCKS;
        this->num_allocated_bytes = ALLIGNMEMT_FACTOR;
        this->num_free_blocks = INIT_NUM_BLOCKS;
        this->num_free_bytes = ALLIGNMEMT_FACTOR;
    }

    void* allocateSbrkBlock(size_t size) {                       // allocate block using sbrk
        if (is_first_smalloc) {                                  // only malloc when asked
            firstSbrkAllocs();
            is_first_smalloc = false;
        }

        MetaData* optimal_block = findBestBlockForSbrkAlloc(size);
        if (optimal_block == NULL)
            return NULL;
        
        // bc maybe we split it and recieve a lesser size
        removeSbrkBlockFromList(optimal_block);
        //start splitting
        size_t half_block_size = (optimal_block->size + MMD_SIZE) / 2;
        while((half_block_size >= (size + MMD_SIZE))
           && (half_block_size >= INIT_BLOCK_UNIT)) // can split
        {
            splitBlock(optimal_block);
            half_block_size = (optimal_block->size + MMD_SIZE) / 2;
            
            //stats
            this->num_allocated_blocks += 1;
        }
        // after splitting for who knows how long, we add this block as well yay
        addSbrkBlock(optimal_block);
        //now we can use the block
        optimal_block->is_free = false;

        //add to statistics
        this->num_free_blocks -= 1;
        this->num_free_bytes -= size;

        return optimal_block;
    }

    void* allocateMmapBlock(size_t size) {
        void* new_block = mmap(NULL, MMD_SIZE + size, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(new_block == MAP_FAILED)
            return NULL;

        MetaData* mmap_block = (MetaData*)new_block;
        addMmapBlock(mmap_block);
        mmap_block->size = size;
        mmap_block->is_free = false;
        mmap_block->next = NULL;
        mmap_block->prev = NULL;

        //add to statistics
        this->num_allocated_blocks += 1;
        this->num_allocated_bytes += size;

        return mmap_block;
    }

    size_t numfreeSbrkBlocks() { return num_free_blocks; }
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
    void* new_block;
    if( size >= MAX_SIZE_BLOCK) {
        new_block = block_list.allocateMmapBlock(size);
    }
    else {
        new_block = block_list.allocateSbrkBlock(size);
    }
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
    if(p == NULL)
        return;
    MetaData* block = block_list.getMetaData(p);
    if(block->size > MAX_SIZE_BLOCK)
        block_list.freeMmapBlock(p);                // even more easy peasy
    else
        block_list.freeSbrkBlock(p);                //easy peasy lemon squeezy
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
    MetaData* metadata = block_list.getMetaData(oldp);
    size_t old_size = metadata->size;
    if (old_size > MAX_SIZE_BLOCK) {                              // mmap
        if(size == old_size)                                      // need to make sure according to pdf
            return oldp;
        
        void* new_block = smalloc(size);
        if(new_block == NULL)                                     // faild
            return NULL;
        else if (size > old_size)                                 // pretty self explanatory
            memmove(new_block, oldp, old_size);
        else
            memmove(new_block, oldp, size);
        sfree(oldp);
        
        return new_block;
    }
    else {                                                        // sbrk
        if(size <= old_size)                                          // if new size < old size - no need to allocate again
            return oldp;
        
        //check to merge

        /*if(table.can_merge_to_create_block(details,size)) // check if we can merge
        {
            //make it return the new Metadata
            MetaData* new_allocated=table.merge_to_create_block(details,size);
            void* adrees_of_data=(char*)new_allocated+sizeof(MetaData);
            memmove(adrees_of_data,oldp,details->size);
            new_allocated->is_free=false;
            new_allocated->prev=NULL;
            new_allocated->next=NULL;
            return (char*)new_allocated+sizeof(MetaData);
        } */
    }

    // else - cannot merge

    void* new_block = smalloc(size);                              // using our lovely assistant smalloc we get a brand new spanking free block
    if (new_block == NULL)
        return NULL;                                              // or maybe not
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
    return block_list.numfreeSbrkBlocks();
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