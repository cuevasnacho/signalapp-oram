JC ?= /home/mpi-sp-it/jasmin/compiler/jasminc
CC ?= gcc
MAKE ?= make

JASMIN_ARGS = -nowarning
C_ARGS = -Wall -Wextra -g -O3 -fomit-frame-pointer

.PHONY: clean run

all: test
	@true

test: $(JC) $(JASMIN_ARGS) -o build/jtree_path.s jasmin/jtree_path.jazz &&\
			$(CC) $(C_ARGS) -o build/test_tree_path src/tree_path.c build.jtree_path.s tests/test_tree_path.c &&\
			./build/test_tree_path

clean:
	rm -rf build/*