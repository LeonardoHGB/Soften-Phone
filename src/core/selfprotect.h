#pragma once
//
// selfprotect.h — Endurecimento do processo no Windows.
//
namespace sphone {

// Aplica no proprio processo uma DACL que NEGA PROCESS_TERMINATE ao usuario atual,
// fazendo o "Finalizar tarefa" do Gerenciador de Tarefas falhar (acesso negado)
// para usuarios SEM privilegio de administrador. Um admin (com SeDebugPrivilege)
// ainda contorna — nao existe processo 100% imatavel. O auto-encerramento do
// proprio app (Sair pelo supervisor / qApp->quit) NAO e afetado, pois nao passa
// por OpenProcess+TerminateProcess. Best-effort: qualquer falha e silenciosa.
// No-op fora do Windows.
void hardenProcessAgainstTermination();

}  // namespace sphone
