/* Copyright 2021 Hein-Pieter van Braam-Stewart
 *
 * This file is part of ppelib (Portable Portable Executable LIBrary)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <inttypes.h>
#include <ppe_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ppelib/ppelib-constants.h>

#include "ppelib_internal.h"
#include "main.h"

EXPORT_SYM uint8_t* ppelib_get_trailing_data(const ppelib_file_t *pe) {
	ppelib_reset_error();

	return pe->trailing_data;
}

EXPORT_SYM size_t ppelib_get_trailing_data_size(const ppelib_file_t *pe) {
	ppelib_reset_error();

	return pe->trailing_data_size;
}

EXPORT_SYM void ppelib_set_trailing_data(ppelib_file_t *pe, const uint8_t *buffer, size_t size) {
	ppelib_reset_error();

	if (!buffer && size) {
		ppelib_set_error("Can't set data from a NULL pointer");
		return;
	}

	void *oldptr = pe->trailing_data;

	if (!buffer || !size) {
		pe->trailing_data = NULL;
		pe->trailing_data_size = 0;
	} else {
		pe->trailing_data = malloc(size);
		if (!pe->trailing_data) {
			ppelib_set_error("Failed to allocate new trailing data");
			pe->trailing_data = oldptr;
			return;
		}

		memcpy(pe->trailing_data, buffer, size);
		pe->trailing_data_size = 0;
	}

	free(oldptr);
}

EXPORT_SYM ppelib_file_t* ppelib_create() {
	ppelib_reset_error();

	ppelib_file_t *pe = calloc(sizeof(ppelib_file_t), 1);
	if (!pe) {
		ppelib_set_error("Failed to allocate PE structure");
	}

	return pe;
}

EXPORT_SYM void ppelib_destroy(ppelib_file_t *pe) {
	if (!pe) {
		return;
	}

	if (pe->sections) {
		for (uint16_t i = 0; i < pe->header.number_of_sections; ++i) {
			free(pe->sections[i]->contents);
			free(pe->sections[i]);
		}
	}

	free(pe->dos_header.stub);
	free(pe->dos_header.message);
	free(pe->data_directories);
	free(pe->sections);
	free(pe->trailing_data);

	free(pe);
}

EXPORT_SYM ppelib_file_t* ppelib_create_from_buffer(const uint8_t *buffer, size_t size) {
	ppelib_reset_error();

	ppelib_file_t *pe = ppelib_create();
	if (ppelib_error_peek()) {
		ppelib_destroy(pe);
		return NULL;
	}

	size_t dos_header_size = ppelib_dos_header_deserialize(buffer, size, 0, &pe->dos_header);
	if (ppelib_error_peek()) {
		ppelib_destroy(pe);
		return NULL;
	}

#ifndef FUZZ // disable some checks to allow more invalid data to pass
	if (pe->dos_header.signature != MZ_SIGNATURE) {
		ppelib_set_error("Not a PE file (MZ signature missing)");
		ppelib_destroy(pe);
		return NULL;
	}
#endif

	if (size < pe->dos_header.pe_header_offset + sizeof(uint32_t)) {
		ppelib_set_error("Not a PE file (file too small)");
		ppelib_destroy(pe);
		return NULL;
	}

	if (dos_header_size > pe->dos_header.pe_header_offset) {
		ppelib_set_error("DOS header size larger than PE header offset");
		ppelib_destroy(pe);
		return NULL;
	}

	size_t dos_stub_size = pe->dos_header.pe_header_offset - dos_header_size;
	printf("DOS Stub size: %zi\n", dos_stub_size);
	pe->dos_header.stub = malloc(dos_stub_size);
	if (!pe->dos_header.stub) {
		ppelib_set_error("Couldn't allocate DOS stub");
		ppelib_destroy(pe);
		return NULL;
	}

	memcpy(pe->dos_header.stub, buffer + dos_header_size, dos_stub_size);
	pe->dos_header.stub_size = dos_stub_size;
	parse_dos_stub(&pe->dos_header);

#ifndef FUZZ // disable some checks to allow more invalid data to pass
	uint32_t signature = read_uint32_t(buffer + pe->dos_header.pe_header_offset);
	if (signature != PE_SIGNATURE) {
		ppelib_set_error("Not a PE file (PE00 signature missing)");
		ppelib_destroy(pe);
		return NULL;
	}
#endif

	pe->header_offset = pe->dos_header.pe_header_offset + 4;

	size_t header_size = ppelib_header_deserialize(buffer, size, pe->header_offset, &pe->header);
	if (ppelib_error_peek()) {
		ppelib_destroy(pe);
		return NULL;
	}

	if (pe->header.number_of_rva_and_sizes > (UINT32_MAX / 8)) {
		ppelib_set_error("File too small for directory entries (overflow)");
		ppelib_destroy(pe);
		return NULL;
	}

	pe->section_offset = pe->header_offset + header_size + (pe->header.number_of_rva_and_sizes * 8);
	if (pe->section_offset > size) {
		ppelib_set_error("File too small for directory entries");
		ppelib_destroy(pe);
		return NULL;
	}

	pe->start_of_section_data = ((size_t) (pe->header.number_of_sections) * 40) + pe->section_offset;
	if (pe->start_of_section_data > size) {
		ppelib_set_error("File too small for section headers");
		ppelib_destroy(pe);
		return NULL;
	}

	pe->sections = calloc(sizeof(void*) * pe->header.number_of_sections, 1);
	if (!pe->sections) {
		ppelib_set_error("Failed to allocate sections array");
		ppelib_destroy(pe);
		return NULL;
	}

	for (uint16_t i = 0; i < pe->header.number_of_sections; ++i) {
		pe->sections[i] = calloc(sizeof(section_t), 1);
		if (!pe->sections[i]) {
			for (uint16_t s = 0; s < i; ++s) {
				free(pe->sections[s]);
			}
			memset(pe->sections, 0, sizeof(void*) * pe->header.number_of_sections);
			ppelib_set_error("Failed to allocate sections");
			ppelib_destroy(pe);
			return NULL;
		}
	}

	size_t offset = pe->section_offset;
	pe->end_of_section_data = pe->start_of_section_data;

	for (uint16_t i = 0; i < pe->header.number_of_sections; ++i) {
		size_t section_size = ppelib_section_deserialize(buffer, size, offset, pe->sections[i]);
		if (ppelib_error_peek()) {
			ppelib_destroy(pe);
			return NULL;
		}

		section_t *section = pe->sections[i];
		size_t data_size = MIN(section->virtual_size, section->size_of_raw_data);

		if (section->pointer_to_raw_data + data_size > size || section->pointer_to_raw_data > size
				|| data_size > size) {
			ppelib_set_error("Section data outside of file");
			ppelib_destroy(pe);
			return NULL;
		}

		section->contents = malloc(data_size);
		if (!section->contents) {
			ppelib_set_error("Failed to allocate section data");
			ppelib_destroy(pe);
			return NULL;
		}

		section->contents_size = data_size;
		memcpy(section->contents, buffer + section->pointer_to_raw_data, section->contents_size);
		pe->end_of_section_data = MAX(pe->end_of_section_data,
				section->pointer_to_raw_data + section->size_of_raw_data);
		offset += section_size;
	}

	pe->data_directories = calloc(sizeof(data_directory_t) * pe->header.number_of_rva_and_sizes, 1);
	if (!pe->data_directories) {
		ppelib_set_error("Failed to allocate data directories");
		ppelib_destroy(pe);
		return NULL;
	}

	// Data directories don't have a dedicated deserialize function
	offset = pe->header_offset + header_size;
	for (uint32_t i = 0; i < pe->header.number_of_rva_and_sizes; ++i) {
		uint32_t dir_va = read_uint32_t(buffer + offset + 0);
		uint32_t dir_size = read_uint32_t(buffer + offset + 4);

		section_t *section = section_find_by_virtual_address(pe, dir_va);
		if (i != DIR_CERTIFICATE_TABLE && section) {
			pe->data_directories[i].section = section;
			pe->data_directories[i].offset = dir_va - section->virtual_address;
		} else {
			// Certificate tables' addresses aren't virtual. Despite the name.
			pe->data_directories[i].offset = dir_va;
		}
		pe->data_directories[i].size = dir_size;
		pe->data_directories[i].id = i;

		offset += 8;
	}

	if (size > pe->end_of_section_data) {
		pe->trailing_data_size = size - pe->end_of_section_data;
		pe->trailing_data = malloc(pe->trailing_data_size);
		if (!pe->trailing_data) {
			ppelib_set_error("Failed to allocate trailing data");
			ppelib_destroy(pe);
			return NULL;
		}
	}

	return pe;
}

EXPORT_SYM ppelib_file_t* ppelib_create_from_file(const char *filename) {
	ppelib_reset_error();
	size_t file_size;
	uint8_t *file_contents;

	FILE *f = fopen(filename, "rb");

	if (!f) {
		ppelib_set_error("Failed to open file");
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long ftell_size = ftell(f);
	rewind(f);

	if (ftell_size < 0) {
		fclose(f);
		ppelib_set_error("Unable to read file length");
		return NULL;
	}

	file_size = (size_t) ftell_size;

	if (!file_size) {
		fclose(f);
		ppelib_set_error("Empty file");
		return NULL;
	}

	file_contents = malloc(file_size);
	if (!file_size) {
		fclose(f);
		ppelib_set_error("Failed to allocate file data");
		return NULL;
	}

	size_t retsize = fread(file_contents, 1, file_size, f);
	if (retsize != file_size) {
		fclose(f);
		ppelib_set_error("Failed to read file data");
		return NULL;
	}

	fclose(f);

	ppelib_file_t *retval = ppelib_create_from_buffer(file_contents, file_size);
	free(file_contents);

	return retval;
}
