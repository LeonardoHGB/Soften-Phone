# SPHONE 1.2.1 — Correção: reabrir após o auto-update

## O que mudou
- **Correção:** após uma atualização automática, o app **não reabria** sozinho —
  era preciso abrir manualmente. A causa: a única entrada `[Run]` do instalador
  (que reabre o app) tinha a flag `skipifsilent`, e o auto-update roda o instalador
  em modo silencioso (`/VERYSILENT`), então o relançamento era pulado.
- **Correção aplicada:** o `setup.iss` ganhou uma entrada `[Run]` que relança o app
  **apenas quando a instalação é silenciosa** (`Check: WizardSilent`), sem alterar o
  comportamento do instalador interativo (o checkbox "Abrir agora" na tela final).

## Observação
- O relançamento é executado pelo instalador **da versão de destino**. Portanto, o
  update **1.2.0 → 1.2.1** já reabre sozinho (instalador 1.2.1 corrigido), e todas as
  atualizações seguintes também. A falta de reabertura ocorria ao instalar o 1.2.0.

## Compatibilidade
- Sem mudanças de código do app; apenas empacotamento (`setup.iss`) e versão.
- Canal de auto-update inalterado (`LeonardoHGB/Soften-Phone`).
