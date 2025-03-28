#include "paging.h"
#include "swapops.h"
#include "stats.h"

/*  --------------------------------- PROBLEM 6 --------------------------------------
    Page fault handler.

    When the CPU encounters an invalid address mapping in a page table,
    it invokes the OS via this handler.

    Your job is to put a mapping in place so that the translation can
    succeed. You can use free_frame() to make an available frame.
    Update the page table with the new frame, and don't forget
    to fill in the frame table.

    Lastly, you must fill your newly-mapped page with data. If the page
    has never mapped before, just zero the memory out. Otherwise, the
    data will have been swapped to the disk when the page was
    evicted. Call swap_read() to pull the data back in.

    HINTS:
         - You will need to use the global variable current_process when
           setting the frame table entry.

    ----------------------------------------------------------------------------------
 */
void page_fault(vaddr_t address) {
    /* First, split the faulting address and locate the page table entry */
      vpn_t vpn = vaddr_vpn(address); 
      //uint16_t offset = vaddr_offset(address); //not needed
      pte_t *pt_base = (pte_t *) &mem[PTBR * PAGE_SIZE];
      pte_t *entry = &pt_base[vpn];


    /* It's a page fault, so the entry obviously won't be valid. Grab
       a frame to use by calling free_frame(). */
    pfn_t pfn = free_frame(); // Get a free frame for the page


    /* Update the page table entry. Make sure you set any relevant bits. */
    entry->valid = 1; // Set the valid bit to indicate the page is now valid
    entry->pfn = pfn; // Set the physical frame number in the page table entry
    entry->dirty = 0; // Clear the dirty bit since we haven't written to it yet
    

    /* Update the frame table. Make sure you set any relevant bits. */
    frame_table[pfn].mapped = 1; // Mark the frame as mapped
    // frame_table[pfn].referenced = 1; // Set the referenced bit to indicate it's been used
    frame_table[pfn].process = current_process; // Set the owning process
    frame_table[pfn].vpn = vpn; // Set the virtual page number for the frame

    /* Initialize the page's memory. On a page fault, it is not enough
     * just to allocate a new frame. We must load in the old data from
     * disk into the frame. If there was no old data on disk, then
     * we need to clear out the memory (why?).
     *
     * 1) Get a pointer to the new frame in memory.
     * 2) If the page has swap set, then we need to load in data from memory
     *    using swap_read().
     * 3) Else, just clear the memory.
     *
     * Otherwise, zero the page's memory. If the page is later written
     * back, swap_write() will automatically allocate a swap entry.
     */
    uint8_t *frame_ptr = &mem[pfn * PAGE_SIZE]; // Get a pointer to the new frame in memory
    if (swap_exists(entry)) { // Check if the page has swap data
        swap_read(entry, frame_ptr); // Load the old data from disk into the frame
    } else {
        memset(frame_ptr, 0, PAGE_SIZE); // Clear the memory if there's no old data
    }

    if (entry->pfn >= NUM_FRAMES) {
      panic("invalid page number in page_fault method\n");
    }

     stats.page_faults++; // Increment the page fault count in the stats

}
