#include "core/diag.h"
#include "core/paths.h"

#include <QFile>
#include <QMutex>
#include <QDateTime>
#include <QTextStream>

namespace sphone::diag {

void log(const QString& msg) {
    static QMutex gate;
    QMutexLocker lock(&gate);
    QFile f(paths::diagFile());
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;   // diagnostico nunca quebra o app
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        << "  " << msg << '\n';
}

}  // namespace sphone::diag
