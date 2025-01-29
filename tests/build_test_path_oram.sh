rm build/*; \
/home/mpi-sp-it/jasmin/compiler/jasminc -nowarning -o build/jtree_path.s jasmin/jtree_path.jazz && \
/home/mpi-sp-it/jasmin/compiler/jasminc -nowarning -o build/jbucket.s jasmin/jbucket.jazz && \
/home/mpi-sp-it/jasmin/compiler/jasminc -nowarning -o build/jstash.s jasmin/jstash.jazz && \
gcc -Wall -Wextra -g -O3 -fomit-frame-pointer -o build/test_path_oram src/bucket.c src/tree_path.c src/stash.c src/path_oram.c src/position_map.c build/jtree_path.s build/jbucket.s build/jstash.s tests/test_path_oram.c && \
./build/test_path_oram