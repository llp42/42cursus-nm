*This project has been created as part of the 42 curriculum by lepereir.*

# ft_nm

## Description

`ft_nm` is a reimplementation of the Unix `nm` command: it lists the symbols
held in an ELF binary's symbol table.

Every compiled object carries a table naming the functions and variables it
defines and the ones it still expects from elsewhere. `nm` is the tool that
prints that table, and reading it is how you answer questions like "why does
the linker say this symbol is undefined?" or "is this function actually in this
library?".

The goal of the project is to parse the ELF format directly — no `libelf`, no
`libbfd` — from the file header down to the individual symbol entries, and to
reproduce the system `nm`'s output exactly: same symbol selection, same type
letters, same ordering, same padding. The input is untrusted, so every offset
and length read from the file is validated against the mapped region before it
is followed; a truncated or deliberately corrupted object must be rejected with
a message rather than crash the program.

Supported inputs are ELF32 and ELF64: executables, object files (`.o`) and
shared libraries (`.so`). `.a` archives are outside the scope defined by the
subject and are reported as `file format not recognized`.

## Instructions

Build with `make`:

```bash
make        # compile
make clean  # remove object files
make fclean # remove object files and binary
make re     # recompile everything
```

The build needs only a C compiler and the standard library. It compiles with
`-Wall -Wextra -Werror`.

Run it on any ELF file:

```bash
./ft_nm [option(s)] [file(s)]
```

With no operand it reads `a.out`. Each output line is `[value] [type] [name]`;
undefined symbols print spaces where the value would be.

```console
$ ./ft_nm ft_nm
0000000000404e08 d _DYNAMIC
0000000000404fe8 d _GLOBAL_OFFSET_TABLE_
00000000004034a8 R _IO_stdin_used
...
00000000004024ed T main
...
                 U printf@GLIBC_2.2.5
```

(excerpt; symbols are sorted by name)

Note that most system binaries on recent distributions are stripped, so they
have no symbol table to print. Use object files, or binaries you compiled
yourself, to see meaningful output.

## Options

| Flag | Description                     |
|------|---------------------------------|
| `-a` | Also show debugger-only symbols |
| `-g` | Only external symbols           |
| `-u` | Only undefined symbols          |
| `-r` | Reverse the sort order          |
| `-p` | Do not sort; symbol-table order |

Flags may be combined (`-gp`) and may appear before or after the file operands.
An unknown flag is an error.

`-p` overrides `-r`, since there is no order left to reverse. `-g` and `-u`
combine to select external undefined symbols. `-a` adds the section and
source-file entries that are hidden by default; section symbols are named from
the section header string table, because their own `st_name` is zero.

## Symbol types

| Char | Meaning                  |
|------|--------------------------|
| `A`  | Absolute                 |
| `B`  | BSS                      |
| `C`  | Common                   |
| `D`  | Initialized data         |
| `N`  | Debug                    |
| `R`  | Read-only data           |
| `T`  | Text                     |
| `U`  | Undefined                |
| `V`  | Weak object (defined)    |
| `W`  | Weak symbol (defined)    |
| `v`  | Weak object (undefined)  |
| `w`  | Weak symbol (undefined)  |
| `i`  | GNU IFUNC                |
| `u`  | GNU unique               |
| `?`  | Unknown                  |

A lowercase letter marks a local (non-external) symbol.

## Error handling

- An unreadable file or unknown format prints to `stderr`; the remaining
  operands are still processed.
- An invalid option prints to `stderr` and exits.
- A file with no symbol table is a warning, not a failure: the exit status
  stays 0, matching `nm`.
- Corrupted objects are rejected with a message. Every offset, length and
  string-table index taken from the file is bounds-checked against the mapping
  first, so malformed input cannot lead to a read outside it.

## How to test

There is no test suite in the repository; `ft_nm` is verified by comparing it
against the system `nm`, which is the reference the subject asks it to match.

`LC_ALL=C` matters and must be set on **both** commands. GNU `nm` sorts with
`strcoll()`, so in a UTF-8 locale it orders symbols case-insensitively and
ignores punctuation, while `ft_nm` sorts with `strcmp()`. Setting the locale on
the surrounding `diff` instead of on each command is not enough and will show
spurious differences.

Compare a single file:

```bash
diff <(LC_ALL=C nm ft_nm) <(LC_ALL=C ./ft_nm ft_nm) && echo match
```

Compare every flag combination over a set of files:

```bash
for f in /usr/lib64/crt*.o ./ft_nm; do
  for fl in -a -g -u -r -p -agru ""; do
    diff <(LC_ALL=C nm $fl "$f" 2>/dev/null) \
         <(LC_ALL=C ./ft_nm $fl "$f" 2>/dev/null) >/dev/null \
      || echo "MISMATCH [$fl] $f"
  done
done
```

Widen it to whatever unstripped ELF files the system has — `/usr/lib64/*.o`,
`/usr/lib/*.so.*`, anything you compiled yourself. Note that `nm` and `ft_nm`
differ deliberately in their `stderr` prefix (`nm:` versus `ft_nm:`), which the
subject permits, so compare standard output only.

Check that malformed input is rejected rather than crashing:

```bash
head -c 64 /usr/lib64/crt1.o > /tmp/trunc.o   # truncated ELF
./ft_nm /tmp/trunc.o; echo "exit $?"          # message, exit 1, no signal
printf 'not an elf file' > /tmp/plain.txt
./ft_nm /tmp/plain.txt /usr/lib64/crt1.o      # bad file, then keeps going
./ft_nm -z ft_nm                              # invalid option
```

A file with no symbol table is a warning and still exits 0, matching `nm`.

For memory errors that a normal build hides — several out-of-bounds reads land
inside `mmap` page padding and exit cleanly — build with sanitizers:

```bash
clang -Wall -Wextra -Werror -fsanitize=address,undefined -g -o ft_nm_asan src/*.c
./ft_nm_asan -a /usr/lib64/crt1.o
```

Use whichever of `clang` or `gcc` can link `-fsanitize=address,undefined` on
your machine; the runtime libraries are packaged separately from the compiler
and are often installed for only one of the two.

Running that over truncated and byte-corrupted copies of a valid object is the
most effective check on the bounds validation.

## Technical choices

The file is `mmap`ed read-only and never copied; all parsing reads through that
single mapping, which makes "is this offset inside the file?" the one check
that has to be right. `elf_in_range()` is that check, and it subtracts rather
than adds so a hostile offset cannot wrap the comparison.

The 32- and 64-bit paths are kept as separate source files rather than unified
behind macros. `Elf32_Sym` and `Elf64_Sym` differ in field order, not just
width, so a shared implementation would need conditionals at every access; two
explicit readers are longer but each one is straightforward to audit.

Symbol sorting uses `strcmp()`, which matches GNU `nm` under `LC_ALL=C`. GNU
`nm` itself sorts with `strcoll()`, so in another locale its order is
locale-dependent and will differ.

## Resources

Reference material used for the ELF format and `nm`'s behaviour:

- `man 1 nm` and `man 5 elf`
- `/usr/include/elf.h` — the authoritative structure and constant definitions
  on the target system
- [Tool Interface Standard (TIS) Executable and Linking Format (ELF)
  Specification, v1.2](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- [System V ABI — Linux Extensions](https://refspecs.linuxfoundation.org/),
  and the x86-64 psABI supplement for the 64-bit layout
- GNU binutils sources (`binutils/nm.c` and the BFD symbol handling), as the
  reference for which symbols are listed and which type letter each receives
- *Linkers and Loaders*, John R. Levine — background on symbol tables,
  relocation and the role of the linker
- `man 2 mmap`, `man 2 fstat` — file mapping and size handling

### Use of AI

AI (Claude) was used as an assistant on this project, in three areas:

- **Tests.** Building a differential harness that compared `ft_nm`'s output
  against the system `nm` across system binaries, object files and shared
  libraries under every flag combination, and fuzzing the parser with truncated
  and byte-corrupted ELF files under AddressSanitizer and
  UndefinedBehaviorSanitizer. This was used to check the implementation; it is
  not part of the submitted code.
- **Document review.** Reviewing and editing this README and the explanatory
  comments in the source files for accuracy and clarity.
- **Explanations of `nm` internals.** Clarifying points of behaviour that the
  manual page leaves implicit — how the type letter is derived from a section's
  flags, why section symbols carry an `st_name` of zero and must be named from
  the section header string table, and why the reserved symbol at index 0 is
  skipped by position rather than by its empty name.

The implementation is my own. Every explanation was checked against the ELF
specification and the observed behaviour of the system `nm` before being acted
on, and I can explain and defend each part of the design.

## Author

Leonardo Lopes Pereira <lepereir@student.42.fr>
