all:
	gcc -std=c99 -msse4.2 -msse4a -pedantic -O2 -O3 -flto -march=native -o build main.c p2random.c tree.c
