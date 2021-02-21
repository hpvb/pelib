#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"

#define CHECK_BIT(var,val) ((var) & (val))

typedef struct struct_field {
	size_t offset;
	size_t size;
	const char* name;
	union {
		uint8_t c_val;
		uint16_t s_val;
		uint32_t i_val;
		uint64_t l_val;
	} value;
} struct_field_t;

const struct_field_t coff_header_fields[] = {
	{ 0,  2, "Machine", {0}},
	{ 2,  2, "NumberOfSections", {0}},
	{ 4,  4, "TimeDateStamp", {0}},
	{ 8,  4, "PointerToSymbolTable", {0}},
	{ 12, 4, "NumberOfSymbols", {0}},
	{ 16, 2, "SizeOfOptionalHeader", {0}},
	{ 18, 2, "Characteristics", {0}},
	{ 0, 0, "", {0}}
};

const struct_field_t pe_optional_header_standard_fields[] = {
	{ 0,  2, "Magic", {0}},
	{ 2,  1, "MajorLinkerVersion", {0}},
	{ 3,  1, "MinorLinkerVersion", {0}},
	{ 4,  4, "SizeOfCode", {0}},
	{ 8,  4, "SizeOfInitializedData", {0}},
	{ 12,  4, "SizeOfUninitializedData", {0}},
	{ 16,  4, "AddressOfEntryPoint", {0}},
	{ 20,  4, "BaseOfCode", {0}},
	{ 24,  4, "BaseOfData", {0}},
	{ 0, 0, "", {0}}
};

const struct_field_t peplus_optional_header_standard_fields[] = {
	{ 0,  2, "Magic", {0}},
	{ 2,  1, "MajorLinkerVersion", {0}},
	{ 3,  1, "MinorLinkerVersion", {0}},
	{ 4,  4, "SizeOfCode", {0}},
	{ 8,  4, "SizeOfInitializedData", {0}},
	{ 12,  4, "SizeOfUninitializedData", {0}},
	{ 16,  4, "AddressOfEntryPoint", {0}},
	{ 20,  4, "BaseOfCode", {0}},
	{ 0, 0, "", {0}}
};

const struct_field_t pe_optional_header_windows_fields[] = {
	{ 0, 4, "ImageBase", {0}}, 
	{ 4, 4, "SectionAlignment", {0}}, 
	{ 8, 4, "FileAlignment", {0}}, 
	{ 12, 2, "MajorOperatingSystemVersion", {0}}, 
	{ 14, 2, "MinorOperatingSystemVersion", {0}}, 
	{ 16, 2, "MajorImageVersion", {0}}, 
	{ 18, 2, "MinorImageVersion", {0}}, 
	{ 20, 2, "MajorSubsystemVersion", {0}}, 
	{ 22, 2, "MinorSubsystemVersion", {0}}, 
	{ 24, 4, "Win32VersionValue", {0}}, 
	{ 28, 4, "SizeOfImage", {0}}, 
	{ 32, 4, "SizeOfHeaders", {0}}, 
	{ 36, 4, "CheckSum", {0}}, 
	{ 40, 2, "Subsystem", {0}}, 
	{ 42, 2, "DllCharacteristics", {0}}, 
	{ 44, 4, "SizeOfStackReserve", {0}}, 
	{ 48, 4, "SizeOfStackCommit", {0}}, 
	{ 52, 4, "SizeOfHeapReserve", {0}}, 
	{ 56, 4, "SizeOfHeapCommit", {0}}, 
	{ 60, 4, "LoaderFlags", {0}}, 
	{ 64, 4, "NumberOfRvaAndSizes", {0}},
	{ 0, 0, "", {0}}
};

const struct_field_t peplus_optional_header_windows_fields[] = {
	{ 0, 8, "ImageBase", {0}},
	{ 8, 4, "SectionAlignment", {0}},
	{ 12, 4, "FileAlignment", {0}},
	{ 16, 2, "MajorOperatingSystemVersion", {0}},
	{ 18, 2, "MinorOperatingSystemVersion", {0}},
	{ 20, 2, "MajorImageVersion", {0}},
	{ 22, 2, "MinorImageVersion", {0}},
	{ 24, 2, "MajorSubsystemVersion", {0}},
	{ 26, 2, "MinorSubsystemVersion", {0}},
	{ 28, 4, "Win32VersionValue", {0}},
	{ 32, 4, "SizeOfImage", {0}},
	{ 36, 4, "SizeOfHeaders", {0}},
	{ 40, 4, "CheckSum", {0}},
	{ 44, 2, "Subsystem", {0}},
	{ 46, 2, "DllCharacteristics", {0}},
	{ 48, 8, "SizeOfStackReserve", {0}},
	{ 56, 8, "SizeOfStackCommit", {0}},
	{ 64, 8, "SizeOfHeapReserve", {0}},
	{ 72, 8, "SizeOfHeapCommit", {0}},
	{ 80, 4, "LoaderFlags", {0}},
	{ 84, 4, "NumberOfRvaAndSizes", {0}},
	{ 0, 0, "", {0}}
};

typedef struct pefile {
	uint8_t* stub;
	struct_field_t coff_header[8];
	uint16_t magic;
	struct_field_t *optional_header_standard;
	struct_field_t *optional_header_windows;
} pefile_t;

uint8_t read_char(const uint8_t* buffer) {
	return *buffer;
}

uint16_t read_short(const uint8_t* buffer) {
	return *(uint16_t*)buffer;
}

uint32_t read_int(const uint8_t* buffer) {
	return *(uint32_t*)buffer;
}

uint64_t read_long(const uint8_t* buffer) {
	return *(uint64_t*)buffer;
}

const char* map_lookup(uint32_t value, map_entry_t* map) {
	map_entry_t* m = map;
	while (m->string) {
		if (m->value == value) {
			return m->string;
		}
		++m;
	}

	return NULL;
}

struct_field_t* get_field(const char* name, struct_field_t* header) {
	struct_field_t* field = header;

	while (field->size) {
		if (strcmp(field->name, name) == 0) {
			return field;
		}
		++field;
	}

	fprintf(stderr, "Field %s not found\n", name);
	return NULL;
}

uint8_t get_field_char(const char* name, struct_field_t* header) {
	struct_field_t* field = get_field(name, header);
	if (field) {
		return field->value.c_val;
	}
	return 0;
}

uint16_t get_field_short(const char* name, struct_field_t* header) {
	struct_field_t* field = get_field(name, header);
	if (field) {
		return field->value.s_val;
	}
	return 0;
}

uint32_t get_field_int(const char* name, struct_field_t* header) {
	struct_field_t* field = get_field(name, header);
	if (field) {
		return field->value.i_val;
	}
	return 0;
}

uint64_t get_field_long(const char* name, struct_field_t* header) {
	struct_field_t* field = get_field(name, header);
	if (field) {
		return field->value.l_val;
	}
	return 0;
}

void parse_header(const uint8_t* buffer, const struct_field_t* header, struct_field_t* dest) {
	const struct_field_t* field = header;
	uint32_t i = 0;

	while (field->size) {
		dest[i].offset = field->offset;
		dest[i].size = field->size;
		dest[i].name = field->name;
		dest[i].value.l_val = 0;

		switch (field->size) {
			case 1:
				dest[i].value.c_val = read_char(buffer + field->offset);
				break;
			case 2:
				dest[i].value.s_val = read_short(buffer + field->offset);
				break;
			case 4:
				dest[i].value.i_val = read_int(buffer + field->offset);
				break;
			case 8:
				dest[i].value.l_val = read_long(buffer + field->offset);
				break;
			default:
				fprintf(stderr, "Unknown field size %li\n", field->size);
		}
		++field;
		++i;
	}

	dest[i].size = 0;
}

void print_field_name(const char* name, struct_field_t* header) {
	printf("%s: %li\n", name, get_field_long(name, header));
}

void print_field_name_hex(const char* name, struct_field_t* header) {
	printf("%s: 0x%08lX\n", name, get_field_long(name, header));
}

void print_coff_header(struct_field_t* header) {
	uint16_t machine = get_field_short("Machine", header);

	const char* machine_type = map_lookup(machine, machine_type_map);
	if (! machine_type) {
		fprintf(stderr, "Invalid machine type?");
		return;
	}

	printf("COFF header:\n");
	printf("Machine: %s\n", machine_type);
	print_field_name("NumberOfSections", header);
	print_field_name("TimeDateStamp", header);
	print_field_name("PointerToSymbolTable", header);
	print_field_name("NumberOfSymbols", header);
	print_field_name("SizeOfOptionalHeader", header);
	printf("Characteristics: ");

	uint16_t characteristics = get_field_short("Characteristics", header);

	map_entry_t* c_map = characteristics_map;
	while (c_map->string) {
		if (CHECK_BIT(characteristics, c_map->value)) {
			printf("%s ", c_map->string);
		}
		++c_map;
	}
	printf("\n");
	printf("\n");
}

void print_optional_header_standard(struct_field_t* header) {
	uint16_t magic = get_field_short("Magic", header);

	printf("Optional standard headers:\n");
	printf("Magic: ");
	if (magic == PE32_MAGIC) printf("PE\n");
	if (magic == PE32PLUS_MAGIC) printf("PE+\n");
	print_field_name("MajorLinkerVersion", header);
	print_field_name("MinorLinkerVersion", header);
	print_field_name("SizeOfCode", header);
	print_field_name("SizeOfInitializedData", header);
	print_field_name("SizeOfUninitializedData", header);
	print_field_name("AddressOfEntryPoint", header);
	print_field_name("BaseOfCode", header);

	if (magic == PE32_MAGIC) print_field_name("BaseOfData", header);
	printf("\n");
}

void print_optional_header_windows(struct_field_t* header) {
	uint16_t subsystem = get_field_short("Subsystem", header);
	uint16_t dll_characteristics = get_field_short("DllCharacteristics", header);

	printf("Optional windows headers:\n");

	print_field_name_hex("ImageBase", header);
	print_field_name("SectionAlignment", header);
	print_field_name("FileAlignment", header);
	print_field_name("MajorOperatingSystemVersion", header);
	print_field_name("MinorOperatingSystemVersion", header);
	print_field_name("MajorImageVersion", header);
	print_field_name("MinorImageVersion", header);
	print_field_name("MajorSubsystemVersion", header);
	print_field_name("MinorSubsystemVersion", header);
	print_field_name("Win32VersionValue", header);
	print_field_name("SizeOfImage", header);
	print_field_name("SizeOfHeaders", header);
	print_field_name("CheckSum", header);
	printf("Subsystem: %s\n", map_lookup(subsystem, windows_subsystem_map));
	printf("DllCharacteristics: ");
	map_entry_t* d_map = dll_characteristics_map;
	while (d_map->string) {
		if (CHECK_BIT(dll_characteristics, d_map->value)) {
			printf("%s ", d_map->string);
		}
		++d_map;
	}
	printf("\n");
	print_field_name("SizeOfStackReserve", header);
	print_field_name("SizeOfStackCommit", header);
	print_field_name("SizeOfHeapReserve", header);
	print_field_name("SizeOfHeapCommit", header);
	print_field_name("LoaderFlags", header);
	print_field_name("NumberOfRvaAndSizes", header);
	printf("\n");
}

int read_pe_file(const char* filename, uint8_t** file, size_t* size, uint32_t* pe_header_offset) {
	FILE *f = fopen(filename, "r");

	if (fseek(f, PE_SIGNATURE, SEEK_SET) == -1) {
		perror("Seeking to PE header offset");
		return 1;
	}

	size_t retsize;

	retsize = fread(pe_header_offset, 1, 4, f);
	if (retsize != 4) {
		fprintf(stderr, "Couldn't read PE header offset. Got %li bytes, expected 4\n", retsize);
		return 1;
	}

	if (fseek(f, *pe_header_offset, SEEK_SET) == -1) {
		perror("Seeking to PE header");
		return 1;
	}

	uint8_t signature[4];
	retsize = fread(&signature, 1, 4, f);
	if (retsize != 4) {
		fprintf(stderr, "Couldn't read PE signature. Got %li bytes, expected 4\n", retsize);
		return 1;
	}

	if (memcmp(signature, "PE\0", 4) != 0) {
		fprintf(stderr, "Not a PE file. Got 0x%X 0x%X 0x%X 0x%X, expected 0x%X 0x%X 0x%X 0x%X\n", signature[0], signature[1], signature[2], signature[3], 'P', 'E', 0, 0);
	}

	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	rewind(f);
	
	*file = malloc(*size);
	if (! *file) {
		fprintf(stderr, "Unable to allocate memory\n");
		return 1;
	}

	retsize = fread(*file, 1, *size, f);
	if (retsize != *size) {
		fprintf(stderr, "Couldn't file. Got %li bytes, expected 4\n", retsize);
		return 1;
	}
	fclose(f);

	return 0;
}

int main(int argc, char* argv[]) {
	if (! argc) return 1;

	pefile_t pe;
	uint8_t* file;
	size_t size;
	uint32_t pe_header_offset;

	if (read_pe_file(argv[1], &file, &size, &pe_header_offset)) {
		return 1;
	}

	uint32_t coff_header_offset = pe_header_offset + 4;

	if (size < coff_header_offset + COFF_HEADER_SIZE) {
		fprintf(stderr, "File size too small\n");
		return 1;
	}
	
	parse_header(file + coff_header_offset, coff_header_fields, pe.coff_header);
	print_coff_header(pe.coff_header);

	uint32_t pe_optional_header_offset = coff_header_offset + COFF_HEADER_SIZE;
	uint16_t pe_optional_header_size = get_field_short("SizeOfOptionalHeader", pe.coff_header);

	if (! pe_optional_header_size) {
		fprintf(stderr, "No optional headers\n");
		return 1;
	}

	if (size < pe_optional_header_offset + pe_optional_header_size) {
		fprintf(stderr, "File size too small\n");
		return 1;
	}
	pe.magic = read_short(file + pe_optional_header_offset);

	if (pe.magic == PE32_MAGIC) {
		pe.optional_header_standard = malloc(sizeof(struct_field_t) * (PE_OPTIONAL_HEADER_STANDARD_ENTRIES + 1));
		pe.optional_header_windows = malloc(sizeof(struct_field_t) * (PE_OPTIONAL_HEADER_WINDOWS_ENTRIES + 1));
		parse_header(file + pe_optional_header_offset, pe_optional_header_standard_fields, pe.optional_header_standard);
		parse_header(file + pe_optional_header_offset + PE_OPTIONAL_HEADER_STANDARD_SIZE, pe_optional_header_windows_fields, pe.optional_header_windows);
	} else if (pe.magic == PE32PLUS_MAGIC) {
		pe.optional_header_standard = malloc(sizeof(struct_field_t) * (PEPLUS_OPTIONAL_HEADER_STANDARD_ENTRIES + 1));
		pe.optional_header_windows = malloc(sizeof(struct_field_t) * (PEPLUS_OPTIONAL_HEADER_WINDOWS_ENTRIES + 1));
		parse_header(file + pe_optional_header_offset, peplus_optional_header_standard_fields, pe.optional_header_standard);
		parse_header(file + pe_optional_header_offset + PEPLUS_OPTIONAL_HEADER_STANDARD_SIZE, peplus_optional_header_windows_fields, pe.optional_header_windows);
	} else {
		fprintf(stderr, "Do not know how to handle this type of PE\n");
		return 1;
	}

	print_optional_header_standard(pe.optional_header_standard);
	print_optional_header_windows(pe.optional_header_windows);

	pe.stub = malloc(pe_header_offset);
	if (! pe.stub) {
		fprintf(stderr, "Failed to allocate memory\n");
		return 1;
	}
	memcpy(pe.stub, file, pe_header_offset);	

	free(file);
	free(pe.optional_header_standard);
	free(pe.optional_header_windows);
	free(pe.stub);
}
