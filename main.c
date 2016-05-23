#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "p2random.h"
#include "tree.h"

int main(int argc, char* argv[]) {
        // parsing arguments
        assert(argc > 3);
        size_t num_keys = strtoull(argv[1], NULL, 0);
        size_t num_probes = strtoull(argv[2], NULL, 0);
        size_t num_levels = (size_t) argc - 3;
        size_t* fanout = malloc(sizeof(size_t) * num_levels);
        assert(fanout != NULL);
        size_t isInstruction = 1;
        for (size_t i = 0; i < num_levels; ++i) {
                fanout[i] = strtoull(argv[i + 3], NULL, 0);
                // instruction set flag
                if (fanout[i] != 5 && fanout[i] != 9 && fanout[i] != 17) {
                    isInstruction = 0;
                }
        }
        
        // hardcode flag
        size_t isHardcoded = 0;
        if (num_levels == 3 && num_probes % 4 == 0 && fanout[0] == 9 && fanout[1] == 5 && fanout[2] == 9) {
                isHardcoded = 1;        
        }

        // building the tree index
        rand32_t* gen = rand32_init((uint32_t) time(NULL));
        assert(gen != NULL);
        int32_t* delimiter = generate_sorted_unique(num_keys, gen);
        assert(delimiter != NULL);
        Tree* tree = build_index(num_levels, fanout, num_keys, delimiter);
        free(delimiter);
        free(fanout);
        if (tree == NULL) {
                free(gen);
                exit(EXIT_FAILURE);
        }

        // generate probes
        int32_t* probe = generate(num_probes, gen);
        assert(probe != NULL);
        free(gen);
        uint32_t* result = malloc(sizeof(uint32_t) * num_probes);
        assert(result != NULL);

        // perform index probing (Phase 2)
        clock_t start_t, end_t;

        if (isHardcoded == 1) { // hardcoded fastest
                start_t = clock();
                probe_index_959(tree, probe, num_probes, result);
                end_t = clock();
        } else if (isInstruction == 1) { // instruction set faster
                start_t = clock();
                for (size_t i = 0; i < num_probes; ++i) {
                        result[i] = probe_index_is(tree, probe[i]);
                }
                end_t = clock();
        } else { // binary search slowest
                start_t = clock();
                for (size_t i = 0; i < num_probes; ++i) {
                        result[i] = probe_index_bs(tree, probe[i]);
                }
                end_t = clock();
            
        }

        double total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
        printf("Total probing time: %f\n", total_t);

        // output results
        for (size_t i = 0; i < num_probes; ++i) {
                fprintf(stdout, "%d %u\n", probe[i], result[i]);
        }

        // cleanup and exit
        free(result);
        free(probe);
        cleanup_index(tree);
        return EXIT_SUCCESS;
}
