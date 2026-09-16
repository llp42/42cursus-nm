/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: ft_nm.h
 *
 * Shared interface: the file mapping and symbol types, the parser status
 * codes, and the prototypes crossing the source files.
 *
 * Author: Leonardo Lopes Pereira
 * Email: lepereir@student.42.fr
 *
 * SPDX-License-Identifier: blessing
 */

#ifndef FT_NM_H
#define FT_NM_H

#include <elf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Result of locating a symbol table inside an ELF image. */
#define ELF_OK 0
#define ELF_BAD (-1) /* malformed: reject the file */
#define ELF_NOSYM 1  /* well formed, but carries no symbol table */

typedef struct s_symbol
{
	uint64_t addr;
	char type;
	const char *name;
} t_symbol;

typedef struct s_file
{
	void *data;
	size_t size;
	int fd;
	const char *filename;
} t_file;

/*
 * A symbol table whose offsets have all been validated against the mapped file.
 * Everything downstream takes this rather than re-deriving fields from the raw
 * ELF headers, so no unchecked value can reach a dereference.
 */
typedef struct s_symtab32
{
	const Elf32_Sym *symbols;
	uint64_t count;
	const char *strtab;
	uint64_t strtab_size;
	const char *shstrtab;
	uint64_t shstrtab_size;
	const Elf32_Shdr *shdrs;
	uint16_t shnum;
} t_symtab32;

typedef struct s_symtab64
{
	const Elf64_Sym *symbols;
	uint64_t count;
	const char *strtab;
	uint64_t strtab_size;
	const char *shstrtab;
	uint64_t shstrtab_size;
	const Elf64_Shdr *shdrs;
	uint16_t shnum;
} t_symtab64;

typedef struct s_nm
{
	bool all_symbols;    /* -a: also show debugger-only symbols */
	bool global_only;    /* -g: show only external symbols */
	bool undefined_only; /* -u: show only undefined symbols */
	bool reverse_sort;   /* -r: sort in reverse order */
	bool no_sort;        /* -p: do not sort */
	int operand_count;   /* file operands given; more than one labels each file */
} t_nm;

int validate_flags(t_nm *nm);

int file_open(t_file *file, const char *path);
void file_close(t_file *file);
int elf_parse(t_nm *nm, t_file *file);

int elf_process_symbols32(t_nm *nm, t_file *file, const Elf32_Ehdr *ehdr);
int elf_process_symbols64(t_nm *nm, t_file *file, const Elf64_Ehdr *ehdr);

bool is_file_elf(t_file *file);

/* True when [offset, offset + size) lies wholly inside the mapped file. */
bool elf_in_range(const t_file *file, uint64_t offset, uint64_t size);

char get_symbol_type32(const Elf32_Sym *sym, const Elf32_Shdr *shdrs, uint16_t shnum);
char get_symbol_type64(const Elf64_Sym *sym, const Elf64_Shdr *shdrs, uint16_t shnum);
int symbol_cmp(const void *a, const void *b);
int symbol_cmp_reverse(const void *a, const void *b);

#endif
