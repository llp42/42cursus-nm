/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: elf_parser.c
 *
 * Identifies the input format and dispatches it: archives to the ar
 * path, little-endian ELF32/ELF64 to their symbol readers, anything
 * else to the 'file format not recognized' error.
 *
 * Author: Leonardo Lopes Pereira
 * Email: lepereir@student.42.fr
 *
 * SPDX-License-Identifier: blessing
 */

#include "ft_nm.h"

#include <stdio.h>

bool is_file_elf(t_file *file)
{
	const unsigned char *ident;

	if (!file->data || file->size < EI_NIDENT)
		return (false);
	ident = (const unsigned char *)file->data;
	return (ident[EI_MAG0] == ELFMAG0 && ident[EI_MAG1] == ELFMAG1 && ident[EI_MAG2] == ELFMAG2 &&
	        ident[EI_MAG3] == ELFMAG3);
}

static int elf_unrecognized(t_file *file)
{
	fprintf(stderr, "ft_nm: %s: file format not recognized\n", file->filename);
	return (-1);
}

/*
 * Identifies the format and dispatches it. The headers are read as native
 * structs, so a foreign byte order is rejected here rather than decoded into
 * nonsense.
 */
int elf_parse(t_nm *nm, t_file *file)
{
	const unsigned char *ident;

	if (!is_file_elf(file))
		return (elf_unrecognized(file));
	ident = (const unsigned char *)file->data;
	if (ident[EI_DATA] != ELFDATA2LSB)
		return (elf_unrecognized(file));
	if (ident[EI_CLASS] == ELFCLASS32)
	{
		if (file->size < sizeof(Elf32_Ehdr))
			return (elf_unrecognized(file));
		return (elf_process_symbols32(nm, file, (const Elf32_Ehdr *)file->data));
	}
	if (ident[EI_CLASS] == ELFCLASS64)
	{
		if (file->size < sizeof(Elf64_Ehdr))
			return (elf_unrecognized(file));
		return (elf_process_symbols64(nm, file, (const Elf64_Ehdr *)file->data));
	}
	return (elf_unrecognized(file));
}
