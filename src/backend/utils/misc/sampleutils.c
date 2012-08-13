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


static int	compare_rows(const void *a, const void *b);

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

/*
 * Acquire a random sample of rows from the table.
 * This is the place where Vitter's Reservior Sampling Algo gets implemented.
 * Used by tablesample scan and analyze.c. See the usage below.
 *
 *
 * Selected rows are returned in the caller-allocated array rows[], which
 * must have at least targrows entries.
 * The actual number of rows selected is returned as the function result.
 * We also estimate the total numbers of live and dead rows in the table,
 * and return them into *totalrows and *totaldeadrows, respectively.
 *
 * The returned list of tuples is in order by physical position in the table.
 * (We will rely on this later to derive correlation estimates.)
 *
 * As of May 2004 we use a new two-stage method:  Stage one selects up
 * to targrows random blocks (or all blocks, if there aren't so many).
 * Stage two scans these blocks and uses the Vitter algorithm to create
 * a random sample of targrows rows (or less, if there are less in the
 * sample of blocks).  The two stages are executed simultaneously: each
 * block is processed as soon as stage one returns its number and while
 * the rows are read stage two controls which ones are to be inserted
 * into the sample.
 *
 * Although every row has an equal chance of ending up in the final
 * sample, this sampling method is not perfect: not every possible
 * sample has an equal chance of being selected.  For large relations
 * the number of different blocks represented by the sample tends to be
 * too small.  We can live with that for now.  Improvements are welcome.
 *
 * An important property of this sampling method is that because we do
 * look at a statistically unbiased set of blocks, we should get
 * unbiased estimates of the average numbers of live and dead rows per
 * block.  The previous sampling method put too much credence in the row
 * density near the start of the table.
 */
int
acquire_vitter_rows(Relation onerel, void *state, HeapTuple *rows, int targrows,
					BlockNumber totalblocks, BufferAccessStrategy vac_strategy,
					double *totalrows, double *totaldeadrows,
                    bool isanalyze, int elevel)
{
	int			numrows = 0;	/* # rows now in reservoir */
	double		samplerows = 0; /* total # rows collected */
	double		liverows = 0;	/* # live rows seen */
	double		deadrows = 0;	/* # dead rows seen */
	double		rowstoskip = -1;	/* -1 means not set yet */
	TransactionId OldestXmin;
	BlockSamplerData bs;
	double		rstate;

	Assert(targrows > 0);

	/* Need a cutoff xmin for HeapTupleSatisfiesVacuum */
	OldestXmin = GetOldestXmin(onerel->rd_rel->relisshared, true);

	/* Prepare for sampling block numbers */
	BlockSampler_Init(&bs, totalblocks, targrows);
	/* Prepare for sampling rows */
	rstate = anl_init_selection_state(state, targrows);

	/* Outer loop over blocks to sample */
	while (BlockSampler_HasMore(&bs))
	{
		BlockNumber targblock = BlockSampler_Next(state, &bs);
		Buffer		targbuffer;
		Page		targpage;
		OffsetNumber targoffset,
					maxoffset;

		vacuum_delay_point();

		/*
		 * We must maintain a pin on the target page's buffer to ensure that
		 * the maxoffset value stays good (else concurrent VACUUM might delete
		 * tuples out from under us).  Hence, pin the page until we are done
		 * looking at it.  We also choose to hold sharelock on the buffer
		 * throughout --- we could release and re-acquire sharelock for each
		 * tuple, but since we aren't doing much work per tuple, the extra
		 * lock traffic is probably better avoided.
		 */
		targbuffer = ReadBufferExtended(onerel, MAIN_FORKNUM, targblock,
										RBM_NORMAL, vac_strategy);
		LockBuffer(targbuffer, BUFFER_LOCK_SHARE);
		targpage = BufferGetPage(targbuffer);
		maxoffset = PageGetMaxOffsetNumber(targpage);

		/* Inner loop over all tuples on the selected page */
		for (targoffset = FirstOffsetNumber; targoffset <= maxoffset; targoffset++)
		{
			ItemId		itemid;
			HeapTupleData targtuple;
			bool		sample_it = false;

			itemid = PageGetItemId(targpage, targoffset);

			/*
			 * We ignore unused and redirect line pointers.  DEAD line
			 * pointers should be counted as dead, because we need vacuum to
			 * run to get rid of them.	Note that this rule agrees with the
			 * way that heap_page_prune() counts things.
			 */
			if (!ItemIdIsNormal(itemid))
			{
				if (ItemIdIsDead(itemid))
					deadrows += 1;
				continue;
			}

			ItemPointerSet(&targtuple.t_self, targblock, targoffset);

			targtuple.t_data = (HeapTupleHeader) PageGetItem(targpage, itemid);
			targtuple.t_len = ItemIdGetLength(itemid);

			switch (HeapTupleSatisfiesVacuum(targtuple.t_data,
											 OldestXmin,
											 targbuffer))
			{
				case HEAPTUPLE_LIVE:
					sample_it = true;
					liverows += 1;
					break;

				case HEAPTUPLE_DEAD:
				case HEAPTUPLE_RECENTLY_DEAD:
					/* Count dead and recently-dead rows */
					deadrows += 1;
					break;

				case HEAPTUPLE_INSERT_IN_PROGRESS:

					/*
					 * Insert-in-progress rows are not counted.  We assume
					 * that when the inserting transaction commits or aborts,
					 * it will send a stats message to increment the proper
					 * count.  This works right only if that transaction ends
					 * after we finish analyzing the table; if things happen
					 * in the other order, its stats update will be
					 * overwritten by ours.  However, the error will be large
					 * only if the other transaction runs long enough to
					 * insert many tuples, so assuming it will finish after us
					 * is the safer option.
					 *
					 * A special case is that the inserting transaction might
					 * be our own.	In this case we should count and sample
					 * the row, to accommodate users who load a table and
					 * analyze it in one transaction.  (pgstat_report_analyze
					 * has to adjust the numbers we send to the stats
					 * collector to make this come out right.)
					 */
					if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(targtuple.t_data)))
					{
						sample_it = true;
						liverows += 1;
					}
					break;

				case HEAPTUPLE_DELETE_IN_PROGRESS:

					/*
					 * We count delete-in-progress rows as still live, using
					 * the same reasoning given above; but we don't bother to
					 * include them in the sample.
					 *
					 * If the delete was done by our own transaction, however,
					 * we must count the row as dead to make
					 * pgstat_report_analyze's stats adjustments come out
					 * right.  (Note: this works out properly when the row was
					 * both inserted and deleted in our xact.)
					 */
					if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(targtuple.t_data)))
						deadrows += 1;
					else
						liverows += 1;
					break;

				default:
					elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
					break;
			}

			if (sample_it)
			{
				/*
				 * The first targrows sample rows are simply copied into the
				 * reservoir. Then we start replacing tuples in the sample
				 * until we reach the end of the relation.	This algorithm is
				 * from Jeff Vitter's paper (see full citation below). It
				 * works by repeatedly computing the number of tuples to skip
				 * before selecting a tuple, which replaces a randomly chosen
				 * element of the reservoir (current set of tuples).  At all
				 * times the reservoir is a true random sample of the tuples
				 * we've passed over so far, so when we fall off the end of
				 * the relation we're done.
				 */
				if (numrows < targrows)
					rows[numrows++] = heap_copytuple(&targtuple);
				else
				{
					/*
					 * t in Vitter's paper is the number of records already
					 * processed.  If we need to compute a new S value, we
					 * must use the not-yet-incremented value of samplerows as
					 * t.
					 */
					if (rowstoskip < 0)
						rowstoskip = anl_get_next_S(state, samplerows, targrows,
													&rstate);

					if (rowstoskip <= 0)
					{
						/*
						 * Found a suitable tuple, so save it, replacing one
						 * old tuple at random
						 */
						int			k = (int) (targrows * anl_random_fract(state));

						Assert(k >= 0 && k < targrows);
						heap_freetuple(rows[k]);
						rows[k] = heap_copytuple(&targtuple);
					}

					rowstoskip -= 1;
				}

				samplerows += 1;
			}
		}

		/* Now release the lock and pin on the page */
		UnlockReleaseBuffer(targbuffer);
	}

	if(isanalyze)
	{
		/*
		 * If we didn't find as many tuples as we wanted then we're done. No sort
		 * is needed, since they're already in order.
		 *
		 * Otherwise we need to sort the collected tuples by position
		 * (itempointer). It's not worth worrying about corner cases where the
		 * tuples are already sorted.
		 */
		if (numrows == targrows)
			qsort((void *) rows, numrows, sizeof(HeapTuple), compare_rows);
	
		/*
		 * Estimate total numbers of rows in relation.	For live rows, use
		 * vac_estimate_reltuples; for dead rows, we have no source of old
		 * information, so we have to assume the density is the same in unseen
		 * pages as in the pages we scanned.
		 */
		*totalrows = vac_estimate_reltuples(onerel, true,
											totalblocks,
											bs.m,
											liverows);
		if (bs.m > 0)
			*totaldeadrows = floor((deadrows / bs.m) * totalblocks + 0.5);
		else
			*totaldeadrows = 0.0;
	
		/*
		 * Emit some interesting relation info
		 */
		ereport(elevel,
				(errmsg("\"%s\": scanned %d of %u pages, "
						"containing %.0f live rows and %.0f dead rows; "
						"%d rows in sample, %.0f estimated total rows",
						RelationGetRelationName(onerel),
						bs.m, totalblocks,
						liverows, deadrows,
						numrows, *totalrows)));
	}

	return numrows;
}

/*
 * qsort comparator for sorting rows[] array
 */
static int
compare_rows(const void *a, const void *b)
{
	HeapTuple	ha = *(const HeapTuple *) a;
	HeapTuple	hb = *(const HeapTuple *) b;
	BlockNumber ba = ItemPointerGetBlockNumber(&ha->t_self);
	OffsetNumber oa = ItemPointerGetOffsetNumber(&ha->t_self);
	BlockNumber bb = ItemPointerGetBlockNumber(&hb->t_self);
	OffsetNumber ob = ItemPointerGetOffsetNumber(&hb->t_self);

	if (ba < bb)
		return -1;
	if (ba > bb)
		return 1;
	if (oa < ob)
		return -1;
	if (oa > ob)
		return 1;
	return 0;
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
	memset(random_state, 0, sizeof(random_state));
	memcpy(random_state,
			&seed,
			Min(sizeof(random_state), sizeof(seed)));
}

double sample_random(void *random_state)
{
	return pg_erand48(random_state);
}
