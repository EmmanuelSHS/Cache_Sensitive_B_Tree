# Cache_Sensitive_B_Tree
Pointer Free Cache Sensitive B Tree for DBMS allowing Billions probing per second

## I. Structure Introduction

This project is for implementation of paper (http://www.cs.columbia.edu/~orestis/sigmod14I.pdf), also part of course project from Database System Implementation COMS4112 from Columbia University. 

### PART I. Binary Search Tree

A pointer free tree stucture, as is mentioned by reference document (http://www.cs.columbia.edu/~orestis/sigmod14I.pdf), allow faster in-memory access and data partition. This structure needs no pointer for accessing children nodes. Nodes of same level are compressed into an array, and children accessing is allowed via computing offset by formula. Fanout can be customized for different level in tree.

### PART II. Tree with Instruction Set

For this part, we revised the implementation of pointer free tree such that instruction set is introduced to allow faster query. According to guidance, this tree supports only fanout of 5, 9 and 17. Keys are loaded into SIMD lanes by shuffle instruction, and probe keys are broadcasted and searched via _mm_cmpgt_epi32, _mm_packs_epi32, _mm_movemask_epi8 _bit_scan_forward as well as other funtions.

A 9-5-9 tree is hardcoded, eliminating any unnecessary if-else, while and for conditions for the broadcasting and searching of probe keys. In order to use hardcoded 9-5-9 tree, number of probing key must be the multiplication of 4, else non-hardcoded one is applied.

## II. Source codes

The implementaiton of pointer free tree is in tree.c, which is improved by reference code, functions include:

        a. build_index: function for reading in generated sorted delimiter keys and build pointer free trees.
        
        b. probe_index_is: probing one key in tree via Intel Instruction Set in a non-hardcoded approach, where only fanouts of 5, 9 and 17 are allowed.
        
        c. probe_index_959: probing all keys in tree via instruction set function in hardcoded way. Two optimizations are applied: Firstly, the root level array is pre-loaded before probing, removing redundant loading in probing of every key. Secondly, to fully utilize the space in loading probe keys (which is 128 bits), 4 32-bit keys are probed at one time.
        
        d. probe_index_bt: probing keys via simple binary search tree if the tree nodes does not satified conditions in previous two probing algorithms.
        
        e. cleanup_index: function for releasing memory space taken by point free tree index.
        
Then main function in main.c uses pftree and p2random in following order:

        a. initialization: reading inputs, computing parameters such as total slots in tree, upper and lower bound of numbers of keys for meaningful tree. Then it initializes tree structure, keys and probes, and detects the illegal inputs (e.g. insufficient keys, overloaded tree, etc.).
        
        b. loading: iterates through key array for filling keys as well as required INT_MAX to tree, with loading and filling from pftree.c.
        
        c. probing: iterates through probe array to get range of each probe in tree, via calling probing in pftree.c, and saves result ranges in output array.
        
        d. write: writes output range to stdout. It is also plausible to compare write result with actually range for probes, by using testing function in main.c. (see III. for how to do that). It also outputs the time for probing procedure in unit of cpu clock.
        
        e. cleaning: free pointers.

## III. User Guidance 
This folder contains following files:
        a. Makefile for compiling a pointer free tree.
        b. A README file (this document).
        c. A main.c file for main function.
        d. pftree.h and pftree.c for implementation of pointer free tree.
        e. p2random.c and p2random.h for implementation of random number generator.
        f. A test.sh for automatic compiling and runing test case, and write output to a output.dat file.

To build desired program, uses 'make Makefile all'. This will generate a 'build'. Then it would work with such command as './build 8 2 3 2 3'. Result probe ranges will be printed to screen in required format. 

For automatically generating test result, please use test.sh, it will make a build program, test several sets of input parameters, and redirect printed output to output.dat file. output.dat also contains parameters for each test case.


## IV. Runtime Cost
All runtimes for Probing phase are collected from runnint build on CLIC machine at Columbia University.

        fanouts         nkeys           nprobes         runtime(s)      Hardcoded
         9-5-9           300            200,000,000       1.81          Yes
         9-5-9           300            20,000,000        0.16          Yes
         9-5-9           300            2,000,000         0.02          Yes
         9-5-9           360            200,000,000       1.81          Yes
         9-5-9           360            20,000,000        0.16          Yes
         9-5-9           360            2,000,000         0.02          Yes
         9-5-9           300            200,000,000       2.50          No
         9-5-9           300            20,000,000        0.25          No
         9-5-9           300            2,000,000         0.03          No
         17-17           280            200,000,000       2.36          No
         17-17           280            20,000,000        0.24          No
         17-17           280            2,000,000         0.30          No
         9-5-5-9         2000           200,000,000       3.31          No
         9-5-5-9         2000           20,000,000        0.33          No
         9-5-5-9         2000           2,000,000         0.05          No
         9-5-9           360            200,000,000       11.18         PartI
         9-5-9           360            20,000,000        1.13          PartI
         9-5-9           360            2,000,000         0.12          PartI
         17-17           280            200,000,000       9.72          PartI
         17-17           280            20,000,000        0.98          PartI
         17-17           280            2,000,000         0.10          PartI
         9-5-5-9         2000           200,000,000       15.33         PartI
         9-5-5-9         2000           20,000,000        1.56          PartI
         9-5-5-9         2000           2,000,000         0.15          PartI



