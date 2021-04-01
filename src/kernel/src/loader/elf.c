
#include <elf_common_features.h>
#include <elf.h>
#include <klibc.h>


enum ELF_PROGRAM_TYPE validEPTypes[ACCEPTABLE_PROGRAM_TYPES] = {
    EPT_NULL,
    EPT_LOAD,
    EPT_DYNAMIC,
    EPT_NOTE
};

int elf_validate_magic_sequence(Elf32_Header* header) {
    if(header == NULL) return -1;
    if(header->magic_sequence[EI_MAG0] != ELFMAG0) {
        kprintf("ELF Header EI_MAG0 incorrect. \n");
        return -1;
    }
    if(header->magic_sequence[EI_MAG1] != ELFMAG1) {
        kprintf("ELF Header EI_MAG1 incorrect.\n");
        return -1;
    }
    if(header->magic_sequence[EI_MAG2] != ELFMAG2) {
        kprintf("ELF Header EI_MAG2 incorrect.\n");
        return -1;
    }
    if(header->magic_sequence[EI_MAG3] != ELFMAG3) {
        kprintf("ELF Header EI_MAG3 incorrect.\n");
        return -1;
    }
    return 0;
}

int elf_check_supported(Elf32_Header* header) {

    kprintf("Header address: %p\n", header);

    int result;

    if((result = elf_validate_magic_sequence(header)) < 0) {
        kprintf("Invalid ELF File.\n");
        return result;
    }
    if(header->bit_32_or_64 != EXPECTED_ELF_TYPE) {
        kprintf("Unsupported ELF File Class.\n");
        return -1;
    }
    if(header->type_of_endian != EXPECTED_ENDIANESS) {
        kprintf("Unsupported ELF File byte order.\n");
        return -1;
    }
    if(header->e_architecture != EXPECTED_MACHINE_ISA) {
        kprintf("Unsupported ELF File target.\n");
        return -1;
    }
    if(header->e_version != EXPECTED_VERSION) {
        kprintf("Unsupported ELF File version.\n");
        return -1;
    }
    if(header->e_type != ET_REL && header->e_type != ET_EXEC) {
        kprintf("Unsupported ELF File type.\n");
        return -1;
    }
    if (header->e_header_size != EXPECTED_HEADER_SIZE) {
        kprintf("Invalid header size.\n");
        return -1;
    }
    return 0;

}

int elf_parse_header(Elf * elf, Elf32_Header *elf_header) {

    if(elf == NULL || elf_header == NULL) return -1;

    // Check the validity of the Header
    int result;
    if ((result = elf_check_supported(elf_header) < 0)) {
        kprintf("Parsing of ELF file header failed!\n");
        return result;
    }

    elf->entry = elf_header->e_entry;
    elf->programHeaderTableOffset = elf_header->e_program_header_table_offset;
    elf->sectionHeaderTableOffset = elf_header->e_section_header_table_offset;
    elf->programHeaderTableLength = elf_header->e_program_header_table_num_entries;
    elf->sectionHeaderTableLength = elf_header->e_section_header_table_num_entries;
    elf->sht_index_names = elf_header->e_index_sec_head_tb_entry_names;

    return 0;
}

int check_is_type_acceptable(enum ELF_PROGRAM_TYPE type) {
    for (int i = 0; i < ACCEPTABLE_PROGRAM_TYPES; ++i) {
        if (type == validEPTypes[i]) return true;
    }
    return false;
}

int validate_program_header(Elf32_ProgramHeader * header) {
    if (!check_is_type_acceptable(header->program_type)) {
        kprintf("ELF Program Invalid: Type is not an acceptable or a valid one: %d", header->program_type);
        return -1;
    }
    if (header->program_filesz > header->program_memsz) {
        kprintf("ELF Program Invalid: File size is bigger than Memory Size");
        return -1;
    }

    if (header->program_type == EPT_DYNAMIC) {
        kprintf("ELF Program Invalid: Type is Dynamic and it is not currently supported.");
        return -1;
    }

    // TODO Validate other fields in case the program has LOAD type.
    return 0;
}
