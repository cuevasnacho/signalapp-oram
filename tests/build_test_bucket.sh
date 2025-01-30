rm build/*; \
jasminc -nowarning -o build/jtree_path.s jasmin/jtree_path.jazz && \
jasminc -nowarning -o build/jbucket.s jasmin/jbucket.jazz && \
gcc -Wall -Wextra -g -O3 -fomit-frame-pointer -o build/test_bucket src/bucket.c src/tree_path.c build/jbucket.s build/jtree_path.s tests/test_bucket.c -DIS_TEST && \
./build/test_bucket