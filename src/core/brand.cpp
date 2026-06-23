//
// brand.cpp — Resolucao de fontes por papel (Signal Architecture).
//
// pickFamily escolhe a 1a familia instalada/embarcada de uma lista de
// preferencia; se nenhuma existir, usa um fallback seguro. Assim, quando os
// .ttf OFL do design (DM Mono, Outfit, Work Sans, JetBrains Mono, Jura) forem
// embarcados no .qrc + QFontDatabase::addApplicationFont, os papeis passam a
// usar a fonte real sem tocar no codigo das telas. O resultado e cacheado.
//
#include "core/brand.h"

#include <QFontDatabase>
#include <QHash>

namespace brand {

QString pickFamily(std::initializer_list<const char*> prefs, const char* fallback) {
    static QHash<QString, QString> cache;   // chave = 1a preferencia
    if (prefs.size() == 0) return QString::fromLatin1(fallback);
    const QString key = QString::fromLatin1(*prefs.begin());
    if (auto it = cache.constFind(key); it != cache.constEnd()) return it.value();

    QString chosen = QString::fromLatin1(fallback);
    for (const char* fam : prefs) {
        const QString f = QString::fromLatin1(fam);
        if (QFontDatabase::hasFamily(f)) { chosen = f; break; }
    }
    cache.insert(key, chosen);
    return chosen;
}

QFont fontDisplay(qreal pt) {
    QFont f(pickFamily({"DM Mono", "JetBrains Mono", "Cascadia Mono", "Consolas"}, "Consolas"));
    f.setPointSizeF(pt);
    return f;
}

QFont fontTelemetry(qreal pt) {
    QFont f(pickFamily({"JetBrains Mono", "Cascadia Mono", "Consolas"}, "Consolas"));
    f.setPointSizeF(pt);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    return f;
}

QFont fontPanelTitle(qreal pt) {
    QFont f(pickFamily({"Outfit", "Manrope", "Inter Display", "Segoe UI Semibold"}, "Segoe UI Semibold"));
    f.setPointSizeF(pt);
    f.setWeight(QFont::Bold);
    return f;
}

QFont fontLabel(qreal pt) {
    QFont f(pickFamily({"Work Sans", "Inter", "Segoe UI"}, "Segoe UI"));
    f.setPointSizeF(pt);
    return f;
}

QFont fontBrandSub(qreal pt) {
    QFont f(pickFamily({"Jura", "Outfit", "Segoe UI"}, "Segoe UI"));
    f.setPointSizeF(pt);
    f.setWeight(QFont::Light);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
    return f;
}

}  // namespace brand
