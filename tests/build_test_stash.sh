rm build/*; \
sed -i 's/^param int PATH_LENGTH = [0-9]\+;/param int PATH_LENGTH = 18;/' jasmin/params.jinc && \
jasminc -nowarning -o build/jtree_path.s jasmin/jtree_path.jazz && \
jasminc -nowarning -o build/jbucket.s jasmin/jbucket.jazz && \
jasminc -nowarning -o build/jstash.s jasmin/jstash.jazz && \
gcc -DIS_TEST -Wall -Wextra -g -O3 -fomit-frame-pointer -o build/test_stash src/bucket.c src/tree_path.c src/stash.c build/jbucket.s build/jtree_path.s build/jstash.s tests/test_stash.c && \
./build/test_stash