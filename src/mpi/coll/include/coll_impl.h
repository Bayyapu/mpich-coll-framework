/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2006 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 *  Portions of this code were written by Intel Corporation.
 *  Copyright (C) 2011-2017 Intel Corporation.  Intel provides this material
 *  to Argonne National Laboratory subject to Software Grant and Corporate
 *  Contributor License Agreement dated February 8, 2012.
 */

#ifndef MPIR_COLL_IMPL_H_INCLUDED
#define MPIR_COLL_IMPL_H_INCLUDED

/* Enumerate the list of algorithms */
typedef enum MPIR_Allgather_alg_intra_t {
    MPIR_ALLGATHER_ALG_INTRA_AUTO,
    MPIR_ALLGATHER_ALG_INTRA_BRUCKS,
    MPIR_ALLGATHER_ALG_INTRA_RECURSIVE_DOUBLING,
    MPIR_ALLGATHER_ALG_INTRA_RING
} MPIR_Allgather_alg_intra_t;
/* Have an easy way of finding the algorithm choice later without doing string
 * comparisons */
int MPIR_Allgather_alg_intra_choice;

typedef enum MPIR_Allgather_alg_inter_t {
    MPIR_ALLGATHER_ALG_INTER_AUTO
} MPIR_Allgather_alg_inter_t;
int MPIR_Allgather_alg_inter_choice;

typedef enum MPIR_Bcast_alg_intra_t {
    MPIR_BCAST_ALG_INTRA_AUTO,
    MPIR_BCAST_ALG_INTRA_BINOMIAL,
    MPIR_BCAST_ALG_INTRA_SCATTER_DOUBLING_ALLGATHER,
    MPIR_BCAST_ALG_INTRA_SCATTER_RING_ALLGATHER
} MPIR_Bcast_alg_intra_t;
int MPIR_Bcast_alg_intra_choice;

typedef enum MPIR_Bcast_alg_inter_t {
    MPIR_BCAST_ALG_INTER_AUTO
} MPIR_Bcast_alg_inter_t;
int MPIR_Bcast_alg_inter_choice;

int MPIR_COLL_init(void);

#endif /* MPIR_COLL_IMPL_H_INCLUDED */
