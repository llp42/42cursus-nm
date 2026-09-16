*This project has been created as part of the 42 curriculum by lepereir.*

# ft_nm

## Description

A reimplementation of the Unix `nm` command: it lists the symbols held in an
ELF binary's symbol table — the functions and variables an object defines, and
the ones it still expects from elsewhere.

The project parses ELF directly, without `libelf` or `libbfd`, and reproduces
the system `nm`'s output exactly: same symbol selection, type letters, ordering
and padding. Input is untrusted, so every offset and length read from the file
is bounds-checked against the mapping before it is followed; corrupted objects
are rejected with a message rather than crashing.

Handles ELF32 and ELF64 — executables, object files and shared libraries.
`.a` archives are outside the subject's scope and are reported as
`file format not recognized`.

## Instructions

```bash
make all     # compile (default target)
make clean   # remove the object files
make fclean  # remove the object files and the binary
make re      # fclean, then all
```

Only a C compiler and the standard library are needed. Builds with
`-Wall -Wextra -Werror`.

```console
$ ./ft_nm [option(s)] [file(s)]      # no operand reads a.out

$ ./ft_nm ft_nm
0000000000404e08 d _DYNAMIC
00000000004034a8 R _IO_stdin_used
00000000004024ed T main
                 U printf@GLIBC_2.2.5
```

Each line is `[value] [type] [name]`; undefined symbols print spaces in place
of a value. Most system binaries are stripped and will print `no symbols`.

## Options

| Flag | Description                     |
|------|---------------------------------|
| `-a` | Also show debugger-only symbols |
| `-g` | Only external symbols           |
| `-u` | Only undefined symbols          |
| `-r` | Reverse the sort order          |
| `-p` | Do not sort; symbol-table order |

Combinable (`-gp`), accepted before or after the operands. `-p` overrides `-r`.
`-a` adds section and source-file entries; section symbols are named from the
section header string table, since their own `st_name` is zero.

## Symbol types

| Char | Meaning          | Char | Meaning                 |
|------|------------------|------|-------------------------|
| `A`  | Absolute         | `U`  | Undefined               |
| `B`  | BSS              | `V`  | Weak object (defined)   |
| `C`  | Common           | `W`  | Weak symbol (defined)   |
| `D`  | Initialized data | `v`  | Weak object (undefined) |
| `N`  | Debug            | `w`  | Weak symbol (undefined) |
| `R`  | Read-only data   | `i`  | GNU IFUNC               |
| `T`  | Text             | `u`  | GNU unique              |
| `?`  | Unknown          |      |                         |

Lowercase marks a local symbol.

## Errors

A bad file or format prints to `stderr` and the remaining operands are still
processed; an invalid option exits. A missing symbol table is a warning and
still exits 0, matching `nm`.

## How to test

No suite is included; `ft_nm` is checked against the system `nm`.

```bash
# one file
diff <(nm ft_nm) <(./ft_nm ft_nm) && echo match

# every flag, over any unstripped ELF files
for f in /usr/lib64/crt*.o ./ft_nm; do
  for fl in -a -g -u -r -p -agru ""; do
    diff <(nm $fl "$f" 2>/dev/null) <(./ft_nm $fl "$f" 2>/dev/null) >/dev/null \
      || echo "MISMATCH [$fl] $f"
  done
done
```

Compare stdout only: the `nm:`/`ft_nm:` prefix on `stderr` differs by design.

Malformed input must be rejected, never crash:

```bash
head -c 64 /usr/lib64/crt1.o > /tmp/trunc.o && ./ft_nm /tmp/trunc.o
./ft_nm -z ft_nm                       # invalid option
```

Some out-of-bounds reads land in `mmap` page padding and exit cleanly, so
sanitizers are worth running over truncated and byte-corrupted objects:

```bash
clang -Wall -Wextra -Werror -fsanitize=address,undefined -g -o ft_nm_asan src/*.c
```

Use whichever of `clang` or `gcc` can link the sanitizer runtimes; they are
packaged separately and often present for only one.

## Technical choices

- The file is `mmap`ed read-only and never copied, so "is this offset inside
  the file?" is the one check that must be right. `elf_in_range()` subtracts
  rather than adds, so a hostile offset cannot wrap the comparison.
- The 32- and 64-bit readers are separate files. `Elf32_Sym` and `Elf64_Sym`
  differ in field *order*, not just width, so sharing code would mean a
  conditional at every access; two plain readers are easier to audit.
- `main()` calls `setlocale(LC_ALL, "")` and sorting uses `strcoll()`, as GNU
  `nm` does. A C program starts in the `"C"` locale, where `strcoll()` is just
  `strcmp()`, so without the `setlocale()` call the sort would silently stay
  byte-ordered and diverge from `nm` in any collating locale.

## Resources

- `man 1 nm`, `man 5 elf`, `man 2 mmap`
- `/usr/include/elf.h` — the authoritative structures and constants
- [TIS ELF Specification v1.2](https://refspecs.linuxfoundation.org/elf/elf.pdf)
  and the [System V ABI](https://refspecs.linuxfoundation.org/) x86-64 supplement
- GNU binutils (`binutils/nm.c`, BFD) — reference for symbol selection and
  type letters
- *Linkers and Loaders*, John R. Levine — symbol tables and relocation

### Use of AI

AI (Claude) was used in three areas:

- **Tests** — a differential harness comparing output against the system `nm`
  across binaries and flag combinations, plus fuzzing with corrupted ELF files
  under ASan/UBSan. Used to check the work, not part of the submission.
- **Document review** — editing this README and the source comments.
- **Explanations of `nm` internals** — behaviour the man page leaves implicit:
  how the type letter derives from section flags, why section symbols carry
  `st_name == 0`, and why symbol index 0 is skipped by position.

The implementation is my own. Every explanation was checked against the ELF
specification and the observed behaviour of `nm` before being acted on.

## Author

Leonardo Lopes Pereira <lepereir@student.42.fr>
