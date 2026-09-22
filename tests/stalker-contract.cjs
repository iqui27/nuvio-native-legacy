// Contrato estático dos jobs de fonte Stalker. Os detalhes de rede continuam
// fora do teste; aqui evitamos regressões nos ciclos que perderam o handle ou
// aplicaram uma resposta velha durante a integração.
const assert = require('node:assert/strict');
const fs = require('node:fs');

const app = fs.readFileSync('src/app.c', 'utf8');
const stalker = fs.readFileSync('src/stalker.c', 'utf8');
const updateStart = app.indexOf('void app_atualizar(float dt, Uint32 agora)');
const updateEnd = app.indexOf('void app_desenhar(Uint32 agora)', updateStart);
const update = app.slice(updateStart, updateEnd);
const touchStart = app.indexOf('static void tocarCanal');
const touchEnd = app.indexOf('// A home carregou?', touchStart);
const touch = app.slice(touchStart, touchEnd);
assert.ok(updateStart >= 0 && updateEnd > updateStart, 'app_atualizar range');
assert.ok(touchStart >= 0 && touchEnd > touchStart, 'tocarCanal range');

function check(name, condition) {
  assert.ok(condition, name);
  console.log(`ok ${name}`);
}

check('job tem estado single-flight atomico', /FJOB_IDLE, FJOB_RUNNING, FJOB_DONE/.test(app));
check('troca de canal invalida geracao e pendencia',
  /novaGeracaoFonte\(\);[\s\S]*limparFontePendente\(\);/.test(touch));
check('app usa pedirFonteJob para addon e canal',
  /pedirFonteJob\(FJOB_CANAL/.test(update) && /pedirFonteJob\(FJOB_ADDON/.test(update));
check('nao cria worker direto fora do scheduler',
  (app.match(/pthread_create\(&fioFonte/g) || []).length === 1);
check('join do worker no update ocorre somente apos DONE',
  /atomic_load_explicit\(&job->estado, memory_order_acquire\) != FJOB_DONE\)[\s\S]*?return;[\s\S]*?pthread_join\(fioFonte/.test(app));
check('update nao faz join bloqueante', !/pthread_join\(fioFonte/.test(update));
check('resposta Stalker valida geracao, id e sessao',
  /geracao == fontePedidoGeracao/.test(app) && /mesmoId/.test(app) && /!player_quer_sair\(\)/.test(app));
check('portal e MAC nao usam copias globais',
  !/portalCopia|macCopia/.test(stalker) && /char portalLocal\[256\], macLocal\[32\]/.test(stalker));
check('pedido HTTP recebe contexto local',
  /static char \*pedir\(const char \*portalLocal, const char \*macLocal/.test(stalker) &&
  /pedir\(portalLocal, macLocal, rota, tok, consulta\)/.test(stalker));

console.log('stalker-contract: tudo ok');
