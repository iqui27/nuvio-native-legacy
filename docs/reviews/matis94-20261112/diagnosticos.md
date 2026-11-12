# Paridade Matis94 — diagnósticos, buffer e ajustes

Data: 12/11/2026. Nosso snapshot: `8221bb897110271313db2864d2f0d15d1857b61b`, em `/Volumes/ExternalSSD/nv-21-int`. Fonte: 11 releases em [releases.jsonl](releases.jsonl). Versão pública consultada: `v2.0.8`; presença na integração não significa publicação.

Análise estática de implementação e testes. Nenhum teste, build, instalação, upload ou acesso a conta executado nesta auditoria. Referências abaixo são caminhos e linhas **do snapshot da integração**, não necessariamente do checkout em que este relatório está salvo.

**Presente:** implementação correspondente encontrada. **Parcial:** existe solução relacionada, com diferença material. **Ausente:** caminho equivalente não encontrado. **Não comprovado:** inspeção não demonstra resultado de pacote, performance ou aparelho. Testes citados foram inspecionados, não executados.

| Feature anunciada | Origem no fork | Situação nossa | Evidência / diferença |
|---|---|---|---|
| Página local estável e download de log redigido | 2.0.6.2, 2.0.8, 2.0.8.7 | Presente | `src/logrede.c:381,395`; endereço em `src/ajustes.c:8709`. Porta preferida 8099, com fallback. Nosso download lê log no pedido; não equivale a snapshot congelado do exportador. |
| QR local RED → BLUE com snapshot enquanto tela fica aberta | 2.0.6.2, 2.0.8 | Ausente | Servidor local existe, mas tela QR/lifecycle equivalente não foi localizada. `src/registro.c:543,616` segue outro fluxo. |
| Explicar por que página local não abriu | 2.0.8.7.test2 | Ausente | `src/logrede.c:460` produz placeholder de IP; falhas de arranque em `:510` não têm fluxo equivalente de motivo mostrado à pessoa. |
| Rede 100.x | 2.0.8.7.test2 | Presente, com escopo | `src/logrede.c:188,207`: aceita CGNAT `100.64.0.0/10`, não todo `100.0.0.0/8`. Conectividade real não testada. |
| Link público/QR de log na nuvem válido por 72h; OK/RIGHT como fallback | 2.0.8.7.test2 | Ausente | Nosso envio em `src/registro_envio.inc:49` gera recibo de seis caracteres (`src/avisos.c:590`). Retenção de quatro dias em `servidor/recomendacoes/src/index.js:813` não é link de download de 72h. |
| Chave Diagnostics QR OFF e liberação de recursos ao fechar | 2.0.8.5 | Parcial | Logging normal ativo (`src/main.c:1060`), chave de **página local** OFF (`src/ajustes_ux_padrao.inc:347`), desligamento do servidor em `src/logrede.c:499`. Não há chave/tela QR equivalente; fechar painel não significa desligar servidor. |
| Overlay RAM e GREEN, depois três chaves independentes de RAM/dados/atalho | 2.0.5–2.0.8.1 | Parcial | RAM no medidor de desempenho (`src/desempenho.c:27,69`), OFF por padrão (`src/ajustes_ux_padrao.inc:198`). Não localizado mesmo conjunto de chaves nem GREEN dedicado à RAM. |
| Dados do buffer: MiB pronto/capacidade, tempo estimado, modo sistema e limite HTTP da sessão | 2.0.5–2.0.8.5 | Parcial | Telemetria em `src/dts/dts_engine.c:805`, estimativa em `src/video.c:3706`. Caminho nativo em `:3720` explicitamente evita contadores/overlay novos. Tela dedicada e linha de conexões não encontradas. |
| Diagnóstico de rede redigido: redirects, timings, rate limit e reuso | 2.0.8.5 | Parcial | `src/rede.c:1692` registra DNS/TCP/TLS/total, status e conexões, limitado a primeiras requisições/falhas. Redirect em `:3551`. Não demonstra toda instrumentação detalhada do fork. |
| Cooldown 429 compartilhado com Retry-After | 2.0.8.5 | Presente, com escopo | Parsing segundos/data em `src/rede.c:1449,3385`; prazo compartilhado **por fonte em conversão**, teto 30s, `src/dts/dts_engine.c:228,349`; habilitação ARM `tools/arm.sh:151`. Não é cooldown global de todos pedidos do app. |
| Traduções 29 idiomas: mapping P7, DV MP4, saída DV e tela Buffer | 2.0.8.7 | Parcial | Infraestrutura e 29 tabelas existentes (`src/idioma.c:16`); chaves existentes de P7→P8.1/tamanho de buffer traduzidas. Isso não cobre controles do fork ausentes, nem prova qualidade linguística. |
| Opacidade de legenda em passos de 5%, com migração | 2.0.8.5 | Ausente | `src/ajustes.c:1096`: 100/75/50/25%; secundária permite seguir principal. `src/player.c:6037` mapeia quatro índices. Exige representação e migração compatíveis. |
| ARM completo, ABI, IPK com P2P e limpeza de atalhos de build | Várias | Não comprovado como resultado | Scripts existem (`tools/arm.sh:193,347,438`, `tools/testa-tudo.sh:29`); inclusão P2P é condicional. Nenhum pacote foi gerado/inspecionado aqui. Alegações históricas do fork não foram reproduzidas. |

## Testes encontrados, sem execução nesta rodada

- `tests/logrede.c:16`: redação de URLs/userinfo/magnet/token; não é teste end-to-end do download LAN/QR.
- `tests/regcodigo.c:8`: recibo; não link público nem expiração.
- `tests/buffer_overlay.c:22`: medidor RSS/memória e ausência de leituras repetidas no desenho.
- `tests/buffer_tamanho.c:6`: budgets e piso de memória; não tela de dados.
- `tests/rede_reuso.c:37`, `tests/rede_userinfo_log.c:57`: reuso e redação nos diagnósticos.
- `tests/dts_shared_retry.sh:15,28`: espera compartilhada, cancelamento, isolamento, seek e comportamento legado.
- `tests/rede_retry_wgt.cjs:33`: parsing dos cabeçalhos.
- `src/idioma.c:118`, `tests/idioma.c:74`: tamanho de tabelas/placeholders, não qualidade das traduções.

## Defaults: diferenças que não devem ser copiadas automaticamente

- Nosso buffer inicial é **Automático** (`src/ajustes_ux_padrao.inc:269`); fork começou em 64 MiB.
- Nossa conversão P7 começa **OFF** (`:345`); primeiras builds de teste do fork anunciavam ON.
- Página local e medidor de desempenho começam OFF; isso não significa existir Diagnostics QR.
- Envio automático de logs inicia **ON** (`:124`), com migração em `src/ajustes.c:5418`; comentários antigos adjacentes dizendo OFF não descrevem inicialização efetiva.

Relatos de testes LG C3/C5 e sucesso de pacotes permanecem alegações atribuídas às releases do fork. Não são validação nossa. Não implementar upload público/retention, mudar defaults ou migrar índices persistidos somente por este inventário: este pedido autoriza comparação e mapeamento.
