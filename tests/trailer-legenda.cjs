// Regra de legenda do trailer do YouTube: executa o corpo EM_JS real.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const src = fs.readFileSync('src/trailer.c', 'utf8');
const a = src.indexOf('EM_JS(void, trailer_js_legenda');
assert.ok(a >= 0, 'EM_JS trailer_js_legenda ausente');
const body = src.slice(src.indexOf('{', src.indexOf('), {', a)) + 1, src.indexOf('\n});', a));
const Module = {};
new Function('Module', 'UTF8ToString', 'sub', 'audio', body)(Module, (x) => x, 'pt', 'en');
const pick = Module.nvLegendaEscolher;
const tr = [{ languageCode: 'en' }, { languageCode: 'pt-BR' }];
const t = (n, got, want) => { assert.equal(got, want, n); console.log('ok ' + n); };
t('idioma casa (pt -> pt-BR)', pick(tr, 'pt', 'en'), 'pt-BR');
t('audio dublado no idioma -> nenhuma', pick(tr, 'pt', 'pt'), '');
t('sem faixa no idioma -> nenhuma', pick(tr, 'fr', 'en'), '');
t('sem preferencia -> nenhuma', pick(tr, '', 'en'), '');
t('none -> nenhuma', pick(tr, 'none', 'en'), '');
t('sem lista -> nenhuma', pick(null, 'pt', 'en'), '');
