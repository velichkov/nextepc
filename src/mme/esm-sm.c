/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nas/nas-message.h"
#include "mme-event.h"
#include "mme-timer.h"
#include "mme-sm.h"
#include "mme-fd-path.h"
#include "emm-handler.h"
#include "esm-build.h"
#include "esm-handler.h"
#include "mme-s11-handler.h"
#include "nas-path.h"
#include "mme-gtp-path.h"

#undef OGS_LOG_DOMAIN
#define OGS_LOG_DOMAIN __esm_log_domain

void esm_state_initial(ogs_fsm_t *s, mme_event_t *e)
{
    ogs_assert(s);

    mme_sm_debug(e);

    OGS_FSM_TRAN(s, &esm_state_inactive);
}

void esm_state_final(ogs_fsm_t *s, mme_event_t *e)
{
    ogs_assert(s);

    mme_sm_debug(e);
}

void esm_state_inactive(ogs_fsm_t *s, mme_event_t *e)
{
    int rv;
    mme_ue_t *mme_ue = NULL;
    mme_sess_t *sess = NULL;
    mme_bearer_t *bearer = NULL;
    nas_message_t *message = NULL;

    ogs_assert(s);
    ogs_assert(e);

    mme_sm_debug(e);

    bearer = e->bearer;
    ogs_assert(bearer);
    sess = bearer->sess;
    ogs_assert(sess);
    mme_ue = sess->mme_ue;
    ogs_assert(mme_ue);

    switch (e->id) {
    case OGS_FSM_ENTRY_SIG:
        CLEAR_BEARER_ALL_TIMERS(bearer);
        break;
    case OGS_FSM_EXIT_SIG:
        break;
    case MME_EVT_ESM_MESSAGE:
        message = e->nas_message;
        ogs_assert(message);

        switch (message->esm.h.message_type) {
        case NAS_PDN_CONNECTIVITY_REQUEST:
            ogs_debug("[ESM] PDN Connectivity request");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);
            rv = esm_handle_pdn_connectivity_request(
                    bearer, &message->esm.pdn_connectivity_request);
            if (rv != OGS_OK) {
                OGS_FSM_TRAN(s, esm_state_exception);
                break;
            }
            break;
        case NAS_ESM_INFORMATION_RESPONSE:
            ogs_debug("[ESM] ESM information response");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);

            CLEAR_BEARER_TIMER(bearer->t3489);
            rv = esm_handle_information_response(
                    sess, &message->esm.esm_information_response);
            if (rv != OGS_OK) {
                OGS_FSM_TRAN(s, esm_state_exception);
                break;
            }
            break;
        case NAS_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT:
            ogs_debug("[ESM] Activate default EPS bearer "
                    "context accept");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);
            /* Check if Initial Context Setup Response or 
             *          E-RAB Setup Response is received */
            if (MME_HAVE_ENB_S1U_PATH(bearer)) {
                rv = mme_gtp_send_modify_bearer_request(bearer, 0);
                ogs_assert(rv == OGS_OK);
            }

            rv = nas_send_activate_all_dedicated_bearers(bearer);
            ogs_assert(rv == OGS_OK);

            OGS_FSM_TRAN(s, esm_state_active);
            break;
        case NAS_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT:
            ogs_debug("[ESM] Activate dedicated EPS bearer "
                    "context accept");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);
            /* Check if Initial Context Setup Response or 
             *          E-RAB Setup Response is received */
            if (MME_HAVE_ENB_S1U_PATH(bearer)) {
                rv = mme_gtp_send_create_bearer_response(bearer);
                ogs_assert(rv == OGS_OK);
            }

            OGS_FSM_TRAN(s, esm_state_active);
            break;
        default:
            ogs_error("Unknown message(type:%d)", 
                    message->esm.h.message_type);
            break;
        }
        break;
    case MME_EVT_ESM_TIMER:
        switch (e->timer_id) {
        case MME_TIMER_T3489:
            if (bearer->t3489.retry_count >=
                    mme_timer_cfg(MME_TIMER_T3489)->max_count) {
                ogs_warn("[EMM] Retransmission of IMSI[%s] failed. "
                        "Stop retransmission",
                        mme_ue->imsi_bcd);
                OGS_FSM_TRAN(&bearer->sm, &esm_state_exception);

                nas_send_pdn_connectivity_reject(sess,
                    ESM_CAUSE_ESM_INFORMATION_NOT_RECEIVED);
            } else {
                bearer->t3489.retry_count++;
                nas_send_esm_information_request(bearer);
            }
            break;
        default:
            ogs_error("Unknown timer[%s:%d]",
                    mme_timer_get_name(e->timer_id), e->timer_id);
            break;
        }
        break;
    default:
        ogs_error("Unknown event %s", mme_event_get_name(e));
        break;
    }
}

void esm_state_active(ogs_fsm_t *s, mme_event_t *e)
{
    int rv;
    mme_ue_t *mme_ue = NULL;
    mme_sess_t *sess = NULL;
    mme_bearer_t *bearer = NULL;
    nas_message_t *message = NULL;

    ogs_assert(s);
    ogs_assert(e);

    mme_sm_debug(e);

    bearer = e->bearer;
    ogs_assert(bearer);
    sess = bearer->sess;
    ogs_assert(sess);
    mme_ue = sess->mme_ue;
    ogs_assert(mme_ue);

    switch (e->id) {
    case OGS_FSM_ENTRY_SIG:
        break;
    case OGS_FSM_EXIT_SIG:
        break;
    case MME_EVT_ESM_MESSAGE:
        message = e->nas_message;
        ogs_assert(message);

        switch (message->esm.h.message_type) {
        case NAS_PDN_CONNECTIVITY_REQUEST:
            ogs_debug("[ESM] PDN Connectivity request");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);
            rv = esm_handle_pdn_connectivity_request(
                    bearer, &message->esm.pdn_connectivity_request);
            if (rv != OGS_OK) {
                OGS_FSM_TRAN(s, esm_state_exception);
                break;
            }

            OGS_FSM_TRAN(s, esm_state_inactive);
            break;
        case NAS_PDN_DISCONNECT_REQUEST:
            ogs_debug("[ESM] PDN disconnect request");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);
            if (MME_HAVE_SGW_S1U_PATH(sess)) {
                rv = mme_gtp_send_delete_session_request(sess);
                ogs_assert(rv == OGS_OK);
            } else {
                rv = nas_send_deactivate_bearer_context_request(bearer);
                ogs_assert(rv == OGS_OK);
            }
            OGS_FSM_TRAN(s, esm_state_pdn_will_disconnect);
            break;
        case NAS_MODIFY_EPS_BEARER_CONTEXT_ACCEPT:
            ogs_debug("[ESM] Modify EPS bearer context accept");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);

            rv = mme_gtp_send_update_bearer_response(bearer);
            ogs_assert(rv == OGS_OK);
            break;
        case NAS_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT:
            ogs_debug("[ESM] [A] Deactivate EPS bearer "
                    "context accept");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);
            rv = mme_gtp_send_delete_bearer_response(bearer);
            ogs_assert(rv == OGS_OK);
            OGS_FSM_TRAN(s, esm_state_bearer_deactivated);
            break;
        default:
            ogs_error("Unknown message(type:%d)", 
                    message->esm.h.message_type);
            break;
        }
        break;
    default:
        ogs_error("Unknown event %s", mme_event_get_name(e));
        break;
    }
}

void esm_state_pdn_will_disconnect(ogs_fsm_t *s, mme_event_t *e)
{
    mme_ue_t *mme_ue = NULL;
    mme_sess_t *sess = NULL;
    mme_bearer_t *bearer = NULL;
    nas_message_t *message = NULL;

    ogs_assert(s);
    ogs_assert(e);

    mme_sm_debug(e);

    bearer = e->bearer;
    ogs_assert(bearer);
    sess = bearer->sess;
    ogs_assert(sess);
    mme_ue = sess->mme_ue;
    ogs_assert(mme_ue);

    switch (e->id) {
    case OGS_FSM_ENTRY_SIG:
        break;
    case OGS_FSM_EXIT_SIG:
        break;
    case MME_EVT_ESM_MESSAGE:
        message = e->nas_message;
        ogs_assert(message);

        switch (message->esm.h.message_type) {
        case NAS_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT:
            ogs_debug("[ESM] [D] Deactivate EPS bearer "
                    "context accept");
            ogs_debug("    IMSI[%s] PTI[%d] EBI[%d]",
                    mme_ue->imsi_bcd, sess->pti, bearer->ebi);
            OGS_FSM_TRAN(s, esm_state_pdn_did_disconnect);
            break;
        default:
            ogs_error("Unknown message(type:%d)", 
                    message->esm.h.message_type);
            break;
        }
        break;

    default:
        ogs_error("Unknown event %s", mme_event_get_name(e));
        break;
    }
}

void esm_state_pdn_did_disconnect(ogs_fsm_t *s, mme_event_t *e)
{
    ogs_assert(e);
    mme_sm_debug(e);

    switch (e->id) {
    case OGS_FSM_ENTRY_SIG:
        break;
    case OGS_FSM_EXIT_SIG:
        break;
    default:
        ogs_error("Unknown event %s", mme_event_get_name(e));
        break;
    }
}

void esm_state_bearer_deactivated(ogs_fsm_t *s, mme_event_t *e)
{
    ogs_assert(e);
    mme_sm_debug(e);

    switch (e->id) {
    case OGS_FSM_ENTRY_SIG:
        break;
    case OGS_FSM_EXIT_SIG:
        break;
    default:
        ogs_error("Unknown event %s", mme_event_get_name(e));
        break;
    }
}

void esm_state_exception(ogs_fsm_t *s, mme_event_t *e)
{
    mme_bearer_t *bearer = NULL;
    ogs_assert(e);
    mme_sm_debug(e);

    bearer = e->bearer;

    switch (e->id) {
    case OGS_FSM_ENTRY_SIG:
        CLEAR_BEARER_ALL_TIMERS(bearer);
        break;
    case OGS_FSM_EXIT_SIG:
        break;
    default:
        ogs_error("Unknown event %s", mme_event_get_name(e));
        break;
    }
}
