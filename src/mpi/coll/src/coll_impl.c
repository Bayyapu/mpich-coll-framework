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

#include "mpiimpl.h"

/*
=== BEGIN_MPI_T_CVAR_INFO_BLOCK ===

cvars:
    - name        : MPIR_CVAR_DEVICE_COLLECTIVES
      category    : COLLECTIVE
      type        : boolean
      default     : true
      class       : device
      verbosity   : MPI_T_VERBOSITY_USER_BASIC
      scope       : MPI_T_SCOPE_ALL_EQ
      description : >-
        If set to true, MPI collectives will use allow the device to override
        the default, MPIR-level collective algorithms. The device still has the
        option to call the MPIR-level algorithms manually.  If set to false,
        the device-level collective function will not be called.

=== END_MPI_T_CVAR_INFO_BLOCK ===
*/

int MPIR_COLL_init(void) {
    int mpi_errno = MPI_SUCCESS;

    /* =========================================================================== */
    /* | Check if the user manually selected any collective algorithms via CVARs | */
    /* =========================================================================== */

    /* Allgather Intra */
    if (0 == strcmp(MPIR_CVAR_ALLGATHER_ALGORITHM_INTRA, "brucks"))
        MPIR_Allgather_alg_intra_choice = MPIR_ALLGATHER_ALG_INTRA_BRUCKS;
    else if (0 == strcmp(MPIR_CVAR_ALLGATHER_ALGORITHM_INTRA, "recursive_doubling"))
        MPIR_Allgather_alg_intra_choice = MPIR_ALLGATHER_ALG_INTRA_RECURSIVE_DOUBLING;
    else if (0 == strcmp(MPIR_CVAR_ALLGATHER_ALGORITHM_INTRA, "ring"))
        MPIR_Allgather_alg_intra_choice = MPIR_ALLGATHER_ALG_INTRA_RING;
    else
        MPIR_Allgather_alg_intra_choice = MPIR_ALLGATHER_ALG_INTRA_AUTO;

    /* Allgather Inter */
    MPIR_Allgather_alg_inter_choice = MPIR_ALLGATHER_ALG_INTER_AUTO;

    /* Allgatherv Intra */

    /* Allgatherv Inter */

    /* Allreduce_group Intra */

    /* Allreduce_group Inter */

    /* Allreduce Intra */

    /* Allreduce Inter */

    /* Alltoall Intra */

    /* Alltoall Inter */

    /* Alltoallv Intra */

    /* Alltoallv Inter */

    /* Alltoallw Intra */

    /* Alltoallw Inter */

    /* Barrier Intra */

    /* Barrier Inter */

    /* Barrier_group Intra */

    /* Barrier_group Inter */

    /* Bcast Intra */
    if (0 == strcmp(MPIR_CVAR_BCAST_ALGORITHM_INTRA, "binomial"))
        MPIR_Bcast_alg_intra_choice = MPIR_BCAST_ALG_INTRA_BINOMIAL;
    else if (0 == strcmp(MPIR_CVAR_BCAST_ALGORITHM_INTRA, "scatter_doubling_allgather"))
        MPIR_Bcast_alg_intra_choice = MPIR_BCAST_ALG_INTRA_SCATTER_DOUBLING_ALLGATHER;
    else if (0 == strcmp(MPIR_CVAR_BCAST_ALGORITHM_INTRA, "scatter_ring_allgather"))
        MPIR_Bcast_alg_intra_choice = MPIR_BCAST_ALG_INTRA_SCATTER_RING_ALLGATHER;
    else
        MPIR_Bcast_alg_intra_choice = MPIR_BCAST_ALG_INTRA_AUTO;

    /* Bcast Inter */
    MPIR_Bcast_alg_inter_choice = MPIR_BCAST_ALG_INTER_AUTO;

    /* Exscan Intra */

    /* Exscan Inter */

    /* Gather Intra */

    /* Gather Inter */

    /* Gatherv Intra */

    /* Gatherv Inter */

    /* Iallgather Intra */

    /* Iallgather Inter */

    /* Iallgatherv Intra */

    /* Iallgatherv Inter */

    /* Iallreduce Intra */

    /* Iallreduce Inter */

    /* Ialltoall Intra */

    /* Ialltoall Inter */

    /* Ialltoallv Intra */

    /* Ialltoallv Inter */

    /* Ialltoallw Intra */

    /* Ialltoallw Inter */

    /* Ibarrier Intra */

    /* Ibarrier Inter */

    /* Ibcast Intra */

    /* Ibcast Inter */

    /* Iexscan Intra */

    /* Iexscan Inter */

    /* Igather Intra */

    /* Igather Inter */

    /* Igatherv Intra */

    /* Igatherv Inter */

    /* Inhb_allgather Intra */

    /* Inhb_allgather Inter */

    /* Inhb_allgatherv Intra */

    /* Inhb_allgatherv Inter */

    /* Inhb_alltoall Intra */

    /* Inhb_alltoall Inter */

    /* Inhb_alltoallv Intra */

    /* Inhb_alltoallv Inter */

    /* Inhb_alltoallw Intra */

    /* Inhb_alltoallw Inter */

    /* Ired_scat Intra */

    /* Ired_scat Inter */

    /* Ired_scat_block Intra */

    /* Ired_scat_block Inter */

    /* Ireduce Intra */

    /* Ireduce Inter */

    /* Iscan Intra */

    /* Iscan Inter */

    /* Iscatter Intra */

    /* Iscatter Inter */

    /* Iscatterv Intra */

    /* Iscatterv Inter */

    /* Nhb_allgather Intra */

    /* Nhb_allgather Inter */

    /* Nhb_allgatherv Intra */

    /* Nhb_allgatherv Inter */

    /* Nhb_alltoall Intra */

    /* Nhb_alltoall Inter */

    /* Nhb_alltoallv Intra */

    /* Nhb_alltoallv Inter */

    /* Nhb_alltoallw Intra */

    /* Nhb_alltoallw Inter */

    /* Red_scat Intra */

    /* Red_scat Inter */

    /* Red_scat_block Intra */

    /* Red_scat_block Inter */

    /* Reduce Intra */

    /* Reduce Inter */

    /* Scan Intra */

    /* Scan Inter */

    /* Scatter Intra */

    /* Scatter Inter */

    /* Scatterv Intra */

    /* Scatterv Inter */

    return mpi_errno;
}

