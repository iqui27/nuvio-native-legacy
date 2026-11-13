# Lote G integrado — diagnóstico e exportação de log

Pedido do dono: importar o que falta do fork Matis94 para a integração de TV. Merge `6aa346ec3c544184e7386069b0f2bf04c2076fef`, candidato `e7daae8ad2919b4997f9d5134591d429350e96eb`, base `4d8a7e3823dbbb86e4a95f535c7998138ef65435`. Código de D02, D03, D05 e D06 incorporado, com limitações P2 abaixo; não significa aceite físico ou disponibilidade do serviço. Sem push, release, instalação, escrita em conta ou upload real. Serviço de nuvem não provisionado nem publicado. Hook documental existente atualiza painel e roadmap público automaticamente.

## Comportamento incorporado

- Cartão de diagnóstico reutiliza o QR existente para o endereço local, mantém endereço legível e permite atualizar o snapshot. Motivos de indisponibilidade orientam o fallback explícito para nuvem. Chave local de QR desligada por padrão.
- Snapshot redigido antes da compactação e de qualquer upload. Arquivo privado criado com `mkstemp`, usando TMPDIR ou diretório de dados no Android, com cabeçalho neutro à plataforma. Fechamento normal remove snapshot; encerramento do processo tem a limitação F9 abaixo.
- Link público de log usa identificador aleatório de 128 bits e validade de 72 horas. Código do serviço limita corpo a 2 MiB e quotas atômicas de seis envios por IP/hora e cem por dia.
- Credencial embarcada somente para upload separada da credencial de diagnóstico amplo. Configuração com ambas iguais falha fechada; rotas de conta recusam esse modo. Sem rotas públicas de listagem ou administração de logs.
- Cache e resposta de link respeitam a capacidade do encoder QR. Fixture com link sintético de 134 bytes verificou os módulos do QR e sessenta quadros estáveis sem uploads de textura. HTTP de nuvem foi substituído somente na fronteira do teste; nenhum envio real.
- Tabelas de Ajustes preservam os IDs integrados do lote F e acrescentam a opção de diagnóstico ao final. Ícone reutiliza `aj_stethoscope`.

## Revisão, verificação e gate

Revisor independente `j-rev-g2-zima`; verificador distinto `j-verify-g3-zima`, cobertura de fonte concluída. Achado histórico F0 P1 de privilégio amplo retirado após inspeção de credenciais e rotas. Nenhum P0/P1 atual confirmado. Revisores não executaram os testes host e atribuíram os resultados anteriores à Central.

SHA-256 do diff revisado: `6d051715539310741c88557e73f4b57c9910efbcc3ee4cb133d43ef1aa5e57b9`. Gate executou oito comandos no candidato exato: skip_button_settings, celbotao_log_shot, diagnostics_port, logexport, logcloud, i18n, build-local e ajustes_ux_interacao. Oito passaram, nenhum ignorado, timeout ou divergência; agreement verdadeiro. Digest do gate: `287c94a7b6cb85a9db56dcdd6a29ff2af8370085c626399e15e4db6d8de1f48f`. Varredura i18n automática do integrador passou.

## Pós-merge

HEAD `6aa346ec3c544184e7386069b0f2bf04c2076fef` idêntico antes/depois. TMPDIR no SSD e `NUVIO_PROPERTIES=/dev/null`.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/diagnostics_port.sh` | 0 | `5959eb1bb3d0696b92b3740866b55a94102d8dec92ef78a26cf6ef141df2ff9d` |
| `bash tests/logexport.sh` | 0 | `6e329674ebdb1eff5771c3da51d96497faf9d04792e6e1bb0bb8fa24a8648b55` |
| `bash tests/logcloud.sh` | 0 | `602d53287d2cf28665e0ffcdcb597413599cea81bfa3216d8ee576cc0bed2351` |

Evidência privada em `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/`: `g-final-gate.log`, `g-integration.log`, `g-postmerge-evidence.json`, `j-rev-g2-zima.log` e `j-verify-g3-zima.log`.

## Notas P2 e limites

- F2: expiração/bloqueio local pode mostrar motivo `?`; texto informa explicitamente que OK envia à nuvem.
- F3: cliente pode tentar enviar gzip acima de 2 MiB e receber 413.
- F6: caminho anterior de relatório automático/manual usa log bruto, podendo incluir endereços de interfaces. Novo snapshot redige essa linha; não houve upload real.
- F7: redação é heurística; nomes de hosts ou credenciais curtas sem identificação podem permanecer.
- F8: página local tem erro de escape JavaScript que impede viewer, filtro, contagem e cópia. Download direto de texto continua funcionando. Confirmado por análise independente; teste HTTP não executa esse JavaScript.
- F9: sair ou matar o app com cartão aberto pode deixar snapshot redigido privado no diretório temporário ou persistente Android. Fechamento normal cobre remoção; não há limpeza no próximo início.
- Sem prova em TV, ABI, aprovação visual, decodificação externa por scanner QR ou serviço real. Capturas host verificam o componente existente com dados sintéticos. Código integrado não representa implantação do Worker nem fechamento de issues.
