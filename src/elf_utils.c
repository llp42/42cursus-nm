/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: elf_utils.c
 *
 * Helpers shared by the 32- and 64-bit paths: the bounds check every
 * offset read from the file must pass, the section/binding to type
 * character mapping, and the sort comparators.
 *
 * Author: Leonardo Lopes Pereira
 * Email: lepereir@student.42.fr
 *
 * SPDX-License-Identifier: blessing
 */

#include "ft_nm.h"

#include <string.h>

/*
 * Whether [offset, offset + size) lies inside the mapping. The subtraction
 * keeps the sum from wrapping on attacker-chosen values.
 */
bool elf_in_range(const t_file *file, uint64_t offset, uint64_t size)
{
	if (offset > file->size)
		return (false);
	return (size <= file->size - offset);
}

/*
 * Classifies a defined symbol from the flags of the section holding it.
 * Mirrors what nm derives from the BFD section flags.
 */
static char section_type_char(uint64_t flags, uint32_t sh_type)
{
	if (flags & SHF_EXECINSTR)
		return ('T');
	if ((flags & SHF_WRITE) && sh_type == SHT_NOBITS)
		return ('B');
	if (flags & SHF_WRITE)
		return ('D');
	if (flags & SHF_ALLOC)
		return ('R');
	return ('N');
}

/*
 * Order follows bfd_decode_symclass: common and undefined first, then the
 * indirect-function and weak cases, and only then the section lookup. Getting
 * the order wrong is what turns a defined weak object (V) into an undefined
 * one (v).
 *
 * Only the letters derived from a section (plus A) take the lowercase form for
 * local symbols; C, u, U, W, V, w, v and ? are reported as-is.
 */
char get_symbol_type64(const Elf64_Sym *sym, const Elf64_Shdr *shdrs, uint16_t shnum)
{
	uint8_t bind;
	uint8_t type;
	char c;

	bind = ELF64_ST_BIND(sym->st_info);
	type = ELF64_ST_TYPE(sym->st_info);
	if (sym->st_shndx == SHN_COMMON)
		return ('C');
	if (sym->st_shndx == SHN_UNDEF)
	{
		if (bind == STB_WEAK)
			return (type == STT_OBJECT ? 'v' : 'w');
		return ('U');
	}
	if (type == STT_GNU_IFUNC)
		return ('i');
	if (bind == STB_WEAK)
		return (type == STT_OBJECT ? 'V' : 'W');
	if (bind == STB_GNU_UNIQUE)
		return ('u');
	if (sym->st_shndx == SHN_ABS)
		c = 'A';
	else if (sym->st_shndx < shnum)
		c = section_type_char(shdrs[sym->st_shndx].sh_flags, shdrs[sym->st_shndx].sh_type);
	else
		return ('?');
	if (bind == STB_LOCAL)
		c += 'a' - 'A';
	return (c);
}

char get_symbol_type32(const Elf32_Sym *sym, const Elf32_Shdr *shdrs, uint16_t shnum)
{
	uint8_t bind;
	uint8_t type;
	char c;

	bind = ELF32_ST_BIND(sym->st_info);
	type = ELF32_ST_TYPE(sym->st_info);
	if (sym->st_shndx == SHN_COMMON)
		return ('C');
	if (sym->st_shndx == SHN_UNDEF)
	{
		if (bind == STB_WEAK)
			return (type == STT_OBJECT ? 'v' : 'w');
		return ('U');
	}
	if (type == STT_GNU_IFUNC)
		return ('i');
	if (bind == STB_WEAK)
		return (type == STT_OBJECT ? 'V' : 'W');
	if (bind == STB_GNU_UNIQUE)
		return ('u');
	if (sym->st_shndx == SHN_ABS)
		c = 'A';
	else if (sym->st_shndx < shnum)
		c = section_type_char(shdrs[sym->st_shndx].sh_flags, shdrs[sym->st_shndx].sh_type);
	else
		return ('?');
	if (bind == STB_LOCAL)
		c += 'a' - 'A';
	return (c);
}

int symbol_cmp(const void *a, const void *b)
{
	const t_symbol *sa;
	const t_symbol *sb;

	sa = (const t_symbol *)a;
	sb = (const t_symbol *)b;
	return (strcmp(sa->name, sb->name));
}

int symbol_cmp_reverse(const void *a, const void *b)
{
	return (symbol_cmp(b, a));
}
