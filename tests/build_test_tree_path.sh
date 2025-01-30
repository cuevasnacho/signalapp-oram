rm build/*; \
jasminc -nowarning -o build/jtree_path.s jasmin/jtree_path.jazz && \
gcc -Wall -Wextra -g -O3 -fomit-frame-pointer -o build/test_tree_path src/tree_path.c build/jtree_path.s tests/test_tree_path.c -DIS_TEST && \
./build/test_tree_path