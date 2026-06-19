#pragma once
//
// diag.h — Log de diagnostico simples (porte de Diag.cs).
// Grava em %LOCALAPPDATA%\SPHONE\diag.txt, formato "yyyy-MM-dd HH:mm:ss" + 2
// espacos + msg + \n. Sem segredos. Nunca lanca (best-effort).
//
#include <QString>

namespace sphone::diag {
void log(const QString& msg);
}
