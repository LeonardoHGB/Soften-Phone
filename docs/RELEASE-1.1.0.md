# SPHONE 1.1.0 — Auditoria de grupo de toque

Esta versão melhora a auditoria de chamadas no Discord para o cenário de **grupo de
toque** (a chamada toca em vários ramais e apenas um atende) e prepara o terreno
para um bot consolidar e analisar as chamadas.

## O que mudou

### Identificação de quem atendeu
- A perna que **atende** passa a exibir o campo **"Atendido por: ramal X"** na embed.
  Como só o ramal que atende sabe que foi ele (o PABX não informa isso às outras
  pernas), é ele quem se autoidentifica na mensagem.

### Sem mensagens duplicadas no grupo de toque
- As pernas que recebem **"atendida em outro ramal"** agora **apagam a própria
  mensagem de toque** em vez de deixá-la como "Atendida em outro ramal". Das N
  mensagens de "tocando", sobra apenas a do ramal que atendeu.
- Trata também a corrida em que o "atendida em outro ramal" chega antes de o POST
  inicial retornar: a mensagem fica pendente e é apagada assim que o id chega.

### Dados estruturados para análise (bot futuro)
- Toda embed leva um rodapé JSON (`schema: sphone.call/1`) parseável por um bot,
  com **Call-ID SIP** como chave de correlação das pernas da mesma chamada,
  além de direção, ramal, número, início (ISO-8601), duração e resultado.
- O **resultado** vai em código estável (`ANSWERED`, `MISSED`, `REJECTED`,
  `CANCELLED`, `TRANSFERRED`, `ANSWERED_ELSEWHERE`, `RINGING`, `FAILED`),
  independente do texto em português exibido na embed.

### Motor SIP
- O Call-ID SIP da chamada passou a ser exposto pelo engine (capturado no INVITE de
  entrada e, nas chamadas de saída, logo após originar) e gravado em cada registro
  de auditoria e no histórico local.

## Compatibilidade
- **Sem migração de dados.** O histórico local (`callhistory.json`) ganhou a chave
  `SipCallId`, serializada por nome: arquivos antigos abrem na versão nova (chave
  ausente → vazia) e o histórico novo abre em versões antigas (a chave desconhecida
  é ignorada). Atualização e rollback são seguros.
- **Mecanismo de auto-update inalterado.** A entrega segue o fluxo de sempre
  (manifesto no GitHub → comparação de versão → download → verificação SHA256 →
  instalação silenciosa).

## Observação conhecida
- Uma chamada que **ninguém** atende ainda gera uma mensagem "Perdida" por ramal
  (uma cópia em cada cliente). Desduplicar isso exige a visão global e é o papel do
  bot futuro, usando o Call-ID SIP que agora acompanha cada mensagem.
