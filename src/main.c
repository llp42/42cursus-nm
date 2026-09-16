/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: main.c
 *
 * Entry point: parses the flag operands, validates their combinations,
 * then walks the file operands and prints a name banner when more
 * than one is given.
 *
 * Author: Leonardo Lopes Pereira
 * Email: lepereir@student.42.fr
 *
 * SPDX-License-Identifier: blessing
 */

#include "ft_nm.h"

#include <locale.h>
#include <stdio.h>
#include <string.h>

/*
 * -p means "do not sort", which leaves -r nothing to reverse.
 */
int validate_flags(t_nm *nm)
{
	if (nm->no_sort && nm->reverse_sort)
		nm->reverse_sort = false;
	return (0);
}

static bool is_flag(const char *arg)
{
	return (arg[0] == '-' && arg[1] != '\0');
}

/* nm accepts options anywhere on the command line, including after operands. */
static int parse_flags(int argc, char **argv, t_nm *nm)
{
	int i;
	int j;

	i = 1;
	while (i < argc)
	{
		j = 1;
		while (is_flag(argv[i]) && argv[i][j])
		{
			if (argv[i][j] == 'a')
				nm->all_symbols = true;
			else if (argv[i][j] == 'g')
				nm->global_only = true;
			else if (argv[i][j] == 'u')
				nm->undefined_only = true;
			else if (argv[i][j] == 'r')
				nm->reverse_sort = true;
			else if (argv[i][j] == 'p')
				nm->no_sort = true;
			else
			{
				fprintf(stderr, "ft_nm: invalid option -- '%c'\n", argv[i][j]);
				return (-1);
			}
			j++;
		}
		i++;
	}
	return (0);
}

/*
 * With more than one operand nm labels each one. Only files it can actually
 * read get a label.
 */
static int process_file(const char *path, t_nm *nm, bool show_name)
{
	t_file file;
	int ret;

	if (file_open(&file, path) < 0)
		return (1);
	if (show_name && is_file_elf(&file))
		printf("\n%s:\n", path);
	ret = elf_parse(nm, &file);
	file_close(&file);
	return (ret);
}

static int count_operands(int argc, char **argv)
{
	int i;
	int n;

	i = 1;
	n = 0;
	while (i < argc)
	{
		if (!is_flag(argv[i]))
			n++;
		i++;
	}
	return (n);
}

int main(int argc, char **argv)
{
	t_nm nm;
	int i;
	int ret;
	bool has_operand;
	bool show_name;

	memset(&nm, 0, sizeof(t_nm));
	setlocale(LC_ALL, "");
	if (parse_flags(argc, argv, &nm) < 0)
		return (1);
	if (validate_flags(&nm) < 0)
		return (1);
	ret = 0;
	show_name = count_operands(argc, argv) > 1;
	has_operand = false;
	i = 1;
	while (i < argc)
	{
		if (!is_flag(argv[i]))
		{
			has_operand = true;
			if (process_file(argv[i], &nm, show_name) != 0)
				ret = 1;
		}
		i++;
	}
	if (!has_operand && process_file("a.out", &nm, false) != 0)
		ret = 1;
	return (ret);
}
