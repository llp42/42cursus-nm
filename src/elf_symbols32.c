/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: elf_symbols32.c
 *
 * Symbol handling for ELF32 images: locates the symbol table and its
 * string table, filters entries per the -g/-u flags, sorts them and
 * prints the result. Mirror of elf_symbols64.c.
 *
 * Author: Leonardo Lopes Pereira
 * Email: lepereir@student.42.fr
 *
 * SPDX-License-Identifier: blessing
 */

#include "ft_nm.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Resolves the symbol table and its string table, validating every offset
 * against the mapped size first. Nothing below this function may assume a
 * field read from the file is sane. A zero entry size would divide by zero and
 * a wrong one would misalign every read, so both are rejected; requiring a NUL
 * last byte in the string table makes any in-range name terminated. The
 * section-header string table is resolved too, since -a names section symbols
 * from it rather than from the symbol string table.
 */
static int elf_load_symtab32(t_file *file, const Elf32_Ehdr *ehdr, t_symtab32 *st)
{
	const Elf32_Shdr *shdrs;
	const Elf32_Shdr *symtab;
	const Elf32_Shdr *strtab;
	const Elf32_Shdr *shstrtab;
	uint16_t i;

	if (ehdr->e_shnum == 0 || ehdr->e_shentsize != sizeof(Elf32_Shdr))
		return (ELF_BAD);
	if (ehdr->e_shoff % _Alignof(Elf32_Shdr) != 0)
		return (ELF_BAD);
	if (!elf_in_range(file, ehdr->e_shoff, (uint64_t)ehdr->e_shnum * ehdr->e_shentsize))
		return (ELF_BAD);
	shdrs = (const Elf32_Shdr *)((const char *)file->data + ehdr->e_shoff);
	symtab = NULL;
	i = 0;
	while (i < ehdr->e_shnum)
	{
		if (shdrs[i].sh_type == SHT_SYMTAB)
		{
			symtab = &shdrs[i];
			break;
		}
		i++;
	}
	if (!symtab)
		return (ELF_NOSYM);
	if (symtab->sh_entsize != sizeof(Elf32_Sym) || symtab->sh_link >= ehdr->e_shnum)
		return (ELF_BAD);
	if (symtab->sh_offset % _Alignof(Elf32_Sym) != 0)
		return (ELF_BAD);
	if (!elf_in_range(file, symtab->sh_offset, symtab->sh_size))
		return (ELF_BAD);
	strtab = &shdrs[symtab->sh_link];
	if (strtab->sh_size == 0 || !elf_in_range(file, strtab->sh_offset, strtab->sh_size))
		return (ELF_BAD);
	st->shstrtab = NULL;
	st->shstrtab_size = 0;
	if (ehdr->e_shstrndx < ehdr->e_shnum)
	{
		shstrtab = &shdrs[ehdr->e_shstrndx];
		if (shstrtab->sh_size == 0
		    || !elf_in_range(file, shstrtab->sh_offset, shstrtab->sh_size))
			return (ELF_BAD);
		st->shstrtab = (const char *)file->data + shstrtab->sh_offset;
		st->shstrtab_size = shstrtab->sh_size;
		if (st->shstrtab[st->shstrtab_size - 1] != '\0')
			return (ELF_BAD);
	}
	st->shdrs = shdrs;
	st->shnum = ehdr->e_shnum;
	st->symbols = (const Elf32_Sym *)((const char *)file->data + symtab->sh_offset);
	st->count = symtab->sh_size / sizeof(Elf32_Sym);
	st->strtab = (const char *)file->data + strtab->sh_offset;
	st->strtab_size = strtab->sh_size;
	if (st->strtab[st->strtab_size - 1] != '\0')
		return (ELF_BAD);
	return (ELF_OK);
}

/*
 * Section symbols carry st_name == 0; their name lives in the section header
 * string table instead. Returns NULL when it cannot be resolved in range, so
 * the caller drops the symbol rather than printing past the mapping.
 */
static const char *section_name32(const t_symtab32 *st, const Elf32_Sym *sym)
{
	uint32_t off;

	if (!st->shstrtab || sym->st_shndx >= st->shnum)
		return (NULL);
	off = st->shdrs[sym->st_shndx].sh_name;
	if (off >= st->shstrtab_size || st->shstrtab[off] == '\0')
		return (NULL);
	return (st->shstrtab + off);
}

/*
 * Index 0 is the reserved null entry and is skipped by position before this
 * runs, so an empty name here is a real symbol -- under -a nm prints the
 * unnamed STT_FILE entry the linker leaves behind.
 *
 * -g keeps every external symbol (including undefined and weak ones) and -u
 * every undefined one, so both must test the symbol itself. Deciding from the
 * printed letter instead would drop U, w and v from -g. Neither the source
 * file entry nor section symbols are listed unless -a asks for them.
 */
static bool symbol_listed32(const t_symtab32 *st, const Elf32_Sym *sym, t_nm *nm)
{
	uint8_t type;

	type = ELF32_ST_TYPE(sym->st_info);
	if (type == STT_SECTION)
	{
		if (!nm->all_symbols || !section_name32(st, sym))
			return (false);
	}
	else if (sym->st_name >= st->strtab_size)
		return (false);
	else if (type == STT_FILE)
	{
		if (!nm->all_symbols)
			return (false);
	}
	else if (sym->st_name == 0 || st->strtab[sym->st_name] == '\0')
		return (false);
	if (nm->global_only && ELF32_ST_BIND(sym->st_info) == STB_LOCAL)
		return (false);
	if (nm->undefined_only && sym->st_shndx != SHN_UNDEF)
		return (false);
	return (true);
}

static int collect_symbols32(const t_symtab32 *st, t_nm *nm, t_symbol **out)
{
	t_symbol *list;
	uint64_t count;
	uint64_t i;
	uint64_t j;

	count = 0;
	i = 1;
	while (i < st->count)
	{
		if (symbol_listed32(st, &st->symbols[i], nm))
			count++;
		i++;
	}
	*out = NULL;
	if (count == 0)
		return (0);
	list = malloc(sizeof(t_symbol) * count);
	if (!list)
		return (-1);
	i = 1;
	j = 0;
	while (i < st->count)
	{
		if (symbol_listed32(st, &st->symbols[i], nm))
		{
			list[j].addr = st->symbols[i].st_value;
			if (ELF32_ST_TYPE(st->symbols[i].st_info) == STT_SECTION)
				list[j].name = section_name32(st, &st->symbols[i]);
			else
				list[j].name = st->strtab + st->symbols[i].st_name;
			list[j].type = get_symbol_type32(&st->symbols[i], st->shdrs, st->shnum);
			j++;
		}
		i++;
	}
	*out = list;
	return ((int)count);
}

/*
 * Only undefined symbols print blanks in the value column; V, W and u carry an
 * address.
 */
static void print_symbols32(const t_symbol *symbols, int count)
{
	int i;

	i = 0;
	while (i < count)
	{
		if (symbols[i].type == 'U' || symbols[i].type == 'w' || symbols[i].type == 'v')
			printf("%8c %c %s\n", ' ', symbols[i].type, symbols[i].name);
		else
			printf("%08" PRIx32 " %c %s\n", (uint32_t)symbols[i].addr, symbols[i].type,
			       symbols[i].name);
		i++;
	}
}

/*
 * Drives one ELF image end to end. An absent symbol table is a warning, not a
 * failure: nm keeps an exit status of 0 for it.
 */
int elf_process_symbols32(t_nm *nm, t_file *file, const Elf32_Ehdr *ehdr)
{
	t_symtab32 st;
	t_symbol *symbols;
	int count;
	int status;

	status = elf_load_symtab32(file, ehdr, &st);
	if (status == ELF_BAD)
	{
		fprintf(stderr, "ft_nm: %s: file format not recognized\n", file->filename);
		return (-1);
	}
	if (status == ELF_NOSYM)
	{
		fprintf(stderr, "ft_nm: %s: no symbols\n", file->filename);
		return (0);
	}
	count = collect_symbols32(&st, nm, &symbols);
	if (count < 0)
	{
		fprintf(stderr, "ft_nm: %s: out of memory\n", file->filename);
		return (-1);
	}
	if (count > 0 && !nm->no_sort)
	{
		if (nm->reverse_sort)
			qsort(symbols, count, sizeof(t_symbol), symbol_cmp_reverse);
		else
			qsort(symbols, count, sizeof(t_symbol), symbol_cmp);
	}
	print_symbols32(symbols, count);
	free(symbols);
	return (0);
}
