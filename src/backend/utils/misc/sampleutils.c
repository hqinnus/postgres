/*-------------------------------------------------------------------------
 *
 * sampleutils.c
 *	  Routines designed for TableSample Clause, containing Vitter's Reservoir
 *	  Sampling Algo routines. Tablesample System, Bernoulli sampling and analyze
 *	  is using these apis.
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/sampleutils.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/sampleutils.h"
#include "storage/procarray.h"
#include "storage/bufmgr.h"
#include "utils/tqual.h"
#include "utils/rel.h"

#define MEM_OVERHEAD 100000000


/*
 * BlockSampler_Init -- prepare for random sampling of blocknumbers
 *
 * BlockSampler is used for stage one of our new two-stage tuple
 * sampling mechanism as discussed on pgsql-hackers 2004-04-02 (subject
 * "Large DB").  It selects a random sample of samplesize blocks out of
 * the nblocks blocks in the table.  If the table has less than
 * samplesize blocks, all blocks are selected.
 *
 * Since we know the total number of blocks in advance, we can use the
 * straightforward Algorithm S from Knuth 3.4.2, rather than Vitter's
 * algorithm.
 */
void
BlockSampler_Init(BlockSampler bs, BlockNumber nblocks, int samplesize)
{
	bs->N = nblocks;			/* measured table size */

	/*
	 * If we decide to reduce samplesize for tables that have less or not much
	 * more than samplesize blocks, here is the place to do it.
	 */
	bs->n = samplesize;
	bs->t = 0;					/* blocks scanned so far */
	bs->m = 0;					/* blocks selected so far */
}

bool
BlockSampler_HasMore(BlockSampler bs)
{
	return (bs->t < bs->N) && (bs->m < bs->n);
}

BlockNumber
BlockSampler_Next(void *state, BlockSampler bs)
{
	BlockNumber K = bs->N - bs->t;		/* remaining blocks */
	int			k = bs->n - bs->m;		/* blocks still to sample */
	double		p;				/* probability to skip block */
	double		V;				/* random */

	Assert(BlockSampler_HasMore(bs));	/* hence K > 0 and k > 0 */

	if ((BlockNumber) k >= K)
	{
		/* need all the rest */
		bs->m++;
		return bs->t++;
	}

	/*----------
	 * It is not obvious that this code matches Knuth's Algorithm S.
	 * Knuth says to skip the current block with probability 1 - k/K.
	 * If we are to skip, we should advance t (hence decrease K), and
	 * repeat the same probabilistic test for the next block.  The naive
	 * implementation thus requires an anl_random_fract() call for each block
	 * number.	But we can reduce this to one anl_random_fract() call per
	 * selected block, by noting that each time the while-test succeeds,
	 * we can reinterpret V as a uniform random number in the range 0 to p.
	 * Therefore, instead of choosing a new V, we just adjust p to be
	 * the appropriate fraction of its former value, and our next loop
	 * makes the appropriate probabilistic test.
	 *
	 * We have initially K > k > 0.  If the loop reduces K to equal k,
	 * the next while-test must fail since p will become exactly zero
	 * (we assume there will not be roundoff error in the division).
	 * (Note: Knuth suggests a "<=" loop condition, but we use "<" just
	 * to be doubly sure about roundoff error.)  Therefore K cannot become
	 * less than k, which means that we cannot fail to select enough blocks.
	 *----------
	 */
	V = anl_random_fract(state);
	p = 1.0 - (double) k / (double) K;
	while (V < p)
	{
		/* skip */
		bs->t++;
		K--;					/* keep K == N - t */

		/* adjust p to be new cutoff point in reduced range */
		p *= 1.0 - (double) k / (double) K;
	}

	/* select */
	bs->m++;
	return bs->t++;
}



/*
 * These two routines embody Algorithm Z from "Random sampling with a
 * reservoir" by Jeffrey S. Vitter, in ACM Trans. Math. Softw. 11, 1
 * (Mar. 1985), Pages 37-57.  Vitter describes his algorithm in terms
 * of the count S of records to skip before processing another record.
 * It is computed primarily based on t, the number of records already read.
 * The only extra state needed between calls is W, a random state variable.
 *
 * anl_init_selection_state computes the initial W value.
 *
 * Given that we've already read t records (t >= n), anl_get_next_S
 * determines the number of records to skip before the next record is
 * processed.
 */
double
anl_init_selection_state(void *state, int n)
{
	/* Initial value of W (for use when Algorithm Z is first applied) */
	return exp(-log(anl_random_fract(state)) / n);
}

double
anl_get_next_S(void *state, double t, int n, double *stateptr)
{
	double		S;

	/* The magic constant here is T from Vitter's paper */
	if (t <= (22.0 * n))
	{
		/* Process records using Algorithm X until t is large enough */
		double		V,
					quot;

		V = anl_random_fract(state); /* Generate V */
		S = 0;
		t += 1;
		/* Note: "num" in Vitter's code is always equal to t - n */
		quot = (t - (double) n) / t;
		/* Find min S satisfying (4.1) */
		while (quot > V)
		{
			S += 1;
			t += 1;
			quot *= (t - (double) n) / t;
		}
	}
	else
	{
		/* Now apply Algorithm Z */
		double		W = *stateptr;
		double		term = t - (double) n + 1;

		for (;;)
		{
			double		numer,
						numer_lim,
						denom;
			double		U,
						X,
						lhs,
						rhs,
						y,
						tmp;

			
			U = anl_random_fract(state); /* Generate U and X */
			X = t * (W - 1.0);
			S = floor(X);		/* S is tentatively set to floor(X) */
			/* Test if U <= h(S)/cg(X) in the manner of (6.3) */
			tmp = (t + 1) / term;
			lhs = exp(log(((U * tmp * tmp) * (term + S)) / (t + X)) / n);
			rhs = (((t + X) / (term + S)) * term) / t;
			if (lhs <= rhs)
			{
				W = rhs / lhs;
				break;
			}
			/* Test if U <= f(S)/cg(X) */
			y = (((U * (t + 1)) / term) * (t + S + 1)) / (t + X);
			if ((double) n < S)
			{
				denom = t;
				numer_lim = term + S;
			}
			else
			{
				denom = t - (double) n + S;
				numer_lim = t + 1;
			}
			for (numer = t + S; numer >= numer_lim; numer -= 1)
			{
				y *= numer / denom;
				denom -= 1;
			}
			W = exp(-log(anl_random_fract(state)) / n);		/* Generate W in advance */
			if (exp(log(y) / n) <= (t + X) / t)
				break;
		}
		*stateptr = W;
	}
	return S;
}

/* ---------------------------------------------------
 * Random number generator
 * ---------------------------------------------------
 */

/* Select a random value R uniformly distributed in (0 - 1), for tablsample use */
double
anl_random_fract(void *state)
{
	/* XXX: If the sample_random can work as good as random(), may merge,
	 * but now, as sample_random surely needs improvement, we leave 
	 * analzye.c and tablesample using different random generator.
	 */
	if(state == NULL)
	{
		return ((double) random() + 1) / ((double) MAX_RANDOM_VALUE + 2);
	}else{
		return sample_random(state);
	}
}

/*
 * Returns a randomly-generated trigger x, such that a <= x < b
 */
int
get_rand_in_range(void *rand_state, int a, int b)
{
	double rand = sample_random(rand_state);
	return (rand * b) + a;
}

/*
 * Random Number Seeding
 */
void sample_set_seed(void *random_state, double seed)
{
	/*     
	 * XXX. This seeding algorithm could certainly be improved.
	 * May need some effort on this.
	 */    
	seed += MEM_OVERHEAD; //seed must be large enough to make difference

	memset(random_state, 0, sizeof(random_state));
	memcpy(random_state,
			&seed,
			Min(sizeof(random_state), sizeof(seed)));
}

double sample_random(void *random_state)
{
	return pg_erand48(random_state);
}
