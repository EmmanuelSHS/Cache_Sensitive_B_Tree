#include "tree.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xmmintrin.h>
#include <emmintrin.h>
#include <pmmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#include <nmmintrin.h>
#include <immintrin.h>
#include <ammintrin.h>
#include <x86intrin.h>

extern int posix_memalign(void** memptr, size_t alignment, size_t size);
size_t alignment = 16;

Tree* build_index(size_t num_levels, size_t fanout[], size_t num_keys, int32_t key[]) {
        // return null pointer for invalid tree configuration
        size_t min_num_keys = 1;
        for (size_t i = 0; i < num_levels - 1; ++i) {
                min_num_keys *= fanout[i];
        }
        size_t max_num_keys = min_num_keys * fanout[num_levels - 1] - 1;
        if (num_keys < min_num_keys || num_keys > max_num_keys) {
                fprintf(stderr, "Error: incorrect number of keys, min %zu, max %zu\n", min_num_keys, max_num_keys);
                return NULL;
        }

        // initialize the tree index
        Tree* tree = malloc(sizeof(Tree));
        assert(tree != NULL);
        tree->num_levels = num_levels;
        tree->node_capacity = malloc(sizeof(size_t) * num_levels);
        assert(tree->node_capacity != NULL);
        for (size_t i = 0; i < num_levels; ++i) {
                tree->node_capacity[i] = fanout[i] - 1;
        }
        tree->key_array = malloc(sizeof(int32_t*) * num_levels);
        assert(tree->key_array != NULL);
        size_t* key_count = malloc(sizeof(size_t) * num_levels);
        assert(key_count != NULL);
        size_t* array_capacity = malloc(sizeof(size_t) * num_levels);
        assert(array_capacity != NULL);
        for (size_t i = 0; i < num_levels; ++i) {
                size_t size = sizeof(int32_t) * tree->node_capacity[i];         // allocate one node per level
                int error = posix_memalign((void**) &(tree->key_array[i]), alignment, size);
                assert(error == 0);
                key_count[i] = 0;
                array_capacity[i] = tree->node_capacity[i];     // array_capacity[i] is always a multiple of node_capacity[i]
        }

        // insert sorted keys into index
        for (size_t i = 1; i < num_keys; ++i) {
                assert(key[i - 1] < key[i]);
        }
        for (size_t i = 0; i < num_keys; ++i) {
                size_t level = num_levels - 1;
                while (key_count[level] == array_capacity[level])
                        level -= 1;
                tree->key_array[level][key_count[level]] = key[i];
                key_count[level] += 1;
                while (level < num_levels - 1) {
                        level += 1;
                        size_t new_capacity = array_capacity[level] + tree->node_capacity[level];
                        size_t size = sizeof(int32_t) * new_capacity;           // allocate one more node
                        int32_t* new_array = NULL;
                        int error = posix_memalign((void**) &new_array, alignment, size);
                        assert(error == 0);
                        memcpy(new_array, tree->key_array[level], sizeof(int32_t) * key_count[level]);
                        free(tree->key_array[level]);
                        tree->key_array[level] = new_array;
                        array_capacity[level] = new_capacity;
                }
        }

        // pad with INT32_MAXs
        for (size_t i = 0; i < num_levels; ++i) {
                for (size_t j = key_count[i]; j < array_capacity[i]; ++j)
                        tree->key_array[i][j] = INT32_MAX;
                key_count[i] = array_capacity[i];
        }

        // print the tree
        //for (size_t i = 0; i < num_levels; ++i) {
        //        printf("Level %zu:", i);
        //        for (size_t j = 0; j < key_count[i]; ++j)
        //                printf(" %d", tree->key_array[i][j]);
        //        printf("\n");
        //}

        free(array_capacity);
        free(key_count);
        return tree;
}

uint32_t probe_index_is(Tree* tree, int32_t probe_key) {
        int32_t result = 0;
        int32_t prev_res = 0;

        __m128i key = _mm_cvtsi32_si128(probe_key);
        key = _mm_shuffle_epi32(key, _MM_SHUFFLE(0, 0, 0, 0));
        
        // init 4 searching & corresponding partition range lane
        for (size_t level = 0; level < tree->num_levels; ++level) {
                if (tree->node_capacity[level] == 4) {
                        // load range delimiters
                        __m128i del_ABCD = _mm_load_si128((__m128i *) &tree->key_array[level][prev_res << 2]);

                        // search
                        __m128i cmp_ABCD = _mm_cmpgt_epi32(key, del_ABCD);

                        // extract
                        result = _mm_movemask_ps((__m128) cmp_ABCD);
                        result = _bit_scan_forward(result ^ 0x1F);
                        result += (prev_res << 2) + prev_res;
                        prev_res = result;

                } else if (tree->node_capacity[level] == 8) {
                        __m128i del_ABCD = _mm_load_si128((__m128i *) &tree->key_array[level][prev_res << 3]);
                        __m128i del_EFGH = _mm_load_si128((__m128i *) &tree->key_array[level][(prev_res << 3) + 4]);

                        __m128i cmp_ABCD = _mm_cmpgt_epi32(key, del_ABCD);
                        __m128i cmp_EFGH = _mm_cmpgt_epi32(key, del_EFGH);

                        __m128i cmp_A_to_H = _mm_packs_epi32(cmp_ABCD, cmp_EFGH);
                        cmp_A_to_H = _mm_packs_epi16(cmp_A_to_H, _mm_setzero_si128());

                        result = _mm_movemask_epi8(cmp_A_to_H);
                        result = _bit_scan_forward(result ^ 0x1FF);
                        result += (prev_res << 3) + prev_res;
                        prev_res = result;

                } else if (tree->node_capacity[level] == 16) {
                        __m128i del_ABCD = _mm_load_si128((__m128i *) &tree->key_array[level][prev_res << 4]);
                        __m128i del_EFGH = _mm_load_si128((__m128i *) &tree->key_array[level][(prev_res << 4) + 4]);
                        __m128i del_IJKL = _mm_load_si128((__m128i *) &tree->key_array[level][(prev_res << 4) + 8]);
                        __m128i del_MNOP = _mm_load_si128((__m128i *) &tree->key_array[level][(prev_res << 4) + 12]);

                        __m128i cmp_ABCD = _mm_cmpgt_epi32(key, del_ABCD);
                        __m128i cmp_EFGH = _mm_cmpgt_epi32(key, del_EFGH);
                        __m128i cmp_IJKL = _mm_cmpgt_epi32(key, del_IJKL);
                        __m128i cmp_MNOP = _mm_cmpgt_epi32(key, del_MNOP);

                        __m128i cmp_A_to_H = _mm_packs_epi32(cmp_ABCD, cmp_EFGH);
                        __m128i cmp_I_to_P = _mm_packs_epi32(cmp_IJKL, cmp_MNOP);
                        __m128i cmp_A_to_P = _mm_packs_epi16(cmp_A_to_H, cmp_I_to_P);

                        result = _mm_movemask_epi8(cmp_A_to_P);
                        result = _bit_scan_forward(0x1FFFF ^ result);
                        result += (prev_res << 4) + prev_res;
                        prev_res = result;
                }
        }
        return (uint32_t) result;
}

void probe_index_959(Tree* tree, int32_t* probe_keys, size_t num_probes, uint32_t* result) {
        // preloading level 0
        __m128i del_lvl_0_A = _mm_load_si128((__m128i *) &tree->key_array[0][0]);
        __m128i del_lvl_0_B = _mm_load_si128((__m128i *) &tree->key_array[0][4]);

        for (size_t i = 0; i < num_probes/4; ++i) {
                size_t level = 0;

                int32_t r_0_1  = 0;
                int32_t r_0_2  = 0;
                int32_t r_0_3  = 0;
                int32_t r_0_4  = 0;

                int32_t r_1_1  = 0;
                int32_t r_1_2  = 0;
                int32_t r_1_3  = 0;
                int32_t r_1_4  = 0;

                int32_t r_2_1  = 0;
                int32_t r_2_2  = 0;
                int32_t r_2_3  = 0;
                int32_t r_2_4  = 0;

                // load keys
                //printf("batch starting: %d\n", probe_keys[4 * i]);
                __m128i k = _mm_load_si128((__m128i*) &probe_keys[4 * i]);
                register __m128i k1 = _mm_shuffle_epi32(k, _MM_SHUFFLE(0,0,0,0));
                register __m128i k2 = _mm_shuffle_epi32(k, _MM_SHUFFLE(1,1,1,1));
                register __m128i k3 = _mm_shuffle_epi32(k, _MM_SHUFFLE(2,2,2,2));
                register __m128i k4 = _mm_shuffle_epi32(k, _MM_SHUFFLE(3,3,3,3));

                // level 0, fanout = 9
                __m128i cmp_lvl_0_A_1 = _mm_cmpgt_epi32(k1, del_lvl_0_A);
                __m128i cmp_lvl_0_A_2 = _mm_cmpgt_epi32(k2, del_lvl_0_A);
                __m128i cmp_lvl_0_A_3 = _mm_cmpgt_epi32(k3, del_lvl_0_A);
                __m128i cmp_lvl_0_A_4 = _mm_cmpgt_epi32(k4, del_lvl_0_A);

                __m128i cmp_lvl_0_B_1 = _mm_cmpgt_epi32(k1, del_lvl_0_B);
                __m128i cmp_lvl_0_B_2 = _mm_cmpgt_epi32(k2, del_lvl_0_B);
                __m128i cmp_lvl_0_B_3 = _mm_cmpgt_epi32(k3, del_lvl_0_B);
                __m128i cmp_lvl_0_B_4 = _mm_cmpgt_epi32(k4, del_lvl_0_B);

                __m128i cmp_lvl_0_1 = _mm_packs_epi32(cmp_lvl_0_A_1, cmp_lvl_0_B_1);
                __m128i cmp_lvl_0_2 = _mm_packs_epi32(cmp_lvl_0_A_2, cmp_lvl_0_B_2);
                __m128i cmp_lvl_0_3 = _mm_packs_epi32(cmp_lvl_0_A_3, cmp_lvl_0_B_3);
                __m128i cmp_lvl_0_4 = _mm_packs_epi32(cmp_lvl_0_A_4, cmp_lvl_0_B_4);

                cmp_lvl_0_1 = _mm_packs_epi16(cmp_lvl_0_1, _mm_setzero_si128());
                cmp_lvl_0_2 = _mm_packs_epi16(cmp_lvl_0_2, _mm_setzero_si128());
                cmp_lvl_0_3 = _mm_packs_epi16(cmp_lvl_0_3, _mm_setzero_si128());
                cmp_lvl_0_4 = _mm_packs_epi16(cmp_lvl_0_4, _mm_setzero_si128());

                r_0_1 = _mm_movemask_epi8(cmp_lvl_0_1);
                r_0_2 = _mm_movemask_epi8(cmp_lvl_0_2);
                r_0_3 = _mm_movemask_epi8(cmp_lvl_0_3);
                r_0_4 = _mm_movemask_epi8(cmp_lvl_0_4);

                r_0_1 = _bit_scan_forward(r_0_1 ^ 0x1FF);
                r_0_2 = _bit_scan_forward(r_0_2 ^ 0x1FF);
                r_0_3 = _bit_scan_forward(r_0_3 ^ 0x1FF);
                r_0_4 = _bit_scan_forward(r_0_4 ^ 0x1FF);
                //printf("Level 0: %d %d %d %d\n", r_0_1, r_0_2, r_0_3, r_0_4);

                // level 1, fanout = 5
                __m128i del_lvl_1_1 = _mm_load_si128((__m128i *) &tree->key_array[1][r_0_1 << 2]);
                __m128i del_lvl_1_2 = _mm_load_si128((__m128i *) &tree->key_array[1][r_0_2 << 2]);
                __m128i del_lvl_1_3 = _mm_load_si128((__m128i *) &tree->key_array[1][r_0_3 << 2]);
                __m128i del_lvl_1_4 = _mm_load_si128((__m128i *) &tree->key_array[1][r_0_4 << 2]);

                __m128i cmp_lvl_1_1 = _mm_cmpgt_epi32(k1, del_lvl_1_1);
                __m128i cmp_lvl_1_2 = _mm_cmpgt_epi32(k2, del_lvl_1_2);
                __m128i cmp_lvl_1_3 = _mm_cmpgt_epi32(k3, del_lvl_1_3);
                __m128i cmp_lvl_1_4 = _mm_cmpgt_epi32(k4, del_lvl_1_4);

                r_1_1 = _mm_movemask_ps((__m128) cmp_lvl_1_1);
                r_1_2 = _mm_movemask_ps((__m128) cmp_lvl_1_2);
                r_1_3 = _mm_movemask_ps((__m128) cmp_lvl_1_3);
                r_1_4 = _mm_movemask_ps((__m128) cmp_lvl_1_4);

                r_1_1 = _bit_scan_forward(r_1_1 ^ 0x1F);
                r_1_2 = _bit_scan_forward(r_1_2 ^ 0x1F);
                r_1_3 = _bit_scan_forward(r_1_3 ^ 0x1F);
                r_1_4 = _bit_scan_forward(r_1_4 ^ 0x1F);

                r_1_1 += r_0_1 + (r_0_1 << 2);
                r_1_2 += r_0_2 + (r_0_2 << 2);
                r_1_3 += r_0_3 + (r_0_3 << 2);
                r_1_4 += r_0_4 + (r_0_4 << 2);
                //printf("Level 1: %d %d %d %d\n", r_1_1, r_1_2, r_1_3, r_1_4);
                
                // level 2, fanout = 9
                __m128i del_lvl_2_A_1 = _mm_load_si128((__m128i *) &tree->key_array[2][r_1_1 << 3]);
                __m128i del_lvl_2_A_2 = _mm_load_si128((__m128i *) &tree->key_array[2][r_1_2 << 3]);
                __m128i del_lvl_2_A_3 = _mm_load_si128((__m128i *) &tree->key_array[2][r_1_3 << 3]);
                __m128i del_lvl_2_A_4 = _mm_load_si128((__m128i *) &tree->key_array[2][r_1_4 << 3]);

                __m128i del_lvl_2_B_1 = _mm_load_si128((__m128i *) &tree->key_array[2][(r_1_1 << 3) + 4]);
                __m128i del_lvl_2_B_2 = _mm_load_si128((__m128i *) &tree->key_array[2][(r_1_2 << 3) + 4]);
                __m128i del_lvl_2_B_3 = _mm_load_si128((__m128i *) &tree->key_array[2][(r_1_3 << 3) + 4]);
                __m128i del_lvl_2_B_4 = _mm_load_si128((__m128i *) &tree->key_array[2][(r_1_4 << 3) + 4]);

                __m128i cmp_lvl_2_A_1 = _mm_cmpgt_epi32(k1, del_lvl_2_A_1);
                __m128i cmp_lvl_2_A_2 = _mm_cmpgt_epi32(k2, del_lvl_2_A_2);
                __m128i cmp_lvl_2_A_3 = _mm_cmpgt_epi32(k3, del_lvl_2_A_3);
                __m128i cmp_lvl_2_A_4 = _mm_cmpgt_epi32(k4, del_lvl_2_A_4);

                __m128i cmp_lvl_2_B_1 = _mm_cmpgt_epi32(k1, del_lvl_2_B_1);
                __m128i cmp_lvl_2_B_2 = _mm_cmpgt_epi32(k2, del_lvl_2_B_2);
                __m128i cmp_lvl_2_B_3 = _mm_cmpgt_epi32(k3, del_lvl_2_B_3);
                __m128i cmp_lvl_2_B_4 = _mm_cmpgt_epi32(k4, del_lvl_2_B_4);

                __m128i cmp_lvl_2_1 = _mm_packs_epi32(cmp_lvl_2_A_1, cmp_lvl_2_B_1);
                __m128i cmp_lvl_2_2 = _mm_packs_epi32(cmp_lvl_2_A_2, cmp_lvl_2_B_2);
                __m128i cmp_lvl_2_3 = _mm_packs_epi32(cmp_lvl_2_A_3, cmp_lvl_2_B_3);
                __m128i cmp_lvl_2_4 = _mm_packs_epi32(cmp_lvl_2_A_4, cmp_lvl_2_B_4);

                cmp_lvl_2_1 = _mm_packs_epi16(cmp_lvl_2_1, _mm_setzero_si128());
                cmp_lvl_2_2 = _mm_packs_epi16(cmp_lvl_2_2, _mm_setzero_si128());
                cmp_lvl_2_3 = _mm_packs_epi16(cmp_lvl_2_3, _mm_setzero_si128());
                cmp_lvl_2_4 = _mm_packs_epi16(cmp_lvl_2_4, _mm_setzero_si128());

                r_2_1 = _mm_movemask_epi8(cmp_lvl_2_1);
                r_2_2 = _mm_movemask_epi8(cmp_lvl_2_2);
                r_2_3 = _mm_movemask_epi8(cmp_lvl_2_3);
                r_2_4 = _mm_movemask_epi8(cmp_lvl_2_4);

                r_2_1 = _bit_scan_forward(r_2_1 ^ 0x1FF);
                r_2_2 = _bit_scan_forward(r_2_2 ^ 0x1FF);
                r_2_3 = _bit_scan_forward(r_2_3 ^ 0x1FF);
                r_2_4 = _bit_scan_forward(r_2_4 ^ 0x1FF);

                r_2_1 += r_1_1 + (r_1_1 << 3);
                r_2_2 += r_1_2 + (r_1_2 << 3);
                r_2_3 += r_1_3 + (r_1_3 << 3);
                r_2_4 += r_1_4 + (r_1_4 << 3);
                //printf("Level 2: %d %d %d %d\n", r_2_1, r_2_2, r_2_3, r_2_4);

                result[4 * i] = (uint32_t) r_2_1;
                result[4 * i + 1] = (uint32_t) r_2_2;
                result[4 * i + 2] = (uint32_t) r_2_3;
                result[4 * i + 3] = (uint32_t) r_2_4;
        }
}

uint32_t probe_index_bs(Tree* tree, int32_t probe_key) {
        int32_t result = 0;

        for (size_t level = 0; level < tree->num_levels; ++level) {
                size_t offset = result * tree->node_capacity[level];
                size_t low = 0;
                size_t high = tree->node_capacity[level];
                while (low != high) {
                        size_t mid = (low + high) / 2;
                        if (tree->key_array[level][mid + offset] < probe_key)
                                low = mid + 1;
                        else
                                high = mid;
                }
                size_t k = low;       // should go to child k
                result = result * (tree->node_capacity[level] + 1) + k;
        }
        return (uint32_t) result;
}

void cleanup_index(Tree* tree) {
        free(tree->node_capacity);
        for (size_t i = 0; i < tree->num_levels; ++i)
                free(tree->key_array[i]);
        free(tree->key_array);
        free(tree);
}
