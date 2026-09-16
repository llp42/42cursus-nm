/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: elf_symbols64.c
 *
 * Symbol handling for ELF64 images: locates the symbol table and its
 * string table, filters entries per the -g/-u flags, classifies
 * them, sorts them and prints the result. Mirror of elf_symbols32.c.
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
static int elf_load_symtab64(t_file *file, const Elf64_Ehdr *ehdr, t_symtab64 *st)
{
	const Elf64_Shdr *shdrs;
	const Elf64_Shdr *symtab;
	const Elf64_Shdr *strtab;
	const Elf64_Shdr *shstrtab;
	uint16_t i;

	if (ehdr->e_shnum == 0 || ehdr->e_shentsize != sizeof(Elf64_Shdr))
		return (ELF_BAD);
	if (ehdr->e_shoff % _Alignof(Elf64_Shdr) != 0)
		return (ELF_BAD);
	if (!elf_in_range(file, ehdr->e_shoff, (uint64_t)ehdr->e_shnum * ehdr->e_shentsize))
		return (ELF_BAD);
	shdrs = (const Elf64_Shdr *)((const char *)file->data + ehdr->e_shoff);
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
	if (symtab->sh_entsize != sizeof(Elf64_Sym) || symtab->sh_link >= ehdr->e_shnum)
		return (ELF_BAD);
	if (symtab->sh_offset % _Alignof(Elf64_Sym) != 0)
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
		if (shstrtab->sh_size == 0 || !elf_in_range(file, shstrtab->sh_offset, shstrtab->sh_size))
			return (ELF_BAD);
		st->shstrtab = (const char *)file->data + shstrtab->sh_offset;
		st->shstrtab_size = shstrtab->sh_size;
		if (st->shstrtab[st->shstrtab_size - 1] != '\0')
			return (ELF_BAD);
	}
	st->shdrs = shdrs;
	st->shnum = ehdr->e_shnum;
	st->symbols = (const Elf64_Sym *)((const char *)file->data + symtab->sh_offset);
	st->count = symtab->sh_size / sizeof(Elf64_Sym);
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
static const char *section_name64(const t_symtab64 *st, const Elf64_Sym *sym)
{
	uint64_t off;

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
static bool symbol_listed64(const t_symtab64 *st, const Elf64_Sym *sym, t_nm *nm)
{
	uint8_t type;

	type = ELF64_ST_TYPE(sym->st_info);
	if (type == STT_SECTION)
	{
		if (!nm->all_symbols || !section_name64(st, sym))
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
	if (nm->global_only && ELF64_ST_BIND(sym->st_info) == STB_LOCAL)
		return (false);
	if (nm->undefined_only && sym->st_shndx != SHN_UNDEF)
		return (false);
	return (true);
}

/*
 * Order follows bfd_decode_symclass: common and undefined first, weak next,
 * section lookup last -- reordering turns a defined weak object (V) into an
 * undefined one (v). Only section-derived letters and A lowercase for locals.
 */
static char get_symbol_type64(const Elf64_Sym *sym, const Elf64_Shdr *shdrs, uint16_t shnum)
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

static int collect_symbols64(const t_symtab64 *st, t_nm *nm, t_symbol **out)
{
	t_symbol *list;
	uint64_t count;
	uint64_t i;
	uint64_t j;

	count = 0;
	i = 1;
	while (i < st->count)
	{
		if (symbol_listed64(st, &st->symbols[i], nm))
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
		if (symbol_listed64(st, &st->symbols[i], nm))
		{
			list[j].addr = st->symbols[i].st_value;
			if (ELF64_ST_TYPE(st->symbols[i].st_info) == STT_SECTION)
				list[j].name = section_name64(st, &st->symbols[i]);
			else
				list[j].name = st->strtab + st->symbols[i].st_name;
			list[j].type = get_symbol_type64(&st->symbols[i], st->shdrs, st->shnum);
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
static void print_symbols64(const t_symbol *symbols, int count)
{
	int i;

	i = 0;
	while (i < count)
	{
		if (symbols[i].type == 'U' || symbols[i].type == 'w' || symbols[i].type == 'v')
			printf("%16c %c %s\n", ' ', symbols[i].type, symbols[i].name);
		else
			printf("%016" PRIx64 " %c %s\n", symbols[i].addr, symbols[i].type, symbols[i].name);
		i++;
	}
}

/*
 * Drives one ELF image end to end. An absent symbol table is a warning, not a
 * failure: nm keeps an exit status of 0 for it.
 */
int elf_process_symbols64(t_nm *nm, t_file *file, const Elf64_Ehdr *ehdr)
{
	t_symtab64 st;
	t_symbol *symbols;
	int count;
	int status;

	status = elf_load_symtab64(file, ehdr, &st);
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
	count = collect_symbols64(&st, nm, &symbols);
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
	print_symbols64(symbols, count);
	free(symbols);
	return (0);
}
