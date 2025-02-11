JC ?= jasminc
CC ?= gcc
MAKE ?= make

JFLAGS = -nowarning
CFLAGS = -DIS_TEST -Wall -Wextra -g -O3 -fomit-frame-pointer

.PHONY: clean run

# test commands
test-tree_path: build/test_tree_path
	./build/test_tree_path

test-bucket: build/test_bucket
	./build/test_bucket

test-stash: build/test_stash
	./build/test_stash

test-oram: build/test_path_oram
	./build/test_path_oram


# build tests
build/test_tree_path: setparams-tree_path clean src/tree_path.c build/jtree_path.s tests/test_tree_path.c
	$(CC) $(CFLAGS) -o build/test_tree_path src/tree_path.c build/jtree_path.s tests/test_tree_path.c

build/test_bucket: setparams-bucket clean src/bucket.c src/tree_path.c build/jtree_path.s build/jbucket.s tests/test_bucket.c
	$(CC) $(CFLAGS) -o build/test_bucket src/bucket.c src/tree_path.c build/jtree_path.s build/jbucket.s tests/test_bucket.c

build/test_stash: setparams-stash clean src/bucket.c src/tree_path.c src/stash.c build/jtree_path.s build/jbucket.s build/jstash.s tests/test_stash.c
	$(CC) $(CFLAGS) -o build/test_stash src/bucket.c src/tree_path.c src/stash.c build/jtree_path.s build/jbucket.s build/jstash.s tests/test_stash.c

build/test_path_oram: setparams-oram clean src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c build/jtree_path.s build/jbucket.s build/jstash.s build/jposition_map.s build/jpath_oram.s tests/test_path_oram.c
	$(CC) $(CFLAGS) -o build/test_path_oram src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c build/jtree_path.s build/jbucket.s build/jstash.s build/jposition_map.s build/jpath_oram.s tests/test_path_oram.c

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
