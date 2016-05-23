#ifndef P2RANDOM_H_
#define P2RANDOM_H_

#include <stdint.h>
#include <stdlib.h>

typedef struct {
	size_t index;
	uint32_t num[625];
} rand32_t;

rand32_t *rand32_init(uint32_t x);
int32_t *generate(size_t n, rand32_t *gen);
int32_t *generate_sorted_unique(size_t n, rand32_t *gen);

#endif