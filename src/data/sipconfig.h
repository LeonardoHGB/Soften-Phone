#pragma once
//
// sipconfig.h — Configuracao da conta SIP (porte de SipConfig.cs).
// Persistencia (JSON + senha via DPAPI) entra na fase 6; aqui fica a estrutura
// e as regras puras (isComplete, domain). Campos ExpirySeconds/KeepAliveSeconds/
// DisplayName sao mantidos por paridade, mesmo nao sendo propagados ao motor.
//
#include <QString>

namespace sphone {

struct SipConfig {
    QString server;
    int     port = 5060;
    QString username;
    QString password;          // so memoria; persistencia cifra (fase 6)
    QString displayName;       // persistido, NAO usado no registro (paridade)
    int     expirySeconds = 120;     // nao propagado ao motor (paridade)
    int     keepAliveSeconds = 15;   // nao propagado; motor usa ka_interval=15 fixo
    bool    darkTheme = true;     // base da campanha "Rumo ao Hexa" e o tema escuro

    // Dispositivos de audio escolhidos pelo atendente (gravados pelo NOME, nao pelo
    // indice: o id muda entre boots / ao plugar-desplugar USB; o nome e estavel e
    // portavel). Vazio = padrao do sistema (o motor cai em PJMEDIA_AUD_DEFAULT_*).
    QString captureDevice;     // microfone (captura)
    QString playbackDevice;    // alto-falante/fone (reproducao)

    // Auto-atendimento: atende sozinho 1s apos comecar a tocar (com chime de
    // aviso ao atendente). Ligado por padrao; opcional via Config.
    bool    autoAnswer = true;

    bool isComplete() const {
        return !server.isEmpty() && !username.isEmpty() && !password.isEmpty();
    }

    // Dominio das URIs SIP: inclui :porta apenas quando != 5060.
    QString domain() const {
        return port == 5060 ? server : QStringLiteral("%1:%2").arg(server).arg(port);
    }

    // Persistencia em %LOCALAPPDATA%\SPHONE\sphone.json. A senha NUNCA e gravada
    // em texto puro: cifrada com DPAPI (CryptProtectData, por usuario do Windows).
    static SipConfig load();
    void save() const;
};

}  // namespace sphone
