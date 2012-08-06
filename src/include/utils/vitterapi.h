/*-------------------------------------------------------------------------
 *
 * vitterapi.h
 *	  APIs for Vitter's reservoir Sampling Algo.
 *
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/vitterapi.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef VITTERAPI_H
#define VITTERAPI_H

#include "storage/block.h"

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
extern BlockNumber BlockSampler_Next(BlockSampler bs);
extern double anl_random_fract(void);
extern double anl_init_selection_state(int n);
extern double anl_get_next_S(double t, int n, double *stateptr);

#endif /* VITTERAPI_H */
