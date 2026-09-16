NAME     = ft_nm

CC       = cc
CFLAGS   = -Wall -Wextra -Werror

HEADERS  = src/ft_nm.h

SRCS     = src/elf_parser.c \
           src/elf_symbols32.c \
           src/elf_symbols64.c \
           src/elf_utils.c \
           src/file.c \
           src/main.c
OBJS     = $(SRCS:.c=.o)

all: $(NAME)

$(OBJS): $(HEADERS)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(NAME) $(OBJS)

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
