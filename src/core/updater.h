#pragma once
//
// updater.h — Auto-update via GitHub Releases (instalador silencioso).
// Checa o sphone-version.json do ultimo release; havendo versao maior, baixa o
// SPHONE-Setup, confere o SHA256 e roda /VERYSILENT (que fecha o app, troca os
// arquivos e reabre). Defesas: HTTPS + host do GitHub + hash obrigatorio.
//
#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace sphone {

struct UpdateInfo {
    QString version;
    QString url;      // URL do SPHONE-Setup-x.y.z.exe
    QString sha256;   // hash esperado (hex)
    QString notes;
};

class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QObject* parent = nullptr);

    static QString currentVersion();
    void checkForUpdate();                       // -> updateAvailable / upToDate / checkFailed
    void downloadAndRun(const UpdateInfo& info); // baixa, verifica, roda /VERYSILENT, encerra o app

signals:
    void updateAvailable(const sphone::UpdateInfo& info);
    void upToDate();
    void checkFailed(const QString& msg);
    void downloadProgress(int pct);
    void downloadFailed(const QString& msg);

private:
    static bool isTrustedUrl(const QString& url);
    static int  compareVersions(const QString& a, const QString& b);   // a>b: >0
    void launchInstallerAndQuit(const QString& installerPath);         // libera protecao, roda /VERYSILENT e encerra
    QNetworkAccessManager* m_net = nullptr;
};

}  // namespace sphone

Q_DECLARE_METATYPE(sphone::UpdateInfo)
