# SPHONE 1.2.0 — Auditoria de encerramento e proteção do processo

Esta versão adiciona auditoria de encerramento do app, dificulta o encerramento
pelo usuário e deixa o canal de auditoria de chamadas mais limpo.

## O que mudou

### Auditoria de encerramento (webhook dedicado)
- Quando alguém **tenta encerrar** o Soften Phone com senha de supervisor errada,
  ou **encerra** com a senha correta, o app posta um aviso num webhook do Discord
  **separado** do das chamadas, com Ramal, Máquina, Usuário do Windows e Horário.
- O envio é bloqueante com timeout curto, para garantir que a mensagem saia antes
  de o processo encerrar.

### Proteção contra encerramento (anti-kill)
- No boot (apenas em build de release), o app aplica uma DACL no próprio processo
  que **nega `PROCESS_TERMINATE` ao usuário atual**: o "Finalizar tarefa" do
  Gerenciador de Tarefas passa a falhar (acesso negado) para usuários **sem
  privilégio de administrador**.
- O auto-encerramento legítimo (Sair pelo supervisor) não é afetado.
- **Limites:** um administrador ainda consegue encerrar (não existe processo 100%
  imatável). Como o binário não é assinado, o comportamento pode aumentar
  falso-positivo de antivírus.

### Canal de auditoria de chamadas mais limpo
- O JSON de máquina (schema `sphone.call/1`) saiu do rodapé visível do embed e foi
  para o `content` da mensagem dentro de um **spoiler** (`||…||`): fica recolhido
  para humanos e continua legível (cru) para um bot que leia o `content`.

## Compatibilidade
- Mecanismo de auto-update inalterado; canal de update segue em
  `LeonardoHGB/Soften-Phone`.
- Sem migração de dados.
