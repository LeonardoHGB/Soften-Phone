#include "sip/pjengine.h"
#include "core/diag.h"

namespace sphone {

PjEngine::PjEngine(QObject* parent) : QObject(parent) {}
PjEngine::~PjEngine() { shutdown(); }

}  // namespace sphone

// ===========================================================================
//  Implementacao real (pjsua-lib). Porte 1:1 de native/pjcore.c.
// ===========================================================================
#if defined(SPHONE_WITH_PJSIP)

#include "core/paths.h"
#include <pjsua-lib/pjsua.h>
#include <QFile>
#include <string>

namespace {

sphone::PjEngine* g_self = nullptr;
pjsua_acc_id      g_acc  = PJSUA_INVALID_ID;

// Toda thread que chama a stack precisa estar registrada no PJLIB. As chamadas
// vem da thread de UI (sempre a mesma); registramos uma vez por thread.
void ensure_thread() {
    thread_local pj_thread_desc desc;
    thread_local pj_thread_t*   thread = nullptr;
    if (!pj_thread_is_registered())
        pj_thread_register("ui", desc, &thread);
}

// ---- Callbacks internos do pjsua (rodam em threads do PJSIP) ----

void cb_reg_state(pjsua_acc_id acc, pjsua_reg_info* info) {
    PJ_UNUSED_ARG(acc);
    if (g_self && info && info->cbparam) {
        int code = info->cbparam->code;
        g_self->emitReg((code / 100 == 2) ? 1 : 0, code);
    }
}

void cb_incoming_call(pjsua_acc_id acc, pjsua_call_id call_id, pjsip_rx_data* rdata) {
    pjsua_call_info ci;
    char from[256] = {0};
    PJ_UNUSED_ARG(acc);
    PJ_UNUSED_ARG(rdata);

    if (pjsua_call_get_info(call_id, &ci) == PJ_SUCCESS) {
        int n = (int)(ci.remote_info.slen < 255 ? ci.remote_info.slen : 255);
        if (n > 0) memcpy(from, ci.remote_info.ptr, n);
    }
    // 180 Ringing imediato (sem isso a perna fica "muda" para o PABX/fila).
    pjsua_call_answer(call_id, 180, NULL, NULL);

    if (g_self) g_self->emitIncoming((int)call_id, QString::fromUtf8(from));
}

// Termino por "atendida em outro ramal" (CANCEL com Reason: cause=200/elsewhere).
pj_bool_t is_completed_elsewhere(pjsip_event* e) {
    pjsip_rx_data* rdata = NULL;
    pj_str_t reason_name = pj_str((char*)"Reason");
    pj_str_t mark_else  = pj_str((char*)"elsewhere");
    pj_str_t mark_cause = pj_str((char*)"cause=200");

    if (!e) return PJ_FALSE;
    if (e->type == PJSIP_EVENT_TSX_STATE && e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
        rdata = e->body.tsx_state.src.rdata;
    else if (e->type == PJSIP_EVENT_RX_MSG)
        rdata = e->body.rx_msg.rdata;
    if (!rdata || !rdata->msg_info.msg) return PJ_FALSE;

    auto* reason = (pjsip_generic_string_hdr*)
        pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &reason_name, NULL);
    if (reason && (pj_stristr(&reason->hvalue, &mark_else) ||
                   pj_stristr(&reason->hvalue, &mark_cause)))
        return PJ_TRUE;
    return PJ_FALSE;
}

void cb_call_state(pjsua_call_id call_id, pjsip_event* e) {
    pjsua_call_info ci;
    int flags = 0;
    if (pjsua_call_get_info(call_id, &ci) == PJ_SUCCESS && g_self) {
        if (ci.state == PJSIP_INV_STATE_DISCONNECTED && is_completed_elsewhere(e))
            flags |= sphone::PjEngine::FlagCompletedElsewhere;
        g_self->emitCallState((int)call_id, (int)ci.state, (int)ci.last_status, flags);
    }
}

void cb_call_transfer_status(pjsua_call_id call_id, int st_code, const pj_str_t* st_text,
                             pj_bool_t final, pj_bool_t* p_cont) {
    PJ_UNUSED_ARG(st_text);
    PJ_UNUSED_ARG(final);
    if (st_code >= 100) {
        pjsua_call_hangup(call_id, 0, NULL, NULL);   // libera a perna local
        *p_cont = PJ_FALSE;                          // encerra a subscricao do REFER
    }
}

void cb_call_media_state(pjsua_call_id call_id) {
    pjsua_call_info ci;
    if (pjsua_call_get_info(call_id, &ci) == PJ_SUCCESS &&
        ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        pjsua_conf_connect(ci.conf_slot, 0);   // remoto    -> alto-falante
        pjsua_conf_connect(0, ci.conf_slot);   // microfone -> remoto
    }
}

}  // namespace

namespace sphone {

int PjEngine::start(int port) {
    g_self = this;

    if (pjsua_create() != PJ_SUCCESS) return -1;

    pjsua_config cfg;
    pjsua_config_default(&cfg);
    cfg.cb.on_reg_state2          = &cb_reg_state;
    cfg.cb.on_incoming_call       = &cb_incoming_call;
    cfg.cb.on_call_state          = &cb_call_state;
    cfg.cb.on_call_media_state    = &cb_call_media_state;
    cfg.cb.on_call_transfer_status = &cb_call_transfer_status;

    pjsua_logging_config log_cfg;
    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = 3;

    // Log SIP em arquivo SOMENTE com o flag %LOCALAPPDATA%\SPHONE\sipdebug.
    static std::string logPath;   // precisa viver apos a funcao (pjsua guarda o ponteiro)
    if (QFile::exists(paths::sipDebugFlag())) {
        logPath = paths::sipLogFile().toLocal8Bit().toStdString();
        log_cfg.log_filename = pj_str(const_cast<char*>(logPath.c_str()));
        log_cfg.level = 5;
        log_cfg.log_file_flags = PJ_O_APPEND;
    }

    pjsua_media_config media_cfg;
    pjsua_media_config_default(&media_cfg);

    if (pjsua_init(&cfg, &log_cfg, &media_cfg) != PJ_SUCCESS) { pjsua_destroy(); return -2; }

    pjsua_transport_config tcfg;
    pjsua_transport_config_default(&tcfg);
    tcfg.port = port;   // 0 = porta efemera
    if (pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tcfg, NULL) != PJ_SUCCESS) {
        pjsua_destroy(); return -3;
    }
    if (pjsua_start() != PJ_SUCCESS) { pjsua_destroy(); return -4; }
    return 0;
}

int PjEngine::registerAccount(const QString& domain, const QString& user, const QString& passwd) {
    ensure_thread();
    QByteArray u = user.toUtf8(), p = passwd.toUtf8();
    QByteArray idb  = ("sip:" + user + "@" + domain).toUtf8();
    QByteArray regb = ("sip:" + domain).toUtf8();

    pjsua_acc_config acfg;
    pjsua_acc_config_default(&acfg);
    acfg.id      = pj_str(idb.data());
    acfg.reg_uri = pj_str(regb.data());
    acfg.cred_count = 1;
    acfg.cred_info[0].realm     = pj_str((char*)"*");
    acfg.cred_info[0].scheme    = pj_str((char*)"digest");
    acfg.cred_info[0].username  = pj_str(u.data());
    acfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acfg.cred_info[0].data      = pj_str(p.data());
    acfg.ka_interval = 15;   // keep-alive de NAT nativo

    // pjsua_acc_add copia a config -> buffers locais sao seguros.
    if (pjsua_acc_add(&acfg, PJ_TRUE, &g_acc) != PJ_SUCCESS) return -1;
    return (int)g_acc;
}

int PjEngine::makeCall(const QString& domain, const QString& dest) {
    ensure_thread();
    QByteArray ub = ("sip:" + dest + "@" + domain).toUtf8();
    pj_str_t uri = pj_str(ub.data());
    pjsua_call_id cid;
    if (pjsua_call_make_call(g_acc, &uri, NULL, NULL, NULL, &cid) != PJ_SUCCESS) return -1;
    return (int)cid;
}

void PjEngine::answer(int callId)   { ensure_thread(); pjsua_call_answer((pjsua_call_id)callId, 200, NULL, NULL); }
void PjEngine::hangup(int callId)   { ensure_thread(); pjsua_call_hangup((pjsua_call_id)callId, 0, NULL, NULL); }
void PjEngine::hangupAll()          { ensure_thread(); pjsua_call_hangup_all(); }

void PjEngine::sendDtmf(int callId, const QString& digits) {
    ensure_thread();
    QByteArray d = digits.toUtf8();
    pj_str_t s = pj_str(d.data());
    pjsua_call_dial_dtmf((pjsua_call_id)callId, &s);
}

void PjEngine::hold(int callId)   { ensure_thread(); pjsua_call_set_hold((pjsua_call_id)callId, NULL); }
void PjEngine::unhold(int callId) { ensure_thread(); pjsua_call_reinvite((pjsua_call_id)callId, PJSUA_CALL_UNHOLD, NULL); }

int PjEngine::transfer(int callId, const QString& domain, const QString& dest) {
    ensure_thread();
    QByteArray ub = ("sip:" + dest + "@" + domain).toUtf8();
    pj_str_t uri = pj_str(ub.data());
    return pjsua_call_xfer((pjsua_call_id)callId, &uri, NULL) == PJ_SUCCESS ? 0 : -1;
}

void PjEngine::mute(int callId, bool mute) {
    ensure_thread();
    pjsua_call_info ci;
    if (pjsua_call_get_info((pjsua_call_id)callId, &ci) == PJ_SUCCESS &&
        ci.conf_slot != PJSUA_INVALID_ID) {
        if (mute) pjsua_conf_disconnect(0, ci.conf_slot);
        else      pjsua_conf_connect(0, ci.conf_slot);
    }
}

int PjEngine::getLevel(int callId, int* tx, int* rx) {
    ensure_thread();
    pjsua_call_info ci;
    unsigned txl = 0, rxl = 0;
    if (pjsua_call_get_info((pjsua_call_id)callId, &ci) != PJ_SUCCESS ||
        ci.conf_slot == PJSUA_INVALID_ID)
        return -1;
    pjsua_conf_get_signal_level(ci.conf_slot, &txl, &rxl);
    if (tx) *tx = (int)txl;
    if (rx) *rx = (int)rxl;
    return 0;
}

void PjEngine::unregister() {
    ensure_thread();
    if (g_acc != PJSUA_INVALID_ID) pjsua_acc_set_registration(g_acc, PJ_FALSE);
}

void PjEngine::shutdown() {
    if (g_self != this) return;        // nada iniciado por esta instancia
    ensure_thread();
    pjsua_destroy();
    g_acc = PJSUA_INVALID_ID;
    g_self = nullptr;
}

}  // namespace sphone

// ===========================================================================
//  Stub (sem PJSIP): permite compilar/rodar o shell visual.
// ===========================================================================
#else

namespace sphone {

int  PjEngine::start(int)                                      { sphone::diag::log("PjEngine stub: SPHONE_WITH_PJSIP=OFF"); return -1; }
int  PjEngine::registerAccount(const QString&, const QString&, const QString&) { return -1; }
void PjEngine::unregister()                                    {}
void PjEngine::shutdown()                                      {}
int  PjEngine::makeCall(const QString&, const QString&)        { return -1; }
void PjEngine::answer(int)                                     {}
void PjEngine::hangup(int)                                     {}
void PjEngine::hangupAll()                                     {}
void PjEngine::sendDtmf(int, const QString&)                   {}
void PjEngine::hold(int)                                       {}
void PjEngine::unhold(int)                                     {}
int  PjEngine::transfer(int, const QString&, const QString&)   { return -1; }
void PjEngine::mute(int, bool)                                 {}
int  PjEngine::getLevel(int, int*, int*)                       { return -1; }

}  // namespace sphone

#endif
