/*
 * "It's not a bug — it's an undocumented feature."
 *
 * File: file.c
 *
 * Opens an input file and maps it into memory, rejecting anything that
 * cannot be mapped (directories, empty files, read errors) with a
 * message on stderr. Every later stage reads only from this mapping.
 *
 * Author: Leonardo Lopes Pereira
 * Email: lepereir@student.42.fr
 *
 * SPDX-License-Identifier: blessing
 */

#include "ft_nm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int file_fail(t_file *file, const char *msg)
{
	fprintf(stderr, "ft_nm: '%s': %s\n", file->filename, msg);
	if (file->fd >= 0)
		close(file->fd);
	file->fd = -1;
	return (-1);
}

/*
 * Opens a file and maps it read-only. mmap rejects a zero length, so an empty
 * file is left unmapped and the format check rejects it like any other
 * non-object input.
 */
int file_open(t_file *file, const char *path)
{
	struct stat st;

	file->data = NULL;
	file->size = 0;
	file->fd = -1;
	file->filename = path;
	file->fd = open(path, O_RDONLY);
	if (file->fd < 0)
		return (file_fail(file, strerror(errno)));
	if (fstat(file->fd, &st) < 0)
		return (file_fail(file, strerror(errno)));
	if (S_ISDIR(st.st_mode))
		return (file_fail(file, "is a directory"));
	file->size = st.st_size;
	if (file->size == 0)
		return (0);
	file->data = mmap(NULL, file->size, PROT_READ, MAP_PRIVATE, file->fd, 0);
	if (file->data == MAP_FAILED)
	{
		file->data = NULL;
		file->size = 0;
		return (file_fail(file, strerror(errno)));
	}
	return (0);
}

void file_close(t_file *file)
{
	if (file->data)
		munmap(file->data, file->size);
	if (file->fd >= 0)
		close(file->fd);
	file->data = NULL;
	file->size = 0;
	file->fd = -1;
}
