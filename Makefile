ifeq ($(HOSTTYPE),)
HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

SRCD         = src/
OBJD         = ./obj/
BONUS_OBJD   = ./obj_bonus/
BUILDD       = ./build/
NAME         = $(BUILDD)libft_malloc_$(HOSTTYPE).so
SYM_LINK     = $(BUILDD)libft_malloc.so
BONUS_NAME   = $(BUILDD)libft_malloc_$(HOSTTYPE)_bonus.so
BONUS_SYM_LINK = $(BUILDD)libft_malloc_bonus.so
VERSION_SCRIPT = malloc.version

# Source files
SRCS = $(SRCD)main/src/alloc_manager.c \
       $(SRCD)main/src/free.c \
       $(SRCD)main/src/malloc.c \
       $(SRCD)main/src/realloc.c \
	   $(SRCD)main/src/show_alloc_mem.c \
	   $(SRCD)main/src/show_alloc_mem_ex.c \
       $(SRCD)print_utils/src/print_utils.c \
       $(SRCD)ZoneAllocatorLarge/src/zone_allocator_large.c \
       $(SRCD)ZoneAllocatorSmall/src/zone_allocator_small.c \
       $(SRCD)ZoneAllocatorTiny/src/zone_allocator_tiny.c

# Object files (preserve directory structure)
OBJS       := $(patsubst $(SRCD)%.c,$(OBJD)%.o,$(SRCS))
BONUS_OBJS := $(patsubst $(SRCD)%.c,$(BONUS_OBJD)%.o,$(SRCS))

INCLUDES = -I $(SRCD)main/inc_pub \
		   -I $(SRCD)main/inc_priv \
           -I $(SRCD)print_utils/inc_pub \
           -I $(SRCD)ZoneAllocatorLarge/inc_pub \
           -I $(SRCD)ZoneAllocatorSmall/inc_pub \
           -I $(SRCD)ZoneAllocatorTiny/inc_pub

CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -fPIC $(INCLUDES)
LDFLAGS  = -shared -ldl
LDVER    = -Wl,--version-script=$(VERSION_SCRIPT)


# Default target
all: $(NAME) $(SYM_LINK)

# Bonus target
bonus: $(BONUS_NAME) $(BONUS_SYM_LINK)

# Main library
$(NAME): $(OBJS) $(VERSION_SCRIPT) | $(BUILDD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDVER)

# Bonus library
$(BONUS_NAME): $(BONUS_OBJS) $(VERSION_SCRIPT) | $(BUILDD)
	$(CC) $(CFLAGS) -DFT_BONUS $(LDFLAGS) -o $@ $(BONUS_OBJS) $(LDVER)

# Symbolic links
$(SYM_LINK): $(NAME)
	ln -sf $(notdir $<) $@

$(BONUS_SYM_LINK): $(BONUS_NAME)
	ln -sf $(notdir $<) $@

# Ensure build directory exists
$(BUILDD):
	mkdir -p $@

# Object rule with automatic directory creation
$(OBJD)%.o: $(SRCD)%.c | $(OBJD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BONUS_OBJD)%.o: $(SRCD)%.c | $(BONUS_OBJD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DFT_BONUS -c $< -o $@

# Ensure obj directories exist
$(OBJD):
	mkdir -p $@

$(BONUS_OBJD):
	mkdir -p $@

# Clean rules
clean:
	rm -rf $(OBJD) $(BONUS_OBJD)

fclean: clean
	rm -rf $(BUILDD)

re: fclean all

.PHONY: all bonus clean fclean re