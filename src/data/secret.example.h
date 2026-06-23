#pragma once
//
// secret.example.h — MODELO de segredos do SPHONE.
//
// Copie este arquivo para "secret.h" (na mesma pasta) e preencha com os valores
// reais. O secret.h NAO e versionado (esta no .gitignore); quando presente, ele
// tem prioridade no build (via __has_include). Sem secret.h, estes placeholders
// sao usados: a auditoria do Discord nao posta e a senha de supervisor nao confere.
//
#define SPHONE_DISCORD_WEBHOOK ""        // webhook de auditoria das chamadas ("" desliga o POST)
#define SPHONE_DISCORD_EXIT_WEBHOOK ""   // webhook de auditoria de encerramento ("" desliga o POST)
#define SPHONE_SUPERVISOR_HASH "0000000000000000000000000000000000000000000000000000000000000000"  // SHA256 da senha de supervisor
