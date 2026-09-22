#!/usr/bin/env node
// Contrato estático do app diagnóstico. Não substitui a captura no emulador;
// evita empacotar acidentalmente o package id produtivo ou um fake de AVPlay.
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const read = (p) => fs.readFileSync(path.join(root, p), 'utf8');
const ok = (condition, message) => {
  if (!condition) throw new Error(message);
  console.log(`PASS ${message}`);
};

const config = read('tools/tizen-ass-config.xml');
const shell = read('tools/tizen-ass-shell.html');
const source = read('tools/teste-tizen-ass.c');
const build = read('tools/teste-tizen-ass.sh');
const server = read('tools/tizen-ass-server.py');

ok(config.includes('package="NuvioASS01"'), 'package diagnóstico de 10 caracteres');
ok(!config.includes('NuvioTV002'), 'config diagnóstico não reutiliza package produtivo');
ok(shell.includes('application/avplayer'), 'shell usa objeto AVPlay real');
ok(shell.includes('@ASS_VIDEO_URL@') && shell.includes('@ASS_TEXT_URL@'), 'shell tem URLs somente como placeholders');
ok(source.includes('video_iniciar') && source.includes('video_bombear'), 'harness chama ciclo AVPlay real');
ok(source.includes('nv_tizen_ass_prepared') && shell.includes('nv_tizen_ass_prepared'), 'timers aguardam prepare real do AVPlay');
ok(source.includes('legenda_carregar') && source.includes('legenda_desligar'), 'harness exercita legenda externa e embedded separadamente');
ok(source.includes('nv_tizen_ass_hold_external') && shell.includes('nv_tizen_ass_hold_external'), 'janela externa pausada para captura');
ok(shell.includes('assHoldExternal') && shell.includes('20000'), 'captura aguarda parser ASS antes de pausar');
ok(shell.includes('emulator.klog') && shell.includes('ASS_CONTROL_URL') && build.includes('/controle'), 'log persistente e controle explícito da segunda abertura');
ok(source.includes('mkv_faixas') && source.includes('S_TEXT/ASS'), 'harness identifica codec ASS no cabeçalho Matroska');
ok(source.includes('video_escolher_legenda(0)'), 'harness seleciona faixa TEXT sem UI manual');
ok(source.includes('player_atualizar'), 'harness mantém overlay de produção ativo');
ok(build.includes('NUVIO_TIZEN_EXTRA_SOURCES') && build.includes('NUVIO_TIZEN_CONFIG'), 'build injeta apenas fonte e configuração diagnósticas');
ok(server.includes('Access-Control-Allow-Origin') && server.includes('Range'), 'servidor fixture fornece CORS e Range');
ok(!source.includes('fake') && !shell.includes('fake-avplay'), 'harness não usa AVPlay fake');
console.log('tizen-ass-harness: PASS');
