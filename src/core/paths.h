#pragma once
//
// paths.h — Pastas e arquivos de dados do SPHONE.
//
// Decisao do projeto: comecar limpo em %LOCALAPPDATA%\SPHONE (NAO reusa a pasta
// do SoftenPhone; os dois podem coexistir). Ver project memory.
//
#include <QDir>
#include <QString>
#include <QStandardPaths>

namespace sphone::paths {

// %LOCALAPPDATA%\SPHONE — criada sob demanda.
inline QString dataDir() {
    QString local = qEnvironmentVariable("LOCALAPPDATA");
    if (local.isEmpty())
        local = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = QDir(local).filePath("SPHONE");
    QDir().mkpath(dir);
    return dir;
}

inline QString configFile()  { return QDir(dataDir()).filePath("sphone.json"); }
inline QString historyFile() { return QDir(dataDir()).filePath("history.json"); }
inline QString diagFile()    { return QDir(dataDir()).filePath("diag.txt"); }
inline QString sipLogFile()  { return QDir(dataDir()).filePath("pjsip.log"); }
// Presenca deste arquivo liga o log detalhado do PJSIP (paridade com 'sipdebug').
inline QString sipDebugFlag(){ return QDir(dataDir()).filePath("sipdebug"); }

}  // namespace sphone::paths
