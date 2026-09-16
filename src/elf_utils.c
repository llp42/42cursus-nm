/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: elf_utils.c
 *
 * Helpers shared by the 32- and 64-bit paths: the bounds check every
 * offset read from the file must pass, the section flags to type
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
char section_type_char(uint64_t flags, uint32_t sh_type)
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

int symbol_cmp(const void *a, const void *b)
{
	const t_symbol *sa;
	const t_symbol *sb;

	sa = (const t_symbol *)a;
	sb = (const t_symbol *)b;
	return (strcoll(sa->name, sb->name));
}

int symbol_cmp_reverse(const void *a, const void *b)
{
	return (symbol_cmp(b, a));
}
