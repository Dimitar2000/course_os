#include <loader.h>
#include <stdio.h>
#include <vas2.h>
#include <vm2.h>

#define init(pc, sp) asm volatile("init_state %0 %1"::"i"(pc), "i"(sp));

int loadProcessFromElfFile(struct ProcessControlBlock * PCB, void * file) {

	// validate the input
	if(PCB == NULL || file == NULL) return -1;

	// Parse the header
	Elf elf_info;
	Elf32_Header * header = (Elf32_Header *)file;
	int result = elf_parse_header(&elf_info, header);
    if (result == -1) return -1;

    // Get the pointers to the header tables
	Elf32_SectionHeader * section_header_table = file + elf_info.sectionHeaderTableOffset;
	Elf32_ProgramHeader * program_header_table = file + elf_info.programHeaderTableOffset;

    // Initialize a new address space for the new process to create
    struct vas2 * new_vas = create_vas();
    stack_and_heap stackAndHeap;

    // Process the program(segment) header table - allocate pages, copy the loadable segments
    int processResult = processProgramHeaderTable(new_vas, &stackAndHeap, file, program_header_table, elf_info.programHeaderTableLength);

    if (processResult == -1) {
        free_vas(new_vas);
        FATAL("Creating a process image failed!\n");
    }

    kprintf("Creating the process image was successful!\n");

    // Add the pointer to the new address space to the PCB
    PCB->vas = new_vas;

    // Call init_state with pc = start_address and sp = top of stack
    init(elf_info.entry, stackAndHeap.stack_pointer)

	return 0;
}

void load_memcpy(void * dest, void * src, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) {
        ((char *)dest)[i] = *((char *)src);
    }
}

int processProgramHeaderTable(struct vas2 * vasToFill, stack_and_heap *stackAndHeap, void * file, Elf32_ProgramHeader * phtable, Elf32_Word t_size) {

    // Switch to the new process address space
    switch_to_vas(vasToFill);

    Elf32_Addr currentVirtualAddress = 0;

    // Iterate over the program headers
    for (Elf32_Word ph_index = 0; ph_index < t_size; ph_index++) {

        // Get the current program header
        Elf32_ProgramHeader * currentProgramHeader = phtable + ph_index;

        if (!validate_program_header(currentProgramHeader)) {
            return -1;
        }

        if (currentProgramHeader->program_type == EPT_LOAD) {

            /*
             *   Segment
             * =============
             * =============  <-  header->virtual_address
             *                   \
             *  segment           \
             *   data              |- header->file_size
             *                    /
             *                   /
             * -------------
             *                   \
             *   possible         \
             *   padding           |- header->memsize - file_size
             *  of zeroes         /
             *                   /
             * =============  <- header->virtual_address + memsize
             * =============
             */

            // Get the virtual address
            Elf32_Addr virtualAddress = currentProgramHeader->program_virtual_addr;
            Elf32_Addr segmentStartAddress = virtualAddress;
            Elf32_Addr programToRead = (Elf32_Addr)((char *)file + currentProgramHeader->program_offset);
            Elf32_Word fileSize = currentProgramHeader->program_filesz;
            Elf32_Word sizeLeftToAllocate = currentProgramHeader->program_memsz;

            // While there is more memory space that needs to be allocated
            while (sizeLeftToAllocate > 0) {

                    allocate_page(vasToFill,virtualAddress, currentProgramHeader->flags & EPF_EXEC);
                    sizeLeftToAllocate -= PAGE_SIZE;
                    virtualAddress += PAGE_SIZE;

            }

            // copy the segment data into the newly allocated pages
            load_memcpy((void *)segmentStartAddress, (void *)programToRead, fileSize);

            if (fileSize < currentProgramHeader->program_memsz) {
                Elf32_Addr padding_address = segmentStartAddress + fileSize;
                while (padding_address < segmentStartAddress + currentProgramHeader->program_memsz) {
                    *(char *)padding_address = '\0';
                    padding_address++;
                }
            }

            currentVirtualAddress = virtualAddress;
        }
    }

    /* Allocate pages for the stack and the heap.
        *  STACK
        *   ||  - // TODO MB ? - 16MB currently
        *   \/
        *  -----
        *   /\
        *   ||  - // TODO MB ? - 16MB currently
        *  HEAP
         */

    Elf32_Addr heap_address = currentVirtualAddress;

    // Allocate pages for the heap - 16MB = 4KB * 2^12
    size_t counter = 0;
    while (counter < PROCESS_HEAP_SIZE_IN_PAGES) {
        allocate_page(vasToFill, currentVirtualAddress, false);
        currentVirtualAddress += PAGE_SIZE;
        counter++;
    }

    // Allocate pages for the stack - 16MB = 4KB * 2^12
    counter = 0;
    while (counter < PROCESS_STACK_SIZE_IN_PAGES) {
        allocate_page(vasToFill, currentVirtualAddress, false);
        currentVirtualAddress += PAGE_SIZE;
        counter++;
    }

    Elf32_Addr stack_address = currentVirtualAddress - 4;

    //Set the stack and heap pointers in the passed structure.
    stackAndHeap->stack_pointer = stack_address;
    stackAndHeap->heap_pointer = heap_address;

    return 0;

}

int printSectionNames(void * file, Elf32_SectionHeader * shtable, Elf32_Word t_size, Elf32_Half names_index) {

	Elf32_SectionHeader * names_section = shtable + names_index;

	Elf32_Addr names = (uint32_t)((char *)file + names_section->section_offset);
	
	Elf32_Word size = names_section->section_size;

	Elf32_Word i = 0;

	while(i < t_size) {
		
		// Do not make boundary checks for the offset into the section name table
		kprintf("%s \n", (char *)names + shtable[i].section_name);
        i++;
	}

	return 0;
}
