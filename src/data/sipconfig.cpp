#include "data/sipconfig.h"
#include "core/paths.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <wincrypt.h>   // CryptProtectData / CryptUnprotectData (crypt32)
#endif

namespace sphone {

// ---- DPAPI (cifra por usuario do Windows) ----------------------------------

static QString protect(const QString& plain) {
    if (plain.isEmpty()) return QString();
#ifdef Q_OS_WIN
    QByteArray in = plain.toUtf8();
    DATA_BLOB din;
    din.pbData = reinterpret_cast<BYTE*>(in.data());
    din.cbData = static_cast<DWORD>(in.size());
    DATA_BLOB dout{};
    if (!CryptProtectData(&din, nullptr, nullptr, nullptr, nullptr, 0, &dout))
        return QString();
    QByteArray enc(reinterpret_cast<const char*>(dout.pbData), int(dout.cbData));
    LocalFree(dout.pbData);
    return QString::fromLatin1(enc.toBase64());
#else
    return QString();
#endif
}

static QString unprotect(const QString& b64) {
    if (b64.isEmpty()) return QString();
#ifdef Q_OS_WIN
    QByteArray enc = QByteArray::fromBase64(b64.toLatin1());
    DATA_BLOB din;
    din.pbData = reinterpret_cast<BYTE*>(enc.data());
    din.cbData = static_cast<DWORD>(enc.size());
    DATA_BLOB dout{};
    if (!CryptUnprotectData(&din, nullptr, nullptr, nullptr, nullptr, 0, &dout))
        return QString();   // arquivo copiado de outra maquina/usuario -> nao decifra
    QByteArray dec(reinterpret_cast<const char*>(dout.pbData), int(dout.cbData));
    LocalFree(dout.pbData);
    return QString::fromUtf8(dec);
#else
    return QString();
#endif
}

// ---- Load / Save -----------------------------------------------------------

SipConfig SipConfig::load() {
    SipConfig c;
    QFile f(paths::configFile());
    if (!f.open(QIODevice::ReadOnly)) return c;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return c;

    const QJsonObject o = doc.object();
    c.server           = o.value("Server").toString();
    c.port             = o.value("Port").toInt(5060);
    c.username         = o.value("Username").toString();
    c.displayName      = o.value("DisplayName").toString();
    c.expirySeconds    = o.value("ExpirySeconds").toInt(120);
    c.keepAliveSeconds = o.value("KeepAliveSeconds").toInt(15);
    c.darkTheme        = o.value("DarkTheme").toBool(true);
    c.captureDevice    = o.value("CaptureDevice").toString();
    c.playbackDevice   = o.value("PlaybackDevice").toString();
    c.autoAnswer       = o.value("AutoAnswer").toBool(false);

    const QString prot = o.value("PasswordProtected").toString();
    if (!prot.isEmpty()) c.password = unprotect(prot);
    else                 c.password = o.value("Password").toString();   // fallback legado
    return c;
}

void SipConfig::save() const {
    QJsonObject o;
    o["Server"]            = server;
    o["Port"]              = port;
    o["Username"]          = username;
    o["PasswordProtected"] = protect(password);   // nunca em texto puro
    o["DisplayName"]       = displayName;
    o["ExpirySeconds"]     = expirySeconds;
    o["KeepAliveSeconds"]  = keepAliveSeconds;
    o["DarkTheme"]         = darkTheme;
    o["CaptureDevice"]     = captureDevice;
    o["PlaybackDevice"]    = playbackDevice;
    o["AutoAnswer"]        = autoAnswer;

    QFile f(paths::configFile());   // paths::dataDir() ja garante a pasta
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

}  // namespace sphone
