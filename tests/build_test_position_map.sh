rm build/*; \
jasminc -nowarning -o build/jtree_path.s jasmin/jtree_path.jazz && \
jasminc -nowarning -o build/jbucket.s jasmin/jbucket.jazz && \
jasminc -nowarning -o build/jstash.s jasmin/jstash.jazz && \
jasminc -nowarning -o build/jpath_oram.s jasmin/jpath_oram.jazz && \
jasminc -nowarning -o build/jposition_map.s jasmin/jposition_map.jazz && \
gcc -DIS_TEST -Wall -Wextra -g -O3 -fomit-frame-pointer -o build/test_position_map src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c build/jtree_path.s build/jbucket.s build/jstash.s build/jpath_oram.s build/jposition_map.s tests/test_position_map.c && \
./build/test_position_map