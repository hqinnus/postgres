/*-------------------------------------------------------------------------
 *
 * sampleutils.h
 *	  APIs for Tablesample Support, containing Vitter's Algo APIs, analyze.c
 *	  is also using the APIs.
 *
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/sampleutils.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SAMPLEUTILS_H
#define SAMPLEUTILS_H

#include "storage/block.h"
#include "utils/relcache.h"
#include "access/htup.h"
#include "storage/buf.h"

/* Data structure for Algorithm S from Knuth 3.4.2 */
typedef struct
{
	BlockNumber N;				/* number of blocks, known in advance */
	int			n;				/* desired sample size */
	BlockNumber t;				/* current block number */
	int			m;				/* blocks selected so far */
} BlockSamplerData;

typedef BlockSamplerData *BlockSampler;


extern void BlockSampler_Init(BlockSampler bs, BlockNumber nblocks,
				  int samplesize);
extern bool BlockSampler_HasMore(BlockSampler bs);
extern BlockNumber BlockSampler_Next(void *state, BlockSampler bs);
extern double anl_init_selection_state(void *state, int n);
extern double anl_get_next_S(void *state, double t, int n, double *stateptr);
extern double anl_random_fract(void *state);
extern int get_rand_in_range(void *state, int a, int b);
extern void sample_set_seed(void *random_state, double seed);
extern double sample_random(void *random_state);
extern int acquire_vitter_rows(Relation onerel, void *state, HeapTuple *rows, int targrows,
							BlockNumber totalblocks, BufferAccessStrategy vac_strategy,
							double *totalrows, double *totaldeadrows, bool isanalyze, int elevel);

#endif /* SAMPLEUTILS_H */
