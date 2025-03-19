JC ?= /home/mpi-sp-it/Desktop/icuevas/jasmin/jasmin-2025.02.0/compiler/jasminc
CC ?= gcc
MAKE ?= make

JFLAGS = -nowarning
CTEST  = -DIS_TEST
CFLAGS = -Wall -Wextra -g -O3 -fomit-frame-pointer

.PHONY: clean run

# test commands
tree_path: build/test_tree_path
	./build/test_tree_path

bucket: build/test_bucket
	./build/test_bucket

stash: build/test_stash
	./build/test_stash

position_map: build/test_position_map
	./build/test_position_map

oram: build/test_path_oram
	./build/test_path_oram

ct_div: build/test_ct_div
	./build/test_ct_div

speed: build/test_speed
	./build/test_speed


# build tests
build/test_tree_path: src/tree_path.c build/jtree_path.s tests/test_tree_path.c
	$(CC) $(CTEST) $(CFLAGS) -o build/test_tree_path src/tree_path.c build/jtree_path.s tests/test_tree_path.c

build/test_bucket: src/bucket.c src/tree_path.c build/jtree_path.s build/jbucket.s tests/test_bucket.c
	$(CC) $(CTEST) $(CFLAGS) -o build/test_bucket src/bucket.c src/tree_path.c build/jtree_path.s build/jbucket.s tests/test_bucket.c

build/test_stash: src/bucket.c src/tree_path.c src/stash.c build/jtree_path.s build/jbucket.s build/jstash.s tests/test_stash.c
	$(CC) $(CTEST) $(CFLAGS) -o build/test_stash src/bucket.c src/tree_path.c src/stash.c build/jtree_path.s build/jbucket.s build/jstash.s tests/test_stash.c

build/test_position_map: src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c src/uint128div.c build/jposition_map.s tests/test_position_map.c syscall/jasmin_syscall.o
	$(CC) $(CFLAGS) -o build/test_position_map src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c src/uint128div.c build/jposition_map.s tests/test_position_map.c syscall/jasmin_syscall.o

build/test_path_oram: src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c src/uint128div.c build/jtree_path.s build/jbucket.s build/jstash.s build/jposition_map.s build/jpath_oram.s tests/test_path_oram.c syscall/jasmin_syscall.o
	$(CC) $(CTEST) $(CFLAGS) -o build/test_path_oram src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c src/uint128div.c build/jtree_path.s build/jbucket.s build/jstash.s build/jposition_map.s build/jpath_oram.s tests/test_path_oram.c syscall/jasmin_syscall.o

build/test_ct_div: build/jutil.s src/uint128div.c tests/test_uint128div.c
	$(CC) $(CFLAGS) -o build/test_ct_div src/uint128div.c build/jutil.s tests/test_uint128div.c

build/test_speed: src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c src/uint128div.c build/jtree_path.s build/jbucket.s build/jstash.s build/jposition_map.s build/jpath_oram.s tests/test_speed.c syscall/jasmin_syscall.o
	$(CC) $(CFLAGS) -o build/test_speed src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c src/uint128div.c build/jtree_path.s build/jbucket.s build/jstash.s build/jposition_map.s build/jpath_oram.s tests/test_speed.c syscall/jasmin_syscall.o

syscall/jasmin_syscall.o:
	$(MAKE) -C syscall

build/%.s: jasmin/%.jazz
	$(JC) $(JFLAGS) -o $@ $<


# set parameters
setparams-tree_path: jasmin/params.jinc
	sed -i 's/^param int PATH_LENGTH = [0-9]\+;/param int PATH_LENGTH = 11;/' jasmin/params.jinc

setparams-bucket: jasmin/params.jinc
	sed -i 's/^param int PATH_LENGTH = [0-9]\+;/param int PATH_LENGTH = 11;/' jasmin/params.jinc

setparams-stash: jasmin/params.jinc
	sed -i 's/^param int PATH_LENGTH = [0-9]\+;/param int PATH_LENGTH = 18;/' jasmin/params.jinc

setparams-oram: jasmin/params.jinc
	sed -i 's/^param int PATH_LENGTH = [0-9]\+;/param int PATH_LENGTH = 13;/' jasmin/params.jinc


# remove all trash files
clean:
	rm -rf build/*