/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */
/*
 *
 */



#include "mpid_nem_impl.h"
#include "tofu_impl.h"

/* function prototypes */

static void MPID_nem_tofu_send_handler(void *cba,
		uint64_t *p_reqid);
static void MPID_nem_tofu_recv_handler(void *vp_vc,
		uint64_t raddr, void *buf, size_t bsz);

#undef FUNCNAME
#define FUNCNAME MPID_nem_tofu_poll
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int
MPID_nem_tofu_poll(int in_blocking_progress)
{
    int           mpi_errno = MPI_SUCCESS;
    MPIDI_STATE_DECL(MPID_STATE_MPID_NEM_TOFU_POLL);

    MPIDI_FUNC_ENTER(MPID_STATE_MPID_NEM_TOFU_POLL);

    {
	int rc;
	rc = llctofu_poll(in_blocking_progress,
		MPID_nem_tofu_send_handler,
		MPID_nem_tofu_recv_handler);
	if (rc != 0) {
	    mpi_errno = MPI_ERR_OTHER;
	    MPIU_ERR_POP(mpi_errno);
	}
    }

 fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPID_NEM_TOFU_POLL);
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME MPID_nem_tofu_send_handler
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
static void MPID_nem_tofu_send_handler(void *cba,
	    uint64_t *p_reqid)
{
    /* int mpi_errno = 0; */
    MPID_Request *sreq = cba; /* from llctofu_writev(,,,,cbarg,) */
    MPID_Request_kind_t	kind;      
    /* MPIDI_STATE_DECL(MPID_STATE_MPID_NEM_TOFU_SEND_HANDLER); */

    /* MPIDI_FUNC_ENTER(MPID_STATE_MPID_NEM_TOFU_SEND_HANDLER); */

    MPIU_DBG_MSG(CH3_CHANNEL, VERBOSE, "tofu_send_handler");

    MPIU_Assert(sreq != NULL);

    if (sreq == (void *)0xdeadbeefUL) {
        MPIDI_VC_t *vc = (void *)p_reqid[0];
        MPID_nem_tofu_vc_area *vc_tofu;
        
        MPIU_Assert(vc != NULL);
        /* printf("from credit %p (pg_rank %d)\n", vc, vc->pg_rank); */
        
        vc_tofu = VC_TOFU(vc);
        MPID_nem_tofu_send_queued(vc, &vc_tofu->send_queue);
        
        p_reqid[0] = ! MPIDI_CH3I_Sendq_empty(vc_tofu->send_queue);
        MPIU_DBG_MSG(CH3_CHANNEL, VERBOSE, "tofu_send_handler");
        MPIU_DBG_MSG_D(CH3_CHANNEL, VERBOSE,
                       "send queue %d", (unsigned int)p_reqid[0]);
        
        goto fn_exit;
    }
    
    kind = sreq->kind;
    switch (kind) {
        unsigned int reqtype;
    case MPID_REQUEST_SEND:
    case MPID_PREQUEST_SEND: {
        reqtype = MPIDI_Request_get_type(sreq);
        MPIU_Assert(reqtype != MPIDI_REQUEST_TYPE_GET_RESP);

        int is_contig;
        MPID_Datatype_is_contig(sreq->dev.datatype, &is_contig);
        if (!is_contig && REQ_FIELD(sreq, pack_buf)) {
            dprintf("tofu_send_handler,non-contiguous,free pack_buf\n");
            MPIU_Free(REQ_FIELD(req, pack_buf));
        }
        
        /* sreq: src/mpid/ch3/include/mpidpre.h */
        {
            MPIDI_VC_t *vc;
            int (*reqFn)(MPIDI_VC_t *vc, MPID_Request *sreq, int *complete);
            int complete;
            int r_mpi_errno;
            
            p_reqid[0] = 0 /* REQ_TOFU(sreq)->woff */;
            
            vc = sreq->ch.vc; /* before callback */
            reqFn = sreq->dev.OnDataAvail;
            if (reqFn == 0) {
                MPIDI_CH3U_Request_complete(sreq);
                MPIU_DBG_MSG(CH3_CHANNEL, VERBOSE, ".... complete");
            }
            else {
                complete = 0;
                r_mpi_errno = reqFn(vc, sreq, &complete);
                if (r_mpi_errno) MPIU_ERR_POP(r_mpi_errno);
                if (complete == 0) {
                    MPIU_Assert(complete == TRUE);
		}
                MPIU_DBG_MSG(CH3_CHANNEL, VERBOSE, ".... complete2");
            }
            
            /* push queued messages */
            {
                MPID_nem_tofu_vc_area *vc_tofu = VC_TOFU(vc);
                
                MPID_nem_tofu_send_queued(vc, &vc_tofu->send_queue);
            }
        }
        break; }
    default:
        break;
    }
    
 fn_exit:
    /* MPIDI_FUNC_EXIT(MPID_STATE_MPID_NEM_TOFU_SEND_HANDLER); */
    return /* mpi_errno */;
 fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME MPID_nem_tofu_recv_handler
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
static void MPID_nem_tofu_recv_handler(
    void *vp_vc,
    uint64_t raddr,
    void *buf,
    size_t bsz
)
{
    int mpi_errno = 0;
    MPIDI_VC_t *vc = vp_vc;
    /* MPIDI_STATE_DECL(MPID_STATE_MPID_NEM_TOFU_RECV_HANDLER); */

    /* MPIDI_FUNC_ENTER(MPID_STATE_MPID_NEM_TOFU_RECV_HANDLER); */

    MPIU_DBG_MSG(CH3_CHANNEL, VERBOSE, "tofu_recv_handler");

    {
	int pg_rank = (int) raddr;
	MPIDI_PG_t *pg = MPIDI_Process.my_pg;
	MPIDI_VC_t *vc_from_pg = 0;

	if (
	    (pg != 0)
	    && ((pg_rank >= 0) && (pg_rank < MPIDI_PG_Get_size(pg)))
	) {
	    /*
	     * MPIDI_Comm_get_vc_set_active(comm, rank, &vc);
	     */
	    MPIDI_PG_Get_vc_set_active(pg, pg_rank, &vc_from_pg);
	}
	else {
	    MPIU_DBG_MSG_D(CH3_CHANNEL, VERBOSE, "bad vc %p or", pg);
	    MPIU_DBG_MSG_D(CH3_CHANNEL, VERBOSE, "bad pg_rank %d", pg_rank);
	    MPIU_DBG_MSG_D(CH3_CHANNEL, VERBOSE, "bad pg_rank < %d",
		MPIDI_PG_Get_size(pg));
	    vc_from_pg = vc; /* XXX */
	}
	if (vc != vc_from_pg) {
	MPIU_DBG_MSG_D(CH3_CHANNEL, VERBOSE, "bad vc for pg_rank %d", pg_rank);
	}
	if (vc == 0) {
	    vc = vc_from_pg;
	}
    }
    if (vc != 0) {
        mpi_errno = MPID_nem_handle_pkt(vc, buf, bsz);
	if (mpi_errno != 0) {
	MPIU_DBG_MSG_D(CH3_CHANNEL, VERBOSE,
	    "MPID_nem_handle_pkt() = %d", mpi_errno);
	}
    }

 fn_exit:
    /* MPIDI_FUNC_EXIT(MPID_STATE_MPID_NEM_TOFU_RECV_HANDLER); */
    return ;
    //fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME MPID_nem_tofu_recv_posted
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPID_nem_tofu_recv_posted(struct MPIDI_VC *vc, struct MPID_Request *req)
{
    int mpi_errno = MPI_SUCCESS;
    int dt_contig;
    MPIDI_msg_sz_t data_sz;
    MPID_Datatype *dt_ptr;
    MPI_Aint dt_true_lb;

    MPIDI_STATE_DECL(MPID_STATE_MPID_NEM_TOFU_RECV_POSTED);
    MPIDI_FUNC_ENTER(MPID_STATE_MPID_NEM_TOFU_RECV_POSTED);

    /* req->dev.datatype is set in MPIDI_CH3U_Recvq_FDU_or_AEP (in src/mpid/ch3/src/ch3u_recvq.c) */
    MPIDI_Datatype_get_info(req->dev.user_count, req->dev.datatype, dt_contig, data_sz, dt_ptr,
                            dt_true_lb);

    /* stash vc for ib_poll */
    req->ch.vc = vc;

    void *write_to_buf;
    if (dt_contig) {
        write_to_buf = (void *) ((char *) req->dev.user_buf /*+ REQ_FIELD(req, lmt_dt_true_lb) */);
    }
    else {
        REQ_FIELD(req, pack_buf) = MPIU_Malloc(data_sz);
        MPIU_ERR_CHKANDJUMP(!REQ_FIELD(req, pack_buf), mpi_errno, MPI_ERR_OTHER,
                            "**outofmemory");
        write_to_buf = REQ_FIELD(req, pack_buf);
    }

    cmd[0].opcode = LLC_OPCODE_RECV;
    cmd[0].comm = LLC_COMM_WORLD;
    cmd[0].rank = VC_FIELD(vc, remote_endpoint_addr);
    
    ((MPIDI_Message_match_parts_t*)(&cmd[0].match.bits))->rank = comm->rank;
    ((MPIDI_Message_match_parts_t*)(&cmd[0].match.bits))->tag = tag;
 	((MPIDI_Message_match_parts_t*)(&cmd[0].match.bits))->context_id = comm->context_id + context_offset;
    MPIU_Assert(sizeof(LLC_match_t) >= sizeof(MPIDI_Message_match_parts_t));
    memset((uint8_t*)&cmd[0].match.bits + sizeof(MPIDI_Message_match_parts_t),
           0, sizeof(LLC_match_t) - sizeof(MPIDI_Message_match_parts_t));

 
    cmd[0].iov_local[0].addr = (uint64_t)write_to_buf;
    cmd[0].iov_local[0].length = data_sz;
    cmd[0].niov_local = 1;
    
    cmd[0].iov_remote[0].addr = 0;
    cmd[0].iov_remote[0].length = data_sz;;
    cmd[0].niov_remote = 1;
    
    llc_errno = LLC_post(cmd, 1);
    ERR_CHKANDJUMP(llc_errno != LLC_SUCCESS, -1, printf("LLC_post failed\n"));

  fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPID_NEM_TOFU_RECV_POSTED);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}