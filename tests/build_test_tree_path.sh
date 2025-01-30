rm build/*; \
sed -i 's/^param int PATH_LENGTH = [0-9]\+;/param int PATH_LENGTH = 11;/' jasmin/params.jinc && \
jasminc -nowarning -o build/jtree_path.s jasmin/jtree_path.jazz && \
gcc -DIS_TEST -Wall -Wextra -g -O3 -fomit-frame-pointer -o build/test_tree_path src/tree_path.c build/jtree_path.s tests/test_tree_path.c && \
./build/test_tree_path