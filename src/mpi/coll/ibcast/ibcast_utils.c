/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2010 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"
#include "collutil.h"
#include "ibcast.h"

#undef FUNCNAME
#define FUNCNAME MPII_sched_test_length
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPII_sched_test_length(MPIR_Comm * comm, int tag, void *state)
{
    int mpi_errno = MPI_SUCCESS;
    int recv_size;
    struct  MPII_Ibcast_status *status = (struct MPII_Ibcast_status*) state;
    MPIR_Get_count_impl(&status->status, MPI_BYTE, &recv_size);
    if(status->n_bytes != recv_size || status->status.MPI_ERROR != MPI_SUCCESS) {
        mpi_errno = MPIR_Err_create_code( mpi_errno, MPIR_ERR_RECOVERABLE,
                FCNAME, __LINE__, MPI_ERR_OTHER,
                "**collective_size_mismatch",
                "**collective_size_mismatch %d %d", status->n_bytes, recv_size);
    }
    return mpi_errno;
}

#undef FUNCNAME
#define FUNCNAME MPII_sched_test_curr_length
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPII_sched_test_curr_length(MPIR_Comm * comm, int tag, void *state)
{
    int mpi_errno = MPI_SUCCESS;
    struct  MPII_Ibcast_status *status = (struct MPII_Ibcast_status*) state;
    if(status->n_bytes != status->curr_bytes) {
        mpi_errno = MPIR_Err_create_code( mpi_errno, MPIR_ERR_RECOVERABLE,
                FCNAME, __LINE__, MPI_ERR_OTHER,
                "**collective_size_mismatch",
                "**collective_size_mismatch %d %d", status->n_bytes, status->curr_bytes);
    }
    return mpi_errno;
}

#undef FUNCNAME
#define FUNCNAME MPII_sched_add_length
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPII_sched_add_length(MPIR_Comm * comm, int tag, void *state)
{
    int mpi_errno = MPI_SUCCESS;
    int recv_size;
    struct  MPII_Ibcast_status *status = (struct MPII_Ibcast_status*) state;
    MPIR_Get_count_impl(&status->status, MPI_BYTE, &recv_size);
    status->curr_bytes+= recv_size;
    return mpi_errno;
}

/*
   Broadcast based on a scatter followed by an allgather.

   We first scatter the buffer using a binomial tree algorithm. This costs
   lgp.alpha + n.((p-1)/p).beta
   If the datatype is contiguous and the communicator is homogeneous,
   we treat the data as bytes and divide (scatter) it among processes
   by using ceiling division. For the noncontiguous or heterogeneous
   cases, we first pack the data into a temporary buffer by using
   MPI_Pack, scatter it as bytes, and unpack it after the allgather.

   We use a ring algorithm for the allgather, which takes p-1 steps.
   This may perform better than recursive doubling for long messages and
   medium-sized non-power-of-two messages.
   Total Cost = (lgp+p-1).alpha + 2.n.((p-1)/p).beta
*/
#undef FUNCNAME
#define FUNCNAME MPII_Ibcast_scatter_ring_allgather_sched
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPII_Ibcast_scatter_ring_allgather_sched(void *buffer, int count, MPI_Datatype datatype, int root, MPIR_Comm *comm_ptr, MPIR_Sched_t s)
{
    int mpi_errno = MPI_SUCCESS;
    int comm_size, rank;
    int is_contig, is_homogeneous ATTRIBUTE((unused)), type_size, nbytes;
    int scatter_size, curr_size;
    int i, j, jnext, left, right;
    MPI_Aint true_extent, true_lb;
    void *tmp_buf = NULL;

    struct MPII_Ibcast_status *status;
    MPIR_SCHED_CHKPMEM_DECL(4);

    comm_size = comm_ptr->local_size;
    rank = comm_ptr->rank;

    /* If there is only one process, return */
    if (comm_size == 1)
        goto fn_exit;

    if (HANDLE_GET_KIND(datatype) == HANDLE_KIND_BUILTIN)
        is_contig = 1;
    else {
        MPIR_Datatype_is_contig(datatype, &is_contig);
    }

    is_homogeneous = 1;
#ifdef MPID_HAS_HETERO
    if (comm_ptr->is_hetero)
        is_homogeneous = 0;
#endif
    MPIR_Assert(is_homogeneous); /* we don't handle the hetero case yet */
    MPIR_SCHED_CHKPMEM_MALLOC(status, struct MPII_Ibcast_status*,
                              sizeof(struct MPII_Ibcast_status), mpi_errno, "MPI_Status", MPL_MEM_BUFFER);
    MPIR_Datatype_get_size_macro(datatype, type_size);
    nbytes = type_size * count;
    status->n_bytes = nbytes;
    status->curr_bytes = 0;
    if (is_contig) {
        /* contiguous, no need to pack. */
        MPIR_Type_get_true_extent_impl(datatype, &true_lb, &true_extent);

        tmp_buf = (char *) buffer + true_lb;
    }
    else {
        MPIR_SCHED_CHKPMEM_MALLOC(tmp_buf, void *, nbytes, mpi_errno, "tmp_buf", MPL_MEM_BUFFER);

        /* TODO: Pipeline the packing and communication */
        if (rank == root) {
            mpi_errno = MPIR_Sched_copy(buffer, count, datatype, tmp_buf, nbytes, MPI_BYTE, s);
            if (mpi_errno) MPIR_ERR_POP(mpi_errno);
            MPIR_SCHED_BARRIER(s);
        }
    }

    mpi_errno = MPII_Iscatter_for_bcast_sched(tmp_buf, root, comm_ptr, nbytes, s);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

    /* this is the block size used for the scatter operation */
    scatter_size = (nbytes + comm_size - 1) / comm_size; /* ceiling division */

    /* curr_size is the amount of data that this process now has stored in
     * buffer at byte offset (rank*scatter_size) */
    curr_size = MPL_MIN(scatter_size, (nbytes - (rank * scatter_size)));
    if (curr_size < 0)
        curr_size = 0;
    /* curr_size bytes already inplace */
    status->curr_bytes = curr_size;

    /* long-message allgather or medium-size but non-power-of-two. use ring algorithm. */

    left  = (comm_size + rank - 1) % comm_size;
    right = (rank + 1) % comm_size;

    j     = rank;
    jnext = left;
    for (i = 1; i < comm_size; i++) {
        int left_count, right_count, left_disp, right_disp, rel_j, rel_jnext;

        rel_j     = (j     - root + comm_size) % comm_size;
        rel_jnext = (jnext - root + comm_size) % comm_size;
        left_count = MPL_MIN(scatter_size, (nbytes - rel_jnext * scatter_size));
        if (left_count < 0)
            left_count = 0;
        left_disp = rel_jnext * scatter_size;
        right_count = MPL_MIN(scatter_size, (nbytes - rel_j * scatter_size));
        if (right_count < 0)
            right_count = 0;
        right_disp = rel_j * scatter_size;

        mpi_errno = MPIR_Sched_send(((char *)tmp_buf + right_disp),
                                    right_count, MPI_BYTE, right, comm_ptr, s);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
        /* sendrecv, no barrier here */
        mpi_errno = MPIR_Sched_recv_status(((char *)tmp_buf + left_disp),
                                    left_count, MPI_BYTE, left, comm_ptr, &status->status, s);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
        MPIR_SCHED_BARRIER(s);
        mpi_errno = MPIR_Sched_cb(&MPII_sched_add_length, status, s);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
        MPIR_SCHED_BARRIER(s);

        j     = jnext;
        jnext = (comm_size + jnext - 1) % comm_size;
    }
    mpi_errno = MPIR_Sched_cb(&MPII_sched_test_curr_length, status, s);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

    if (!is_contig && rank != root) {
        mpi_errno = MPIR_Sched_copy(tmp_buf, nbytes, MPI_BYTE, buffer, count, datatype, s);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    }

    MPIR_SCHED_CHKPMEM_COMMIT(s);
fn_exit:
    return mpi_errno;
fn_fail:
    MPIR_SCHED_CHKPMEM_REAP(s);
    goto fn_exit;
}

/* TODO it would be nice if we could refactor things to minimize
   duplication between this and MPIR_Iscatter_intra and friends.  We can't use
   MPIR_Iscatter_intra as is without inducing an extra copy in the noncontig case. */
/* This is a binomial scatter operation, but it does *not* take typical scatter
 * arguments.  At the moment this function always scatters a buffer of nbytes
 * starting at tmp_buf address. */
#undef FUNCNAME
#define FUNCNAME MPII_Iscatter_for_bcast_sched
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPII_Iscatter_for_bcast_sched(void *tmp_buf, int root, MPIR_Comm *comm_ptr, int nbytes, MPIR_Sched_t s)
{
    int mpi_errno = MPI_SUCCESS;
    int rank, comm_size, src, dst;
    int relative_rank, mask;
    int scatter_size, curr_size, recv_size, send_size;

    comm_size = comm_ptr->local_size;
    rank = comm_ptr->rank;
    relative_rank = (rank >= root) ? rank - root : rank - root + comm_size;

    /* The scatter algorithm divides the buffer into nprocs pieces and
       scatters them among the processes. Root gets the first piece,
       root+1 gets the second piece, and so forth. Uses the same binomial
       tree algorithm as above. Ceiling division
       is used to compute the size of each piece. This means some
       processes may not get any data. For example if bufsize = 97 and
       nprocs = 16, ranks 15 and 16 will get 0 data. On each process, the
       scattered data is stored at the same offset in the buffer as it is
       on the root process. */

    scatter_size = (nbytes + comm_size - 1)/comm_size; /* ceiling division */
    curr_size = (rank == root) ? nbytes : 0; /* root starts with all the data */

    mask = 0x1;
    while (mask < comm_size) {
        if (relative_rank & mask) {
            src = rank - mask;
            if (src < 0)
                src += comm_size;

            /* compute the exact recv_size to avoid writing this NBC in callback style*/
            recv_size = nbytes - (relative_rank * scatter_size);
            if (recv_size < 0)
                recv_size = 0;

            curr_size = recv_size;

            if (recv_size > 0) {
                mpi_errno = MPIR_Sched_recv(((char *)tmp_buf + relative_rank*scatter_size),
                                            recv_size, MPI_BYTE, src, comm_ptr, s);
                if (mpi_errno) MPIR_ERR_POP(mpi_errno);
                MPIR_SCHED_BARRIER(s);
            }
            break;
        }
        mask <<= 1;
    }

    /* This process is responsible for all processes that have bits
       set from the LSB upto (but not including) mask.  Because of
       the "not including", we start by shifting mask back down
       one. */

    mask >>= 1;
    while (mask > 0) {
        if (relative_rank + mask < comm_size) {
            send_size = curr_size - scatter_size * mask;
            /* mask is also the size of this process's subtree */

            if (send_size > 0) {
                dst = rank + mask;
                if (dst >= comm_size)
                    dst -= comm_size;
                mpi_errno = MPIR_Sched_send(((char *)tmp_buf + scatter_size*(relative_rank+mask)),
                                            send_size, MPI_BYTE, dst, comm_ptr, s);
                if (mpi_errno) MPIR_ERR_POP(mpi_errno);

                curr_size -= send_size;
            }
        }
        mask >>= 1;
    }

fn_exit:
    return mpi_errno;
fn_fail:
    goto fn_exit;
}

/*
   Broadcast based on a scatter followed by an allgather.

   We first scatter the buffer using a binomial tree algorithm. This costs
   lgp.alpha + n.((p-1)/p).beta
   If the datatype is contiguous and the communicator is homogeneous,
   we treat the data as bytes and divide (scatter) it among processes
   by using ceiling division. For the noncontiguous or heterogeneous
   cases, we first pack the data into a temporary buffer by using
   MPI_Pack, scatter it as bytes, and unpack it after the allgather.

   For the allgather, we use a recursive doubling algorithm for
   medium-size messages and power-of-two number of processes. This
   takes lgp steps. In each step pairs of processes exchange all the
   data they have (we take care of non-power-of-two situations). This
   costs approximately lgp.alpha + n.((p-1)/p).beta. (Approximately
   because it may be slightly more in the non-power-of-two case, but
   it's still a logarithmic algorithm.) Therefore, for long messages
   Total Cost = 2.lgp.alpha + 2.n.((p-1)/p).beta
*/
/* It would be nice to just call:
 * ----8<----
 * MPID_Iscatter_sched(...);
 * MPID_Iallgather_sched(...);
 * ----8<----
 *
 * But that results in inefficient additional memory allocation and copies
 * because the scatter doesn't know that we have the whole bcast buffer to use
 * as a temp buffer for forwarding.  Furthermore, there's no guarantee that
 * nbytes is a multiple of comm_size, and the regular scatter/allgather ops
 * can't cope with that case correctly.  We could use scatterv/allgatherv
 * instead, but that's not scalable in comm_size and still has the temporary
 * buffer problem.
 *
 * So we use a special-cased scatter (MPIR_Iscatter_for_bcast) that just handles
 * bytes and knows how to deal with a "ragged edge" vector length and we
 * implement the recursive doubling algorithm here.
 */
#undef FUNCNAME
#define FUNCNAME MPII_Ibcast_scatter_rec_dbl_allgather_sched
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
int MPII_Ibcast_scatter_rec_dbl_allgather_sched(void *buffer, int count, MPI_Datatype datatype, int root, MPIR_Comm *comm_ptr, MPIR_Sched_t s)
{
    int mpi_errno = MPI_SUCCESS;
    int rank, comm_size, dst;
    int relative_rank, mask;
    int scatter_size, nbytes, curr_size, incoming_count;
    int type_size, j, k, i, tmp_mask, is_contig, is_homogeneous ATTRIBUTE((unused));
    int relative_dst, dst_tree_root, my_tree_root, send_offset;
    int recv_offset, tree_root, nprocs_completed, offset;
    MPI_Aint true_extent, true_lb;
    void *tmp_buf;
    struct MPII_Ibcast_status *status;
    MPIR_SCHED_CHKPMEM_DECL(2);

    comm_size = comm_ptr->local_size;
    rank = comm_ptr->rank;
    relative_rank = (rank >= root) ? rank - root : rank - root + comm_size;

    /* If there is only one process, return */
    if (comm_size == 1)
        goto fn_exit;

    if (HANDLE_GET_KIND(datatype) == HANDLE_KIND_BUILTIN) {
        is_contig = 1;
    }
    else {
        MPIR_Datatype_is_contig(datatype, &is_contig);
    }

    MPIR_SCHED_CHKPMEM_MALLOC(status, struct MPII_Ibcast_status*,
                              sizeof(struct MPII_Ibcast_status), mpi_errno, "MPI_Status", MPL_MEM_BUFFER);
    is_homogeneous = 1;
#ifdef MPID_HAS_HETERO
    if (comm_ptr->is_hetero)
        is_homogeneous = 0;
#endif
    MPIR_Assert(is_homogeneous); /* we don't handle the hetero case right now */

    MPIR_Datatype_get_size_macro(datatype, type_size);

    nbytes = type_size * count;
    status->n_bytes = nbytes;
    status->curr_bytes = 0;
    if (is_contig) {
        /* contiguous and homogeneous. no need to pack. */
        MPIR_Type_get_true_extent_impl(datatype, &true_lb, &true_extent);

        tmp_buf = (char *)buffer + true_lb;
    }
    else {
        MPIR_SCHED_CHKPMEM_MALLOC(tmp_buf, void *, nbytes, mpi_errno, "tmp_buf", MPL_MEM_BUFFER);

        /* TODO: Pipeline the packing and communication */
        if (rank == root) {
            mpi_errno = MPIR_Sched_copy(buffer, count, datatype,
                                        tmp_buf, nbytes, MPI_BYTE, s);
            if (mpi_errno) MPIR_ERR_POP(mpi_errno);
            MPIR_SCHED_BARRIER(s);
        }
    }


    mpi_errno = MPII_Iscatter_for_bcast_sched(tmp_buf, root, comm_ptr, nbytes, s);
    if (mpi_errno) MPIR_ERR_POP(mpi_errno);

    /* this is the block size used for the scatter operation */
    scatter_size = (nbytes + comm_size - 1) / comm_size; /* ceiling division */

    /* curr_size is the amount of data that this process now has stored in
     * buffer at byte offset (relative_rank*scatter_size) */
    curr_size = MPL_MIN(scatter_size, (nbytes - (relative_rank * scatter_size)));
    if (curr_size < 0)
        curr_size = 0;
    /* curr_size bytes already inplace */
    status->curr_bytes = curr_size;

    /* initialize because the compiler can't tell that it is always initialized when used */
    incoming_count = -1;

    mask = 0x1;
    i = 0;
    while (mask < comm_size) {
        relative_dst = relative_rank ^ mask;

        dst = (relative_dst + root) % comm_size;

        /* find offset into send and recv buffers.
           zero out the least significant "i" bits of relative_rank and
           relative_dst to find root of src and dst
           subtrees. Use ranks of roots as index to send from
           and recv into  buffer */

        dst_tree_root = relative_dst >> i;
        dst_tree_root <<= i;

        my_tree_root = relative_rank >> i;
        my_tree_root <<= i;

        send_offset = my_tree_root * scatter_size;
        recv_offset = dst_tree_root * scatter_size;

        if (relative_dst < comm_size) {
            /* calculate the exact amount of data to be received */
            /* alternative */
            if ((nbytes - recv_offset) > 0)
                incoming_count = MPL_MIN((nbytes-recv_offset), (mask * scatter_size));
            else
                incoming_count = 0;

            mpi_errno = MPIR_Sched_send(((char *)tmp_buf + send_offset),
                                        curr_size, MPI_BYTE, dst, comm_ptr, s);
            if (mpi_errno) MPIR_ERR_POP(mpi_errno);
            /* sendrecv, no barrier */
            mpi_errno = MPIR_Sched_recv_status(((char *)tmp_buf + recv_offset),
                                        incoming_count,
                                        MPI_BYTE, dst, comm_ptr,&status->status, s);
            if (mpi_errno) MPIR_ERR_POP(mpi_errno);
            MPIR_SCHED_BARRIER(s);
            mpi_errno = MPIR_Sched_cb(&MPII_sched_add_length, status, s);
            if (mpi_errno) MPIR_ERR_POP(mpi_errno);
            MPIR_SCHED_BARRIER(s);

            curr_size += incoming_count;
        }

        /* if some processes in this process's subtree in this step
           did not have any destination process to communicate with
           because of non-power-of-two, we need to send them the
           data that they would normally have received from those
           processes. That is, the haves in this subtree must send to
           the havenots. We use a logarithmic recursive-halfing algorithm
           for this. */

        /* This part of the code will not currently be
           executed because we are not using recursive
           doubling for non power of two. Mark it as experimental
           so that it doesn't show up as red in the coverage tests. */

        /* --BEGIN EXPERIMENTAL-- */
        if (dst_tree_root + mask > comm_size) {
            nprocs_completed = comm_size - my_tree_root - mask;
            /* nprocs_completed is the number of processes in this
               subtree that have all the data. Send data to others
               in a tree fashion. First find root of current tree
               that is being divided into two. k is the number of
               least-significant bits in this process's rank that
               must be zeroed out to find the rank of the root */
            j = mask;
            k = 0;
            while (j) {
                j >>= 1;
                k++;
            }
            k--;

            offset = scatter_size * (my_tree_root + mask);
            tmp_mask = mask >> 1;

            while (tmp_mask) {
                relative_dst = relative_rank ^ tmp_mask;
                dst = (relative_dst + root) % comm_size;

                tree_root = relative_rank >> k;
                tree_root <<= k;

                /* send only if this proc has data and destination
                   doesn't have data. */

                if ((relative_dst > relative_rank) &&
                    (relative_rank < tree_root + nprocs_completed) &&
                    (relative_dst >= tree_root + nprocs_completed))
                {
                    /* incoming_count was set in the previous
                       receive. that's the amount of data to be
                       sent now. */
                    mpi_errno = MPIR_Sched_send(((char *)tmp_buf + offset),
                                                incoming_count, MPI_BYTE, dst, comm_ptr, s);
                    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
                    MPIR_SCHED_BARRIER(s);
                }
                /* recv only if this proc. doesn't have data and sender
                   has data */
                else if ((relative_dst < relative_rank) &&
                         (relative_dst < tree_root + nprocs_completed) &&
                         (relative_rank >= tree_root + nprocs_completed))
                {
                    /* recalculate incoming_count, since not all processes will have
                     * this value */
                    if ((nbytes - offset) > 0)
                        incoming_count = MPL_MIN((nbytes-offset), (mask * scatter_size));
                    else
                        incoming_count = 0;

                    /* nprocs_completed is also equal to the no. of processes
                       whose data we don't have */
                    mpi_errno = MPIR_Sched_recv_status(((char *)tmp_buf + offset),
                                                incoming_count, MPI_BYTE, dst, comm_ptr,
                                                &status->status, s);
                    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
                    MPIR_SCHED_BARRIER(s);
                    mpi_errno = MPIR_Sched_cb(&MPII_sched_add_length, status, s);
                    if (mpi_errno) MPIR_ERR_POP(mpi_errno);
                    MPIR_SCHED_BARRIER(s);

                    curr_size += incoming_count;
                }
                tmp_mask >>= 1;
                k--;
            }
        }
        /* --END EXPERIMENTAL-- */

        mask <<= 1;
        i++;
    }
    if(is_homogeneous){
        mpi_errno = MPIR_Sched_cb(&MPII_sched_test_curr_length, status, s);
        if (mpi_errno) MPIR_ERR_POP(mpi_errno);
    }
    if (!is_contig) {
        if (rank != root) {
            mpi_errno = MPIR_Sched_copy(tmp_buf, nbytes, MPI_BYTE,
                                        buffer, count, datatype, s);
            if (mpi_errno) MPIR_ERR_POP(mpi_errno);
        }
    }

    MPIR_SCHED_CHKPMEM_COMMIT(s);
fn_exit:
    return mpi_errno;
fn_fail:
    MPIR_SCHED_CHKPMEM_REAP(s);
    goto fn_exit;
}

