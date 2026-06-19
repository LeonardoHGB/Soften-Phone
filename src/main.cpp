//
// main.cpp — Ponto de entrada do SPHONE.
//
// Ordem de boot (porte de Program.Main): instancia unica (compartilhada +
// servidor local para o gesto "mostrar"), tema, janela. Auto-instalacao e
// auto-update ficam a cargo do instalador Inno (decisao do projeto), ao
// contrario do app .NET que se auto-instalava.
//
#include "core/brand.h"
#include "core/diag.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QSharedMemory>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr auto kInstanceKey = "SPHONE.SingleInstance.v1";
constexpr auto kShowServer  = "SPHONE.Show.v1";
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("SPHONE");
    app.setOrganizationName("Soften Sistemas");
    app.setQuitOnLastWindowClosed(false);   // fechar a janela vai para a bandeja

    // ---- Instancia unica -------------------------------------------------
    // Evita registro SIP duplicado do mesmo ramal. Se ja existe, sinaliza a
    // instancia viva para reerguer a janela e sai.
    QSharedMemory guard(kInstanceKey);
    if (!guard.create(1)) {
        QLocalSocket sock;
        sock.connectToServer(kShowServer);
        sock.waitForConnected(300);
        sphone::diag::log("Segunda instancia: sinalizou 'mostrar' e encerrou.");
        return 0;
    }

    sphone::diag::log("=== SPHONE iniciado ===");
    brand::applyTheme(false);   // tema claro (config virá na fase de persistencia)

    sphone::MainWindow w;

    // Servidor local que recebe o sinal de outras instancias para reerguer.
    QLocalServer::removeServer(kShowServer);
    QLocalServer server;
    server.listen(kShowServer);
    QObject::connect(&server, &QLocalServer::newConnection, &w, [&server, &w] {
        if (QLocalSocket* c = server.nextPendingConnection()) c->deleteLater();
        w.restoreFromTray();
    });

    w.show();
    return app.exec();
}
