/*-------------------------------------------------------------------------
 *
<<<<<<< HEAD
 * nodeMockSeqscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/nodeMockSeqscan.h
=======
 * nodeSeqscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/nodeSeqscan.h
>>>>>>> 911dcf2e311d071d93fec67e49ce1d4aea52f96e
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEMOCKSEQSCAN_H
#define NODEMOCKSEQSCAN_H

#include "nodes/execnodes.h"

extern MockSeqScanState *ExecInitMockSeqScan(MockSeqScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecMockSeqScan(MockSeqScanState *node);
extern void ExecEndMockSeqScan(MockSeqScanState *node);
extern void ExecMockSeqMarkPos(MockSeqScanState *node);
extern void ExecMockSeqRestrPos(MockSeqScanState *node);
extern void ExecReScanMockSeqScan(MockSeqScanState *node);

<<<<<<< HEAD
#endif   /* NODEMOCKSEQSCAN_H */
=======
#endif   /* NODESEQSCAN_H */
>>>>>>> 911dcf2e311d071d93fec67e49ce1d4aea52f96e
