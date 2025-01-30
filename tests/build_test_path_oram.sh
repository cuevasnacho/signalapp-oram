rm build/*; \
sed -i 's/^param int PATH_LENGTH = [0-9]\+;/param int PATH_LENGTH = 13;/' jasmin/params.jinc && \
jasminc -nowarning -o build/jtree_path.s jasmin/jtree_path.jazz && \
jasminc -nowarning -o build/jbucket.s jasmin/jbucket.jazz && \
jasminc -nowarning -o build/jstash.s jasmin/jstash.jazz && \
jasminc -nowarning -o build/jpath_oram.s jasmin/jpath_oram.jazz && \
gcc -DIS_TEST -Wall -Wextra -g -O3 -fomit-frame-pointer -o build/test_path_oram src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c build/jtree_path.s build/jbucket.s build/jstash.s build/jpath_oram.s tests/test_path_oram.c && \
./build/test_path_oram