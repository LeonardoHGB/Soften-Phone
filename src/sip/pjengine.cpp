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

sphone::PjEngine* g_self  = nullptr;
pjsua_acc_id      g_acc   = PJSUA_INVALID_ID;
// Estado de mudo da chamada ativa. Precisa ser visivel aos callbacks: re-INVITEs
// (hold/unhold, session timers RFC 4028) disparam on_call_media_state de novo e
// reconectariam o microfone, desfazendo o mudo silenciosamente.
bool              g_muted = false;

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

    QString sip_call_id;
    if (pjsua_call_get_info(call_id, &ci) == PJ_SUCCESS) {
        int n = (int)(ci.remote_info.slen < 255 ? ci.remote_info.slen : 255);
        if (n > 0) memcpy(from, ci.remote_info.ptr, n);
        sip_call_id = QString::fromUtf8(ci.call_id.ptr, (int)ci.call_id.slen);
    }
    // 180 Ringing imediato (sem isso a perna fica "muda" para o PABX/fila).
    pjsua_call_answer(call_id, 180, NULL, NULL);

    if (g_self) g_self->emitIncoming((int)call_id, QString::fromUtf8(from), sip_call_id);
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
        if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
            g_muted = false;   // proxima chamada comeca sempre com o microfone aberto
            if (is_completed_elsewhere(e))
                flags |= sphone::PjEngine::FlagCompletedElsewhere;
        }
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
        if (!g_muted)                          // respeita o mudo apos re-INVITE
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

    // Cancelamento de eco acustico WebRTC AEC3 (melhor algoritmo). Habilitado no
    // build do PJSIP via config_site.h (PJMEDIA_HAS_WEBRTC_AEC3=1). Sem isto, o
    // softphone de mesa devolve a voz do remoto captada pelo mic -> quem liga
    // ouve a propria voz. O AEC3 exige clock de 16/32/48k (o PJSUA usa 16k).
    media_cfg.ec_tail_len = 200;   // ms; o AEC3 gerencia a cauda internamente
    media_cfg.ec_options  = PJMEDIA_ECHO_WEBRTC_AEC3 | PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR;

    // Mantem o device de audio "quente" por 30s apos a ultima chamada: elimina o
    // warm-up (underflow + reset de jitter + re-prefetch do EC) que picotava o
    // INICIO de cada atendimento ("corta quando alguem liga"). 30s (e nao -1)
    // deixa o WMME re-vincular ao device default ATUAL caso o atendente troque de
    // headset durante o turno, em vez de prender pra sempre o device do startup.
    media_cfg.snd_auto_close_time = 30;

    // TX continuo: desliga o VAD/silence-suppression local, que cortava o inicio
    // das palavras na perna que ENVIAMOS (o cliente nos ouvindo). Em LAN o trafego
    // continuo (G.711/GSM) e irrelevante.
    media_cfg.no_vad = PJ_TRUE;

    // Piso do jitter buffer de RECEPCAO (ms): suaviza a retomada de fala depois
    // dos silencios que o Asterisk suprime. Nao cura o silence-suppression do PABX
    // (pacote nunca enviado nao da pra bufferizar), so reduz o glitch da 1a silaba.
    media_cfg.jb_init    = 80;
    media_cfg.jb_min_pre = 40;
    media_cfg.jb_max_pre = 160;
    media_cfg.jb_max     = 240;

    if (pjsua_init(&cfg, &log_cfg, &media_cfg) != PJ_SUCCESS) { pjsua_destroy(); return -2; }

    pjsua_transport_config tcfg;
    pjsua_transport_config_default(&tcfg);
    tcfg.port = port;   // 0 = porta efemera
    if (pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tcfg, NULL) != PJ_SUCCESS) {
        pjsua_destroy(); return -3;
    }
    if (pjsua_start() != PJ_SUCCESS) { pjsua_destroy(); return -4; }

    // Numa LAN sobra banda para G.711 (64kbps): forca PCMA/PCMU e DESABILITA o GSM
    // (13kbps, lossy, com PLC pior) que o pjsua ofertava primeiro -> voz mais limpa.
    // Em chamada SAINTE o Asterisk responde pela ordem do allow= DELE, entao apenas
    // nao ofertar GSM ja garante G.711; em ENTRANTE quem responde somos nos, e a
    // prioridade vale direto. O PABX anuncia PCMA(8) e PCMU(0), entao e seguro.
    {
        pj_str_t c;
        c = pj_str((char*)"PCMA/8000");   pjsua_codec_set_priority(&c, 254);
        c = pj_str((char*)"PCMU/8000");   pjsua_codec_set_priority(&c, 253);
        c = pj_str((char*)"GSM/8000");    pjsua_codec_set_priority(&c, 0);
        c = pj_str((char*)"speex/16000"); pjsua_codec_set_priority(&c, 0);
        c = pj_str((char*)"speex/8000");  pjsua_codec_set_priority(&c, 0);
        c = pj_str((char*)"iLBC/8000");   pjsua_codec_set_priority(&c, 0);
    }

    // Pre-aquece o dispositivo de audio + AEC3 ja no startup: assim a PRIMEIRA
    // chamada nao paga o custo de abrir o device e instanciar o AEC3 (era a unica
    // que demorava). Best-effort: se falhar, segue (abre na 1a chamada como antes).
    pjsua_set_snd_dev(PJMEDIA_AUD_DEFAULT_CAPTURE_DEV, PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV);
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

    // SIP session timers (RFC 4028): rede de seguranca contra "chamada zumbi".
    // Em deploy cross-subnet/VPN o BYE que o PABX envia quando o outro lado desliga
    // as vezes NAO chega ao softphone (o caminho PABX->ramal perde o in-dialog) e,
    // sem isto, a chamada fica ativa PARA SEMPRE no nosso lado. Com o timer ligado,
    // a cada ~sess_expires/2 negociamos um refresh (re-INVITE/UPDATE): se o outro
    // lado sumiu, o refresh recebe 481/timeout e a pilha DERRUBA a chamada sozinha.
    // ALWAYS = roda o timer mesmo que o peer nao sinalize suporte (nao usa
    // "Require: timer", entao nao quebra chamada) -> teardown garantido
    // independente da config de session-timers do Asterisk.
    acfg.use_timer = PJSUA_SIP_TIMER_ALWAYS;
    acfg.timer_setting.min_se       = 90;    // minimo do RFC 4028 (segundos)
    acfg.timer_setting.sess_expires = 120;   // refresh ~60s -> zumbi cai em ate ~2min

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
    g_muted = mute;   // persiste para cb_call_media_state nao reverter num re-INVITE
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

QString PjEngine::sipCallId(int callId) {
    ensure_thread();
    pjsua_call_info ci;
    if (pjsua_call_get_info((pjsua_call_id)callId, &ci) != PJ_SUCCESS) return QString();
    return QString::fromUtf8(ci.call_id.ptr, (int)ci.call_id.slen);
}

QList<AudioDevice> PjEngine::audioDevices() {
    QList<AudioDevice> out;
    if (g_self != this) return out;        // motor nao iniciado por esta instancia
    ensure_thread();

    // Re-scan: capta headset plugado/desplugado depois do startup (o atendente abre
    // Configuracoes justamente apos trocar de fone).
    pjmedia_aud_dev_refresh();

    pjmedia_aud_dev_info info[64];
    unsigned count = PJ_ARRAY_SIZE(info);
    if (pjsua_enum_aud_devs(info, &count) != PJ_SUCCESS) return out;

    for (unsigned i = 0; i < count; ++i) {
        AudioDevice d;
        d.id       = (int)i;               // pjsua_enum_aud_devs lista por indice 0..n-1
        d.name     = QString::fromUtf8(info[i].name);
        d.capture  = info[i].input_count  > 0;
        d.playback = info[i].output_count > 0;
        out.append(d);
    }
    return out;
}

void PjEngine::setSoundDevices(const QString& captureName, const QString& playbackName) {
    if (g_self != this) return;
    ensure_thread();

    pjmedia_aud_dev_index capId  = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    pjmedia_aud_dev_index playId = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

    if (!captureName.isEmpty() || !playbackName.isEmpty()) {
        pjmedia_aud_dev_info info[64];
        unsigned count = PJ_ARRAY_SIZE(info);
        if (pjsua_enum_aud_devs(info, &count) == PJ_SUCCESS) {
            for (unsigned i = 0; i < count; ++i) {
                const QString n = QString::fromUtf8(info[i].name);
                if (!captureName.isEmpty()  && info[i].input_count  > 0 && n == captureName)
                    capId = (pjmedia_aud_dev_index)i;
                if (!playbackName.isEmpty() && info[i].output_count > 0 && n == playbackName)
                    playId = (pjmedia_aud_dev_index)i;
            }
        }
    }
    // Best-effort: se o device sumiu (id nao resolvido), segue com o default do SO.
    pjsua_set_snd_dev(capId, playId);
}

bool PjEngine::getStats(int callId, QString& codec, int& clockRate, int& rttMs, int& lossPermil) {
    ensure_thread();
    pjsua_stream_info si;
    if (pjsua_call_get_stream_info((pjsua_call_id)callId, 0, &si) != PJ_SUCCESS) return false;
    if (si.type != PJMEDIA_TYPE_AUDIO) return false;

    const pjmedia_codec_info& f = si.info.aud.fmt;
    codec     = QString::fromUtf8(f.encoding_name.ptr, (int)f.encoding_name.slen);
    clockRate = (int)f.clock_rate;

    pjsua_stream_stat st;
    if (pjsua_call_get_stream_stat((pjsua_call_id)callId, 0, &st) == PJ_SUCCESS) {
        rttMs = (int)(st.rtcp.rtt.last / 1000);   // RTT (us) -> ms
        const unsigned pkt = st.rtcp.rx.pkt, loss = st.rtcp.rx.loss;
        lossPermil = (pkt + loss > 0) ? (int)((pj_uint64_t)loss * 1000 / (pkt + loss)) : 0;
    } else {
        rttMs = -1; lossPermil = -1;
    }
    return true;
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
bool PjEngine::getStats(int, QString&, int&, int&, int&)       { return false; }
QString PjEngine::sipCallId(int)                               { return QString(); }
QList<AudioDevice> PjEngine::audioDevices()                    { return {}; }
void PjEngine::setSoundDevices(const QString&, const QString&) {}

}  // namespace sphone

#endif
