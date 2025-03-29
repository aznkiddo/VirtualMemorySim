#include "paging.h"
#include "page_splitting.h"
#include "swapops.h"
#include "stats.h"

 /* The frame table pointer. You will set this up in system_init. */
fte_t *frame_table;

/*  --------------------------------- PROBLEM 2 --------------------------------------
    In this problem, you will initialize the frame table.

    The frame table will be located at physical address 0 in our simulated
    memory. You will first assign the frame_table global variable to point to
    this location in memory. You should zero out the frame table, in case for
    any reason physical memory is not clean.

    You should then mark the first entry in the frame table as protected. We do
    this because we do not want our free frame allocator to give out the frame
    used by the frame table.

    HINTS:
        You will need to use the following global variables:
        - mem: Simulated physical memory already allocated for you.
        - PAGE_SIZE: The size of one page.
        You will need to initialize (set) the following global variable:
        - frame_table: a pointer to the first entry of the frame table

    -----------------------------------------------------------------------------------
*/
void system_init(void) {
    /*
     * 1. Set the frame table pointer to point to the first frame in physical
     * memory. Zero out the memory used by the frame table.
     *
     * Address "0" in memory will be used for the frame table. This table will
     * contain n frame table entries (fte_t), where n is the number of
     * frames in memory. The frame table will be useful later if we need to
     * evict pages during page faults.
     */
    frame_table = (fte_t *) &mem[0];
    memset(frame_table, 0, NUM_FRAMES * sizeof(fte_t)); // Zero out the frame table

    /*
     * 2. Mark the first frame table entry as protected.
     *
     * The frame table contains entries for all of physical memory,
     * however, there are some frames we never want to evict.
     * We mark these special pages as "protected" to indicate this.
     */

    frame_table[0].protected = 1; // Mark the first frame as protected
}

/*  --------------------------------- PROBLEM 3 --------------------------------------
    This function gets called every time a new process is created.
    You will need to allocate a new page table for the process in memory using the
    free_frame function so that the process can store its page mappings. Then, you
    will need to store the PFN of this page table in the process's PCB.

    HINTS:
        - Look at the pcb_t struct defined in pagesim.h to know what to set inside.
        - You are not guaranteed that the memory returned by the free frame allocator
        is empty - an existing frame could have been evicted for our new page table.
        - As in the previous problem, think about whether we need to mark any entries
        in the frame_table as protected after we allocate memory for our page table.
    -----------------------------------------------------------------------------------
*/
void proc_init(pcb_t *proc) {//pcn is a process control block 
    /* 
     * 1. Call the free frame allocator (free_frame) to return a free frame for
     * this process's page table. You should zero-out the memory.
     */
    pfn_t page_table_pfn = free_frame(); //pfn is a physical frame number
    //mem[page_table_pfn * PAGE_SIZE] = 0; // Zero out the memory for the page table
    memset(mem + (page_table_pfn * PAGE_SIZE), 0, PAGE_SIZE);


    /*
     * 2. Update the process's PCB with the frame number
     * of the newly allocated page table.
     *
     * Additionally, mark the frame's frame table entry as protected. You do not
     * want your page table to be accidentally evicted.
     */
    proc->saved_ptbr = page_table_pfn; // Set the page table base register to the PFN
    frame_table[page_table_pfn].protected = 1; // Mark the page table as protected
    frame_table[page_table_pfn].mapped = 1; // Mark the page table as mapped to prevent reuse by another process. 
    frame_table[page_table_pfn].referenced = 0; // Set referenced bit to 0
    frame_table[page_table_pfn].process = proc; // Set the process that owns this page table
    proc->state = PROC_RUNNING;

}

/*  --------------------------------- PROBLEM 4 --------------------------------------
    Swaps the currently running process on the CPU to another process.

    Every process has its own page table, as you allocated in proc_init. You will
    need to tell the processor to use the new process's page table.

    HINTS:
        - Look at the global variables defined in pagesim.h. You may be interested in
        the definition of pcb_t as well.
    -----------------------------------------------------------------------------------
 */
void context_switch(pcb_t *proc) {
    proc->state = PROC_RUNNING; // Set the new process to running
    // if (current_process)
    //     current_process->state = PROC_STO; // Set the old process to stopped
    PTBR = proc->saved_ptbr; // Set the page table base register to the new process's page table
    current_process = proc; // Update the current process to the new one
    //TODO: Increment the context switch count
}

/*  --------------------------------- PROBLEM 5 --------------------------------------
    Takes an input virtual address and returns the data from the corresponding
    physical memory address. The steps to do this are:

    1) Translate the virtual address into a physical address using the page table.
    2) Go into the memory and read/write the data at the translated address.

    Parameters:
        1) address     - The virtual address to be translated.
        2) rw          - 'r' if the access is a read, 'w' if a write
        3) data        - One byte of data to write to our memory, if the access is a write.
                         This byte will be NULL on read accesses.

    Return:
        The data at the address if the access is a read, or
        the data we just wrote if the access is a write.

    HINTS:
        - You will need to use the macros we defined in Problem 1 in this function.
        - You will need to access the global PTBR value. This will tell you where to
        find the page table. Be very careful when you think about what this register holds!
        - On a page fault, simply call the page_fault function defined in page_fault.c.
        You may assume that the pagefault handler allocated a page for your address
        in the page table after it returns.
        - Make sure to set the referenced bit in the frame table entry since we accessed the page.
        - Make sure to set the dirty bit in the page table entry if it's a write.
        - Make sure to update the stats variables correctly (see stats.h)
    -----------------------------------------------------------------------------------
 */
uint8_t mem_access(vaddr_t address, char rw, uint8_t data) {


    /* Split the address and find the page table entry */
    vpn_t vpn = vaddr_vpn(address); // Get the virtual page number
    uint16_t offset = vaddr_offset(address); // Get the offset into the page
    pte_t *page_table = (pte_t *) &mem[PTBR * PAGE_SIZE]; // Get the page table from memory
    pte_t *entry = &page_table[vpn]; // Get the page table entry for the virtual page number


    /* If an entry is invalid, just page fault to allocate a page for the page table. */
    if (!entry->valid) {
        //stats.page_faults++; // already incremented in page_fault method. 
        page_fault(address); // Call the page fault handler to allocate a page
        entry = &page_table[vpn]; // Get the page table entry again after the page fault
    }

    /* Set the "referenced" bit to reduce the page's likelihood of eviction */
    frame_table[entry->pfn].referenced = 1; // Set the referenced bit in the frame table entry


    /*
        The physical address will be constructed like this:
        -------------------------------------
        |     PFN    |      Offset          |
        -------------------------------------
        where PFN is the value stored in the page table entry.
        We need to calculate the number of bits are in the offset.

        Create the physical address using your offset and the page
        table entry.
    */
    pfn_t pfn = entry->pfn; // Get the physical frame number from the page table entry
    paddr_t paddr = (pfn << OFFSET_LEN) | offset; // Create the physical address
    //mem access
    /* Either read or write the data to the physical address
       depending on 'rw' */
       stats.accesses++; 
    if (rw == 'r') {
        stats.reads++; 
        
        return mem[paddr]; // Return the data at the physical address if it's a read
        
    } else {
        stats.writes++; 
        
        mem[paddr] = data; // Write the data to the physical address if it's a write
        entry->dirty = 1; // Set the dirty bit in the page table entry since we wrote to it
        return data; // Return the data we just wrote

    }
}

/*  --------------------------------- PROBLEM 8 --------------------------------------
    When a process exits, you need to free any pages previously occupied by the
    process. Otherwise, every time you closed and re-opened Microsoft Word, it
    would gradually eat more and more of your computer's usable memory.

    To free a process, you must clear the "mapped" bit on every page the process
    has mapped. If the process has swapped any pages to disk, you must call
    swap_free() using the page table entry pointer as a parameter.

    You must also clear the "protected" bits for the page table itself.
    -----------------------------------------------------------------------------------
*/
void proc_cleanup(pcb_t *proc) {
    /* Look up the process's page table */
    pte_t *page_table = (pte_t *) &mem[proc->saved_ptbr * PAGE_SIZE];
    /* Clear the protected bit for the page table itself */
    frame_table[proc->saved_ptbr].protected = 0;
    
    


    /* Iterate the page table and clean up each valid page */
    for (size_t i = 0; i < NUM_PAGES; i++) {
        pte_t *entry = &page_table[i]; // Get the page table entry
        if (swap_exists(entry)) { //check if swap exists
            swap_free(entry); //free the swap entry
        }
        if (entry->valid) {
            frame_table[entry->pfn].mapped = 0;
            entry->valid = 0;
        }
    }
 

    /* Free the page table itself in the frame table */

    frame_table[proc->saved_ptbr].mapped = 0; 
    frame_table[proc->saved_ptbr].referenced = 0; 
    frame_table[proc->saved_ptbr].process = NULL; 
    frame_table[proc->saved_ptbr].vpn = 0; 
}
