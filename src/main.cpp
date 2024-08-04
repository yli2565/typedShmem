#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include "p3Heap.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block.
 */
typedef struct blockHeader
{

    /*
     * 1) The size of each heap block must be a multiple of 8
     * 2) heap blocks have blockHeaders that contain size and status bits
     * 3) free heap block contain a footer, but we can use the blockHeader
     *.
     * All heap blocks have a blockHeader with size and status
     * Free heap blocks have a blockHeader as its footer with size only
     *
     * Status is stored using the two least significant bits.
     *   Bit0 => least significant bit, last bit
     *   Bit0 == 0 => free block
     *   Bit0 == 1 => allocated block
     *
     *   Bit1 => second last bit
     *   Bit1 == 0 => previous block is free
     *   Bit1 == 1 => previous block is allocated
     *
     * Start Heap:
     *  The blockHeader for the first block of the heap is after skip 4 bytes.
     *  This ensures alignment requirements can be met.
     *
     * End Mark:
     *  The end of the available memory is indicated using a size_status of 1.
     *
     * Examples:
     *
     * 1. Allocated block of size 24 bytes:
     *    Allocated Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 25
     *      If the previous block is allocated p-bit=1 size_status would be 27
     *
     * 2. Free block of size 24 bytes:
     *    Free Block Header:
     *      If the previous block is free      p-bit=0 size_status would be 24
     *      If the previous block is allocated p-bit=1 size_status would be 26
     *    Free Block Footer:
     *      size_status should be 24
     */
    int size_status;

} blockHeader;

/* Global variable - DO NOT CHANGE NAME or TYPE.
 * It must point to the first block in the heap and is set by init_heap()
 * i.e., the block at the lowest address.
 */
blockHeader *heap_start = NULL;

/* Size of heap allocation padded to round to nearest page size.
 */
int alloc_size;

/*
 * Additional global variables may be added as needed below
 * TODO: add global variables needed by your function
 */

/*
 * Maximum value of int, use as placeholder for best fit block size (so that any reasonable block size can replace this value)
 */
static const int MAX_INT = ((unsigned int)~0 >> 1);
/*
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block (payload) on success.
 * Returns NULL on failure.
 *
 * This function must:
 * - Check size - Return NULL if size < 1
 * - Determine block size rounding up to a multiple of 8
 *   and possibly adding padding as a result.
 *
 * - Use BEST-FIT PLACEMENT POLICY to chose a free block
 *
 * - If the BEST-FIT block that is found is exact size match
 *   - 1. Update all heap blocks as needed for any affected blocks
 *   - 2. Return the address of the allocated block payload
 *
 * - If the BEST-FIT block that is found is large enough to split
 *   - 1. SPLIT the free block into two valid heap blocks:
 *         1. an allocated block
 *         2. a free block
 *         NOTE: both blocks must meet heap block requirements
 *       - Update all heap block header(s) and footer(s)
 *              as needed for any affected blocks.
 *   - 2. Return the address of the allocated block payload
 *
 *   Return if NULL unable to find and allocate block for required size
 *
 * Note: payload address that is returned is NOT the address of the
 *       block header.  It is the address of the start of the
 *       available memory for the requesterr.
 *
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void *balloc(int size)
{
    /*
    Initialize the return value to NULL
    */
    void *payload_ptr = NULL;

    if (size < 1)
        return NULL;

    /*
    The padding size that rounds up the block size to a multiple of 8
    */
    int padsize = ((size + 4) % 8) == 0 ? 0 : (8 - ((size + 4) % 8));
    /*
    The minimum required size of the new block
    */
    int required_size = size + 4 + padsize;

    /*
    Pointer to iterate the heap
    */
    blockHeader *current = heap_start;

    /*
    Record of current best fit block
    */
    blockHeader *best = NULL;
    /*
    Track the best fit block size
    */
    int best_size = MAX_INT;

    // Iterate through all blocks to find the best fit
    while (current->size_status != 1)
    {
        if (current >= heap_start + alloc_size)
            printf("ERROR: balloc out of heap space\n");
        int block_size = current->size_status & ~0b11; // Mask with ...11100
        int used_flag = current->size_status & 0b01;   // Mask with ...00001

        // Only cares about free block that is larger than required size; smaller than current best fit
        if (!used_flag && block_size >= required_size && block_size < best_size)
        {
            best = current;
            best_size = block_size;
        }

        // Move to the next block
        current = (blockHeader *)((char *)current + block_size);
    }
    if (best != NULL)
    {

        // If the block we find is bigger than the required size, we need to split it
        if (best_size > required_size)
        {
            blockHeader *new_free_block_header = (blockHeader *)((char *)best + required_size);
            blockHeader *new_free_block_footer = (blockHeader *)((char *)best + best_size - 4);

            int new_free_block_size = best_size - required_size;
            if (new_free_block_size % 8 != 0)
                printf("ERROR: balloc requires block size to be a multiple of 8\n");

            // The new header should indicate size = new_free_block_size, previous block is occupied, current block is free
            new_free_block_header->size_status = new_free_block_size | 0b10;

            // The new footer should indicate size = new_free_block_size
            new_free_block_footer->size_status = new_free_block_size;
        }
        // Update the header of the best fit block
        best->size_status = required_size | (best->size_status & 0b10) | 0b01;

        blockHeader *next_used_header = (blockHeader *)((char *)best + required_size);

        // If the next block is not the end of the heap
        if (next_used_header->size_status != 1)
        {

            // Set the next block header's p bit to 1
            next_used_header->size_status |= 0b10;
        }

        // Return the address of the payload
        payload_ptr = (void *)((char *)best + 4);
    }

    return payload_ptr;
}

/*
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - Update header(s) and footer as needed.
 *
 * If free results in two or more adjacent free blocks,
 * they will be immediately coalesced into one larger free block.
 * so free blocks require a footer (blockHeader works) to store the size
 *
 * TIP: work on getting immediate coalescing to work after your code
 *      can pass the tests in partA and partB of tests/ directory.
 *      Submit code that passes partA and partB to Canvas before continuing.
 */
int bfree(void *ptr)
{

    // Return -1 if ptr is NULL.
    if (ptr == NULL)
        return -1;

    // Return -1 if ptr is not a multiple of 8.
    if (((size_t)ptr) % 8 != 0)
        return -1;

    // Return -1 if ptr is outside of the heap space.
    if (ptr < ((void *)heap_start) || ptr >= (void *)((char *)heap_start + alloc_size))
        return -1;

    blockHeader *victim_header = (blockHeader *)((char *)ptr - 4);

    // Return -1 if ptr block is already freed.
    if (!(victim_header->size_status & 0b01))
        return -1;

    // Set the 'a' flag to 0
    victim_header->size_status &= ~0b1;

    // Immediate coalescing

    // First, coalesce with the free block(s) after victim block
    /*
    The temp pointer to track the last consecutive free block connected to the victim block
    */
    blockHeader *last_free_block = victim_header;
    /*
    Total size of the final free block (as a whole)
    */
    int total_size = 0;

    // Find the last consecutive block that is free
    while (last_free_block->size_status != 1 && !(last_free_block->size_status & 0b01))
    {
        int block_size = last_free_block->size_status & ~0b11; // Mask with ...11100
        total_size += block_size;
        last_free_block = (blockHeader *)((char *)last_free_block + block_size);
    }

    // Create the footer of the new free block at the last free block's footer position
    blockHeader *victim_footer = (blockHeader *)((char *)victim_header + total_size - 4);

    if (total_size % 8 != 0)
        printf("ERROR: bfree requires block size to be a multiple of 8\n");

    // Update the header of the victim block
    victim_header->size_status = total_size | (victim_header->size_status & 0b10); // Keep the p flag, update size and 'a' flag

    // Update the footer of the victim block
    victim_footer->size_status = total_size;

    // Find the header of the next used block, prepare to update it 'p' flag
    blockHeader *next_used_header = (blockHeader *)((char *)victim_header + total_size);

    // Only update the next block's header if it's not the end of the heap
    if (next_used_header->size_status != 1)
    {
        if (!(next_used_header->size_status & 0b01))
            printf("ERROR: The next block should be in use, else it should also be coalesced\n");

        // Update next block's header
        next_used_header->size_status = (next_used_header->size_status & ~0b11) | 0b01;
    }

    // Then, coalesce with the previous free block(s)
    /*
    The temp pointer to track the first consecutive free block connect to the victim block
    */
    blockHeader *first_free_block = victim_header;
    total_size = 0;

    // Find the first block that is free
    while ((first_free_block->size_status & 0b11) == 0b00)
    {
        blockHeader *prev_footer = (blockHeader *)((char *)first_free_block - 4);
        int block_size = prev_footer->size_status;
        total_size += block_size;
        if (total_size % 8 != 0)
            printf("ERROR: bfree requires block size to be a multiple of 8\n");
        first_free_block = (blockHeader *)((char *)first_free_block - block_size);
    }

    /*
    The new header of the whole big free block
    */
    blockHeader *new_victim_header = (blockHeader *)((char *)victim_header - total_size);
    total_size += victim_header->size_status & ~0b11;
    new_victim_header->size_status = total_size | (new_victim_header->size_status & 0b10);
    victim_footer->size_status = total_size;

    return 0;
}

/*
 * Initializes the memory allocator.
 * Called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int init_heap(int sizeOfRegion)
{
    static int allocated_once = 0; // prevent multiple myInit calls

    int pagesize;   // page size
    int padsize;    // size of padding when heap size not a multiple of page size
    void *mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader *end_mark;

    if (0 != allocated_once)
    {
        fprintf(stderr,
                "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }

    if (sizeOfRegion <= 0)
    {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize from O.S.
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd)
    {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr)
    {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    alloc_size -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heap_start = (blockHeader *)mmap_ptr + 1;

    // Set the end mark
    end_mark = (blockHeader *)((void *)heap_start + alloc_size);
    end_mark->size_status = 1;

    // Set size in header
    heap_start->size_status = alloc_size;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heap_start->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader *)((void *)heap_start + alloc_size - 4);
    footer->size_status = alloc_size;

    return 0;
}

/* STUDENTS MAY EDIT THIS FUNCTION, but do not change function header.
 * TIP: review this implementation to see one way to traverse through
 *      the blocks in the heap.
 *
 * Can be used for DEBUGGING to help you visualize your heap structure.
 * It traverses heap blocks and prints info about each block found.
 *
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts)
 * t_End    : address of the last byte in the block
 * t_Size   : size of the block as stored in the block header
 */
void disp_heap()
{

    int counter;
    char status[6];
    char p_status[6];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    blockHeader *current = heap_start;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used = -1;

    fprintf(stdout,
            "********************************** HEAP: Block List ****************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout,
            "--------------------------------------------------------------------------------\n");

    while (current->size_status != 1)
    {
        t_begin = (char *)current;
        t_size = current->size_status;

        if (t_size & 1)
        {
            // LSB = 1 => used block
            strcpy(status, "alloc");
            is_used = 1;
            t_size = t_size - 1;
        }
        else
        {
            strcpy(status, "FREE ");
            is_used = 0;
        }

        if (t_size & 2)
        {
            strcpy(p_status, "alloc");
            t_size = t_size - 2;
        }
        else
        {
            strcpy(p_status, "FREE ");
        }

        if (is_used)
            used_size += t_size;
        else
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%4i\n", counter, status,
                p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);

        current = (blockHeader *)((char *)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout,
            "--------------------------------------------------------------------------------\n");
    fprintf(stdout,
            "********************************************************************************\n");
    fprintf(stdout, "Total used size = %4d\n", used_size);
    fprintf(stdout, "Total free size = %4d\n", free_size);
    fprintf(stdout, "Total size      = %4d\n", used_size + free_size);
    fprintf(stdout,
            "********************************************************************************\n");
    fflush(stdout);

    return;
}

//		p3Heap.c (SP24)