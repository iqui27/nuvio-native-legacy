import * as M from './model.mjs';

const $ = (s, r = document) => r.querySelector(s);
const esc = v => String(v ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const dd = k => `data-d="${k}"`;
const SRC = { trakt: 'Trakt', simkl: 'Simkl', nuvio: 'Nuvio' };
// Same icon set as the rewatch mockup (24x24, stroke), plus the few this screen needs.
const IC = {
  ok: '<path d="m5 12.5 4.5 4.5L19 7"/>', play: '<path d="M7 4.5v15l12.5-7.5z" fill="currentColor"/>', x: '<path d="M6 6l12 12M18 6 6 18"/>',
  salvo: '<path d="M6 3h12v18l-6-4-6 4z"/>', info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>',
  redo: '<path d="M20 12a8 8 0 1 1-2.3-5.7L20 8.5M20 3.5v5h-5"/>', mais: '<path d="M12 5v14M5 12h14"/>',
  data: '<rect x="4" y="5.5" width="16" height="15" rx="2"/><path d="M4 10h16M8.5 3.5v4M15.5 3.5v4"/>',
  duvida: '<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.6 2.2c-.7.4-1.1.9-1.1 1.8M12 17v.2"/>',
  claquete: '<rect x="4" y="10.5" width="16" height="9" rx="1.5"/><path d="m4.2 10.2 14.6-3.9-.8-2.8L3.4 7.4z"/><path d="m8 6.2 2 3M12.5 5l2 3"/>',
};
const ic = n => `<span class="ic"><svg viewBox="0 0 24 24">${IC[n]}</svg></span>`;

const Q = new URLSearchParams(location.search);
// ?teclas=baixo,ok,... replays a key sequence on a clean state: that is how the PNGs are captured.
const REPLAY = Q.has('teclas');
const STORE = 'nuvio-filmografia-demo-v2';
let saved = {};
try { if (!REPLAY) saved = JSON.parse(localStorage.getItem(STORE)) || {}; } catch {}
const S = {
  pid: M.PERSONS[Q.get('p')] ? Q.get('p') : M.PERSONS[saved.pid] ? saved.pid : 'mara', tab: 'historia', focus: 'regua', year: {}, noise: false,
  filter: { ...M.DEFAULT_FILTER }, overlay: null, sources: false,
  marks: { ...M.emptyMarks(), ...saved.marks }, follow: saved.follow || {},
};
const persist = () => { try { if (!REPLAY) localStorage.setItem(STORE, JSON.stringify({ pid: S.pid, marks: S.marks, follow: S.follow })); } catch {} };

function data() {
  const p = M.PERSONS[S.pid], all = M.prepare(p), core = all.filter(e => !e.noise);
  const visible = S.noise ? all : core;
  const st = e => M.statusOf(e, p.seen, S.marks);
  const ms = M.milestones(visible), ach = M.achievements(p, core, st);
  return { p, all, core, visible, st, ms, ach, ch: M.chapters(visible, ms), years: M.timeline(visible), sum: M.summary(core, st), next: M.nextToWatch(core, st, p.dept, ach) };
}
function curYear(d) {
  const y = S.year[S.pid] ?? d.years[0].year;
  return d.years.reduce((a, b) => Math.abs(b.year - y) < Math.abs(a.year - y) ? b : a).year;
}

const plural = (n, s) => `${n} ${s}${n > 1 ? 's' : ''}`;
const hisEps = (n, p) => `${n === 1 ? 'o único episódio' : `os ${n} episódios`} em que ${p.short} aparece`;
const meta = (e, p) => e.kind !== 'tv' ? `${e.format} · ${e.runtime} min`
  : e.epsTotal > e.eps ? `Série · ${p.short} em ${e.eps} de ${e.epsTotal} episódios` : `Série · ${plural(e.eps, 'episódio')}`;
const part = e => M.roleLabel(e) + (e.roles.includes('Acting') && e.character ? ' · ' + M.charLabel(e) : '');
function badge(s) {
  if (s.state === 'seen') return `<span class="visto ${M.counts(s) ? '' : 'fraco'}">${ic('ok')}</span>`;
  if (s.state === 'progress') return `<span class="trilho"><i style="width:${s.pct}%"></i></span>`;
  if (s.state === 'upcoming') return '<span class="selo">Em breve</span>';
  return s.want ? '<span class="selo">Quero ver</span>' : '';
}
function statusText(e, s, p) {
  if (s.state === 'seen' && s.src === 'manual') return `Você marcou “Já vi” ${s.how === 'quiz' ? 'e passou nas perguntas' : 'sem responder às perguntas: não conta para conquistas'}. Está no Trakt e no Simkl com a data ${s.date === 'release' ? `de lançamento, ${M.fmtDate(s.watched)}, porque você não lembrava o dia` : `de ${M.fmtDate(s.watched)}`}.`;
  if (s.state === 'seen') return `${s.epsIn ? `Visto: você viu ${hisEps(s.epsIn, p)}. ` : 'Visto. '}Histórico do ${SRC[s.src]}, ${M.fmtDate(s.at)}.`;
  if (s.state === 'progress') return s.epsIn ? `Em andamento: ${s.epsSeen} de ${hisEps(s.epsIn, p).slice(3)}. Conta como vista quando você vir todos.` : `Em andamento no Nuvio: ${s.pct}%.`;
  if (s.state === 'upcoming') return 'Ainda não lançado.';
  return s.want ? 'Na sua lista Quero ver.' : 'Não aparece no Nuvio, no Trakt nem no Simkl.';
}
function card(e, d, k) {
  const s = d.st(e);
  return `<button class="pz" data-k="${k}" data-act="credit" data-id="${e.id}" aria-label="${esc(`${e.title}, ${e.year}, ${part(e)}. ${statusText(e, s, d.p)}`)}"><span class="cz pintura" style="--h:${e.hue}" ${dd('hoje')}>${badge(s)}<b>${esc(e.title)}</b></span><span class="t">${esc(e.title)}</span><span class="s">${e.year} · ${esc(M.roleLabel(e))}</span></button>`;
}

function header(d, hue) {
  const { p, sum, next, ach, core } = d, roles = M.roleLine(core), years = core.map(e => e.year);
  const best = ach.filter(a => a.kind === 'trail' && a.level).sort((a, b) => b.level - a.level)[0];
  const top = [...core].sort((a, b) => b.votes - a.votes).slice(0, 2).map(e => e.title);
  const span = Math.min(...years) === Math.max(...years) ? years[0] : `${Math.min(...years)}–${Math.max(...years)}`;
  const on = S.follow[p.id];
  return `<div class="arte" style="--h:${hue}"></div><div class="veu"></div>
  <div class="ilha relogio">20:47<span class="s"></span><span class="av">H</span></div>
  <div class="compacto"><span class="kick">Filmografia</span><b>${esc(p.name)}</b></div>
  <header class="esq">
    <p class="kick"><span ${dd('hoje')}>${roles.map(x => x.label).join(' · ')} · ${esc(p.place)} · ${plural(core.length, 'título')} · ${span}</span></p>
    <h1 class="h1">${esc(p.name)}</h1>
    <div class="meta"><span ${dd('hoje')}>${M.lifeText(p)}</span><span class="pt"></span><span ${dd('hoje')}>Mais votados: <b>${top.map(esc).join(', ')}</b></span>${best ? `<span class="pt"></span><span class="mk cq" ${dd('hoje')}><i></i>${M.LEVEL_NAMES[best.level]} · ${best.name.split(' · ')[1]}</span>` : ''}</div>
    <div class="acoes">
      ${next ? `<button class="btn" data-k="proximo" data-act="credit" data-id="${next.e.id}">${ic('play')}${esc(next.e.title)}<span class="v">${esc(next.reason)}</span></button>` : ''}
      <button class="btn" data-k="seguir" data-act="follow" aria-pressed="${!!on}" ${dd('nosso')}>${ic(on ? 'ok' : 'mais')}${on ? 'Seguindo' : 'Seguir'}</button>
      <div class="viu" ${dd('hoje')}><span class="trilho"><i style="width:${sum.total ? sum.seen / sum.total * 100 : 0}%"></i></span><span>Você já viu <b>${sum.seen}</b> de ${sum.total}</span>
        <span class="v">${roles.map(x => `${x.label} ${sum.roles[x.role]?.seen ?? 0}/${sum.roles[x.role]?.total ?? 0}`).join(' · ')}</span></div>
    </div>
  </header>`;
}

const TABS = [['historia', 'História'], ['filmografia', 'Filmografia'], ['sobre', 'Sobre'], ['conquistas', 'Conquistas']];
function tabs(d) {
  const count = { filmografia: d.visible.length, conquistas: `${d.ach.filter(a => a.level).length}/${d.ach.length}` };
  return `<nav class="seg" role="tablist">${TABS.map(([id, l]) => `<button role="tab" aria-selected="${S.tab === id}" class="sg ${S.tab === id ? 'sel' : ''}" data-k="tab-${id}" data-act="tab">${l}${count[id] != null ? `<span class="n">${count[id]}</span>` : ''}</button>`).join('')}</nav>`;
}

function historia(d) {
  const { years, ch, ms, p, st } = d;
  const y = curYear(d), t = years.find(x => x.year === y), ci = ch.findIndex(c => y >= c.from && y <= c.to), c = ch[ci];
  const min = years[0].year, max = years.at(-1).year, maxN = Math.max(...years.map(x => x.items.length));
  const at = k => max === min ? 50 : (k - min) / (max - min) * 100;
  const dots = years.map(x => `<i class="a ${x.items.some(e => st(e).state === 'seen') ? 'viu1' : ''} ${ms.some(m => m.year === x.year) ? 'marco' : ''}" style="left:${at(x.year)}%;--s:${Math.round(8 + 10 * Math.sqrt(x.items.length / maxN))}px"></i>`).join('');
  const shown = t.items.slice(0, 5), more = t.items.length - shown.length;
  const inCh = d.visible.filter(e => e.year >= c.from && e.year <= c.to), inChSeen = inCh.filter(e => st(e).state === 'seen').length;
  return `<section>
  <div class="caps">${ch.map((x, i) => `<button class="chip ${i === ci ? 'on' : ''}" data-k="cap-${i}" data-act="cap" data-i="${i}" aria-label="Capítulo ${i + 1}: ${esc(x.title)}, ${x.from} a ${x.to}">${i === ci ? `<span class="v">${x.from}${x.to !== x.from ? '–' + x.to : ''}</span>` : ''}${esc(x.title)}</button>`).join('')}
    ${d.all.length > d.core.length ? `<button class="chip ruido" data-k="ruido" data-act="noise" aria-pressed="${S.noise}"><span class="v">Participações e entrevistas</span>${S.noise ? 'Visíveis' : 'Ocultas'} · ${d.all.length - d.core.length}</button>` : ''}</div>
  <button class="regua" data-k="regua" role="slider" aria-label="Ano da carreira" aria-valuemin="${min}" aria-valuemax="${max}" aria-valuenow="${y}" aria-valuetext="${y}: ${plural(t.items.length, 'título')}">
    <span>${min}</span><span class="linha">${dots}<span class="agora" style="left:${at(y)}%">${y}<span class="v">${plural(t.items.length, 'título')}</span></span></span><span>${max}</span></button>
  <div class="palco">
    <div class="conto"><p class="kick">Capítulo ${ci + 1} de ${ch.length} · ${esc(c.title)}</p>
      <p class="ano">${y}</p>
      <p class="sub" aria-live="polite">${esc(M.yearStory(y, t.items, ms, p.awards))}</p>
      <div class="tags">${ms.filter(m => m.year === y).map(m => `<span class="tag">${M.MILESTONE[m.type]}</span>`).join('')}${p.awards.filter(a => a.year === y).map(a => `<span class="tag" ${dd('nosso')}>${a.won ? 'Prêmio' : 'Indicação'}</span>`).join('')}</div>
      <p class="mini">Neste capítulo você viu ${inChSeen} de ${inCh.length}.</p></div>
    <div class="trem">${shown.map((e, i) => card(e, d, `ano-${i}`)).join('')}${more ? `<button class="mais" data-k="ano-mais" data-act="yearGrid" data-year="${y}">+${more}<small>em ${y}</small></button>` : ''}</div>
  </div></section>`;
}

function filmografia(d) {
  const f = S.filter, list = M.filterGrid(d.visible, d.st, f);
  const chip = (name, label) => `<button class="chip" data-k="f-${name}" data-act="filter" data-f="${name}"><span class="v">${label}</span>${M.FILTERS[name].find(o => o[0] === f[name])[1]}</button>`;
  return `<section class="filmo"><div class="filtros">${chip('sort', 'Ordem')}${chip('type', 'Tipo')}${chip('role', 'Função')}${chip('status', 'Status')}
    ${f.year != null ? `<button class="chip on" data-k="f-year" data-act="clearYear"><span class="v">Ano</span>${f.year}${ic('x')}</button>` : ''}
    <button class="chip" data-k="f-ruido" data-act="noise" aria-pressed="${S.noise}"><span class="v">Participações</span>${S.noise ? 'Visíveis' : 'Ocultas'}</button>
    <span class="mini">${list.length} de ${plural(d.all.length, 'crédito')}</span></div>
  <div class="grade">${list.length ? list.map(e => card(e, d, `g-${e.id}`)).join('') : '<p class="mini">Nenhum título com esses filtros. Mude um filtro acima.</p>'}</div></section>`;
}

function sobre(d) {
  const { p, core } = d, bio = p.bio || p.bioEn, col = M.collaborators(core).slice(0, 4);
  const l = (k, v) => `<p class="l" ${dd('hoje')}><span>${k}</span><b>${v}</b></p>`;
  return `<section class="paineis">
  <div class="vid painel" style="width:660px"><p class="kick">Biografia · TMDB</p>
    ${bio ? `<p class="bio" ${dd('hoje')}>${esc(bio)}</p>${p.bio ? '' : '<p class="mini" style="margin-top:10px">Sem biografia em português no TMDB; esta é a versão em inglês.</p>'}<button class="chip" data-k="bio" data-act="bio">Ler biografia completa</button>` : '<p class="mini">O TMDB ainda não tem biografia.</p>'}
    ${l('Nascimento', `${M.fmtDate(p.born)} · ${esc(p.place)}`)}${p.died ? l('Morte', M.fmtDate(p.died)) : ''}
    ${p.aka.length ? l('Outros nomes', p.aka.map(esc).join(', ')) : ''}
    ${l('Fotos', p.images)}${l('Links', `IMDb ${p.ids.imdb}${p.ids.instagram ? ` · @${p.ids.instagram}` : ''}`)}</div>
  <div class="vid painel" style="width:500px"><p class="kick">Parcerias frequentes</p>
    ${col.length ? col.map((c, i) => `<button class="row" data-k="par-${i}" data-act="person" data-name="${esc(c.name)}" ${dd('hoje')}><b>${esc(c.name)}</b><small>${c.n} trabalhos juntos · ${esc(c.titles.slice(0, 3).join(', '))}</small></button>`).join('') : '<p class="mini">Nenhuma parceria repetida ainda.</p>'}
    <p class="kick mais-cima">Família · Wikidata</p>
    ${p.family.length ? p.family.map((f, i) => `<button class="row" data-k="fam-${i}" data-act="person" data-name="${esc(f.name)}" ${dd('nosso')}><b>${esc(f.name)}</b><small>${esc(f.rel)} · ${esc(f.note)}</small></button>`).join('') : '<p class="mini">Sem dados no Wikidata.</p>'}</div>
  <div class="vid painel" style="width:480px"><p class="kick">Prêmios · Wikidata</p>
    ${p.awards.length ? p.awards.map(a => `<p class="premio" ${dd('nosso')}><span class="n">${a.year}</span><b>${esc(a.name.replace(' (indicação)', ''))}</b><small>${a.won ? 'Venceu' : 'Indicação'} · ${esc(a.for)}</small></p>`).join('') : '<p class="mini">Nenhum prêmio no Wikidata.</p>'}
    <p class="pe" ${dd('bloq')}>Prêmios do IMDb e bilheteria ficam fora: só raspando.</p></div>
  </section>`;
}

function conquistas(d) {
  const need = a => [1, M.needFor(2, a.total), M.needFor(3, a.total), a.total];
  const trail = a => `<button class="vid painel trilha" data-k="ach-${a.key}" data-act="ach" data-key="${a.key}" ${dd('hoje')}>
    <span class="cab"><span class="cq-ic">${ic('claquete')}</span><span><b class="h2">${esc(a.name)}</b><small>${a.seen} de ${a.total} vistos${a.level ? ` · nível ${a.level} de 5` : ''}</small></span></span>
    <span class="trilho"><i style="width:${a.seen / a.total * 100}%"></i></span>
    ${[1, 2, 3, 4, 5].map(n => { const ok = a.level >= n, t = need(a)[n - 1]; return `<span class="nv ${ok ? 'ok' : ''}"><span class="bo">${ok ? ic('ok') : n}</span><span><b>${M.LEVEL_NAMES[n]}</b><small>${n === 5 ? 'Na ordem de lançamento, com datas reais' : plural(t, 'título')}</small></span><span class="st">${ok ? 'Ganho' : n === 5 ? 'Opcional' : `Faltam ${t - a.seen}`}</span></span>`; }).join('')}
    <span class="pe">Chave ${a.key}</span></button>`;
  const goal = a => `<button class="vid painel" data-k="ach-${a.key}" data-act="ach" data-key="${a.key}" ${dd('hoje')}><span class="mk ${a.level ? 'cq' : ''}" ${a.level ? '' : 'style="color:var(--mut2)"'}><i></i>${a.level ? 'Desbloqueada' : `${a.seen} de ${a.total}`}</span><b>${esc(a.name)}</b><small>${esc(a.desc)}</small></button>`;
  const goals = d.ach.filter(a => a.kind === 'goal');
  return `<section class="paineis">${d.ach.filter(a => a.kind === 'trail').map(trail).join('')}
  <div class="lado">${goals.length ? `<div class="metas">${goals.map(goal).join('')}</div>` : ''}
    <div class="vid painel"><p class="kick">Como conta</p><ul class="fatos">
      <li>${ic('ok')}Vale o histórico do Nuvio, do Trakt e do Simkl.</li>
      <li>${ic('ok')}“Já vi” manual só conta se você passou nas perguntas.</li>
      <li>${ic('ok')}Série conta quando você viu todos os episódios em que ${esc(d.p.short)} aparece.</li>
      <li class="fora">${ic('x')}Lançamentos futuros e participações ficam fora. “Em ordem” pede datas reais.</li></ul>
      <p class="pe">Aparece em: aviso no canto por 6 s, sem roubar o foco · selo na ficha · Perfil › Conquistas · perfil do amigo, se você deixou.</p></div></div>
  </section>`;
}

// ---- overlays: the same right-hand sheet as the rewatch mockup
const opc = (k, act, title, small, icon, extra = '') => `<button class="opc" data-k="${k}" data-act="${act}" ${extra}>${ic(icon)}<span><b>${title}</b>${small ? `<small>${small}</small>` : ''}</span></button>`;
const folha = (id, kick, body, pe) => `<div class="veu-folha"></div><section class="folha" role="dialog" aria-modal="true" aria-labelledby="${id}"><p class="kick">${kick}</p>${body}${pe ? `<p class="pe">${pe}</p>` : ''}</section>`;

function sheet(d) {
  const e = d.all.find(x => x.id === S.overlay.id), s = d.st(e), p = d.p, left = M.triesLeft(S.marks, e.id, Date.now());
  const open = s.state === 'none' || s.state === 'progress', o = [];
  o.push(s.state === 'progress' ? opc('sh-play', 'sh-play', 'Continuar', s.epsIn ? `${s.epsSeen} de ${s.epsIn} episódios com ${esc(p.short)}` : `${s.pct}% visto`, 'play') : opc('sh-open', 'sh-open', 'Abrir título', 'Página do título, com fontes e episódios.', 'info'));
  if (open) o.push(left ? opc('sh-quiz', 'sh-quiz', 'Já vi', 'Duas perguntas rápidas, sem spoiler. Vai para o Trakt e o Simkl.', 'ok') : opc('sh-locked', 'sh-locked', 'Já vi · amanhã', 'Tentativas de hoje usadas.', 'ok', 'aria-disabled="true"'));
  if (s.src === 'manual' && s.how !== 'quiz' && left) o.push(opc('sh-quiz', 'sh-quiz', 'Responder para contar nas conquistas', 'Duas perguntas rápidas. Passou, passa a contar.', 'claquete'));
  if (s.src === 'manual') o.push(opc('sh-unmark', 'sh-unmark', 'Desfazer “Já vi”', 'Também sai do histórico do Trakt e do Simkl.', 'redo'));
  if (s.state === 'none') o.push(opc('sh-want', 'sh-want', s.want ? 'Tirar de Quero ver' : 'Quero ver', '', 'salvo'));
  o.push(opc('sh-close', 'close', 'Fechar', '', 'x'));
  return folha('folha-t', `<span>${e.year} · ${esc(meta(e, p))}</span>`, `<h2 class="h2" id="folha-t">${esc(e.title)}</h2>
    <div class="prev"><span class="cz pintura" style="--h:${e.hue}" ${dd('hoje')}>${badge(s)}</span><div><p class="t">${esc(part(e))}</p><p class="s">${esc(e.genre)} · ${M.fmtK(e.votes)} votos no TMDB</p></div></div>
    <p class="lead" ${dd('hoje')}>${esc(statusText(e, s, p))}</p><div class="opcs">${o.join('')}</div>`,
  open ? `“Já vi” serve para o que não está no seu histórico. Passou nas perguntas, conta para as conquistas; sem responder, só aparece como visto.${left < M.QUIZ.tries ? ` ${left ? `Resta ${left} tentativa hoje.` : ''}` : ''}` : '');
}

function quiz(d) {
  const o = S.overlay, e = d.all.find(x => x.id === o.id), q = o.q, p = d.p, tv = e.kind === 'tv';
  const what = tv ? `${e.eps === 1 ? '1 episódio' : `${e.eps} episódios`} · show tmdb ${e.id.split('-')[1]}` : `movie tmdb ${e.id.split('-')[1]}`;
  if (o.how) return folha('qz-t', `<span>Já vi · ${o.how === 'quiz' ? 'confere' : 'sem perguntas'}</span>`, `<h2 class="h2" id="qz-t">Quando você viu ${esc(e.title)}?</h2>
    <p class="lead">Vai para o histórico do Nuvio, do Trakt e do Simkl com esta data${tv ? `: ${hisEps(e.eps, p)}` : ''}.</p>
    <div class="opcs">${opc('dt-today', 'qz-save', 'Hoje', `${M.fmtDate(M.TODAY)} · padrão`, 'data', 'data-date="today"')}
      ${opc('dt-release', 'qz-save', 'Não lembro', `Usa a data de lançamento, ${M.fmtDate(M.watchedDate(e, 'release'))}. A ficha passa a dizer que é a data de lançamento.`, 'duvida', 'data-date="release"')}
      ${opc('dt-no', 'close', 'Agora não', 'Nada é marcado nem enviado.', 'x')}</div>
    <ul class="fatos"><li class="${o.how === 'quiz' ? '' : 'fora'}">${ic(o.how === 'quiz' ? 'ok' : 'x')}${o.how === 'quiz' ? `Conta para as conquistas de ${esc(p.short)}: você passou nas perguntas.` : 'Não conta para conquistas: marcado sem as perguntas.'}</li>
      <li class="fora">${ic('info')}“Em ordem” continua pedindo a data real de quando você viu.</li></ul>
    <div class="saida"><div class="ev"><span class="svc trakt">Trakt</span><div><div class="call">POST /sync/history</div><div class="body">${what} · watched_at: hoje, ou "released" em Não lembro</div></div></div>
      <div class="ev"><span class="svc simkl">Simkl</span><div><div class="call">POST /sync/history</div><div class="body">${what} · watched_at: hoje, ou a data de lançamento</div></div></div></div>`,
  'Dá para desfazer na folha do título; a remoção também vai para o Trakt e o Simkl.');
  if (q.result === 'fail') {
    const left = M.triesLeft(S.marks, e.id, Date.now());
    return folha('qz-t', `<span>Já vi · ${esc(e.title)}</span>`, `<h2 class="h2" id="qz-t">Tudo bem, às vezes faz tempo.</h2>
    <p class="lead">Nada foi marcado. ${left ? `Mais ${left} tentativa hoje.` : 'Dá para tentar de novo amanhã.'} Ninguém mais vê isso.</p>
    <div class="opcs">${opc('qz-want', 'qz-want', S.marks.want[e.id] ? 'Já está em Quero ver' : 'Guardar em Quero ver', 'Para rever com calma.', 'salvo')}
      ${left ? opc('qz-retry', 'qz-retry', 'Tentar com outras perguntas', '', 'redo') : ''}
      ${opc('qz-direct', 'qz-direct', 'Marcar como visto mesmo assim', 'Vai para o Trakt e o Simkl, mas não conta para conquistas.', 'ok')}
      ${opc('qz-close', 'close', 'Fechar', '', 'x')}</div>`);
  }
  const cur = q.asked[q.i], L = 'ABCD';
  const demo = i => S.sources && i === cur.answer ? '<em>demo: certa</em>' : '';
  const opts = cur.type === 'still'
    ? `<div class="cenas">${cur.options.map((v, i) => `<button class="cz pintura cena" style="--h:${v}" data-k="qz-${i}" data-act="qz-answer" data-i="${i}" aria-label="Cena ${L[i]}"><span class="letra">${L[i]}</span>${demo(i)}</button>`).join('')}</div>`
    : `<div class="opcs">${cur.options.map((v, i) => `<button class="opc" data-k="qz-${i}" data-act="qz-answer" data-i="${i}"><span class="letra">${L[i]}</span><span><b>${esc(v)}</b></span>${demo(i)}</button>`).join('')}</div>`;
  const prev = q.i ? `${q.last ? 'Certa.' : 'Não era essa.'}${q.bonus && q.i === 2 ? ' Uma pergunta extra para desempatar.' : ''}` : 'Uma checagem leve, não uma prova. Sem spoiler e sem tempo.';
  return folha('qz-t', `<span>Já vi · pergunta ${q.i + 1} de ${q.asked.length}</span><span class="pontos">${q.asked.map((_, i) => `<i class="${i < q.i ? (q.answers[i] ? 'ok' : 'erro') : i === q.i ? 'agora' : ''}"></i>`).join('')}</span>`,
    `<h2 class="h2" id="qz-t">${esc(cur.prompt)}</h2><p class="lead" aria-live="polite">${prev}</p>${opts}
    <div class="linha-chips">${!q.skipped && q.reserve.length ? '<button class="chip" data-k="qz-skip" data-act="qz-skip">Não lembro desta</button>' : ''}<button class="chip" data-k="qz-direct" data-act="qz-direct">Marcar sem responder<span class="v">não conta para conquistas</span></button><button class="chip" data-k="qz-later" data-act="close">Agora não</button></div>`,
    'Sem spoiler: temas e cenas do TMDB passam pela lista de bloqueio antes de virar pergunta. Sair não gasta tentativa. Teclas 1–4 respondem.');
}

function bio(d) {
  return folha('bio-t', '<span>Biografia · TMDB</span>', `<h2 class="h2" id="bio-t">${esc(d.p.name)}</h2><p class="bio-longa">${esc(d.p.bio || d.p.bioEn)}</p><div class="opcs">${opc('bio-close', 'close', 'Fechar', '', 'x')}</div>`);
}

// ---- render and focus
const layer = () => S.overlay ? $('#camada') : $('#tv');
const key = k => k && layer().querySelector(`[data-k="${CSS.escape(k)}"]`);
const focusables = () => [...layer().querySelectorAll('[data-k]')];
function render() {
  const d = data(), hue = S.tab === 'historia' ? d.years.find(t => t.year === curYear(d)).items[0].hue : d.p.hue;
  $('#tv').innerHTML = header(d, hue) + `<div class="corpo">${tabs(d)}<div class="aba">${({ historia, filmografia, sobre, conquistas })[S.tab](d)}</div></div>
    <div class="rodape">${S.tab === 'historia' ? '<span><kbd>←</kbd> <kbd>→</kbd> na régua troca o ano</span><span><kbd>PgUp</kbd> <kbd>PgDn</kbd> capítulo</span>' : ''}<span><kbd>OK</kbd> no cartaz abre a folha do título</span><span><kbd>Esc</kbd> volta</span></div>`;
  $('#camada').innerHTML = S.overlay ? ({ sheet, quiz, bio })[S.overlay.type](d) : '';
  $('#tv').inert = !!S.overlay;
  document.body.classList.toggle('dados', S.sources);
  document.querySelectorAll('[data-pessoa]').forEach(b => b.setAttribute('aria-pressed', b.dataset.pessoa === S.pid));
  $('#fontes').setAttribute('aria-pressed', S.sources);
  focus(key(S.focus) || focusables()[0]);
}
// Focus is real DOM focus (screen readers follow it); the look comes from .foco, as in tv.css.
function mark(el) {
  if (!el.dataset?.k) return;
  $('.foco')?.classList.remove('foco');
  el.classList.add('foco');
  S.focus = el.dataset.k;
  const g = el.closest('.grade');
  if (!S.overlay) $('#tv').classList.toggle('rolou', !!g);
  if (g) {
    if (el.offsetTop < g.scrollTop + 16) g.scrollTop = el.offsetTop - 16;
    else if (el.offsetTop + el.offsetHeight > g.scrollTop + g.clientHeight - 110) g.scrollTop = el.offsetTop + el.offsetHeight - g.clientHeight + 110;
  }
  if (S.focus.startsWith('tab-') && S.tab !== S.focus.slice(4)) { S.tab = S.focus.slice(4); render(); }
}
function focus(el) { if (el) { el.focus({ preventScroll: true }); mark(el); } }
document.addEventListener('focusin', ev => mark(ev.target));

function move(dir) {
  const cur = document.activeElement, list = focusables();
  if (!list.includes(cur)) return focus(list[0]);
  const a = cur.getBoundingClientRect(), horiz = dir === 'left' || dir === 'right';
  let best = null, bs = Infinity;
  for (const el of list) {
    if (el === cur) continue;
    const b = el.getBoundingClientRect();
    const dx = (b.left + b.right - a.left - a.right) / 2, dy = (b.top + b.bottom - a.top - a.bottom) / 2;
    const along = { left: -dx, right: dx, up: -dy, down: dy }[dir];
    if (along <= 1) continue;
    const overlap = horiz ? Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top) : Math.min(a.right, b.right) - Math.max(a.left, b.left);
    const cross = horiz ? Math.abs(dy) : Math.abs(dx);
    // Sideways stays in the row: at the end of a row the press does nothing, as on the TV.
    if (horiz && overlap <= 0) continue;
    const score = along + (overlap > 0 ? 0 : cross * 2) + cross * 0.3;
    if (score < bs) { bs = score; best = el; }
  }
  if (!best) return;
  // Entering a row of tabs or chapters lands on the active one, never on a neighbour by geometry.
  const k = best.dataset.k, ck = cur.dataset.k;
  if (k.startsWith('tab-') && !ck.startsWith('tab-')) best = key('tab-' + S.tab);
  else if (k.startsWith('cap-') && !ck.startsWith('cap-')) best = layer().querySelector('.caps .on') || best;
  else if (k.startsWith('f-') && ck.startsWith('tab-')) best = key('f-sort');
  else if (k.startsWith('ano-') && ck === 'regua') best = key('ano-0');
  focus(best);
}

function setYear(y) { S.year[S.pid] = y; }
function stepYear(delta) {
  const d = data(), i = d.years.findIndex(t => t.year === curYear(d)), n = Math.max(0, Math.min(d.years.length - 1, i + delta));
  if (n === i) return;
  setYear(d.years[n].year); render();
}
function chapterJump(delta) {
  if (S.tab !== 'historia' || S.overlay) return;
  const d = data(), y = curYear(d), ci = d.ch.findIndex(c => y >= c.from && y <= c.to), c = d.ch[Math.max(0, Math.min(d.ch.length - 1, ci + delta))];
  setYear(d.years.find(t => t.year >= c.from).year); render();
}

let toastTimer, avisoTimer;
function toast(html) {
  const t = $('#toast');
  t.innerHTML = html; t.classList.add('show');
  clearTimeout(toastTimer); toastTimer = setTimeout(() => t.classList.remove('show'), 4200);
}
function aviso(a, p) {
  const el = $('#aviso');
  el.innerHTML = `<span class="cq-ic">${ic('claquete')}</span><div><p class="k">Conquista nova <span>Voltar fecha</span></p><b>${esc(a.kind === 'trail' ? M.LEVEL_NAMES[a.level] : a.name)}</b><small>${esc(p.name)} · ${a.seen} de ${a.total} ${a.kind === 'trail' ? 'títulos' : 'passos'}</small></div><span class="trilho"><i style="width:${a.seen / a.total * 100}%"></i></span>`;
  el.classList.add('show');
  clearTimeout(avisoTimer); avisoTimer = setTimeout(() => el.classList.remove('show'), 6000);
}
function open(o) { S.overlay = { ...o, from: S.overlay?.from ?? S.focus }; S.focus = null; render(); }
function back() {
  if ($('#aviso').classList.contains('show')) return $('#aviso').classList.remove('show');
  if (S.overlay) { S.focus = S.overlay.from; S.overlay = null; render(); return; }
  if (S.focus !== 'tab-' + S.tab) return focus(key('tab-' + S.tab));
  toast('No app, Voltar aqui fecha a ficha e volta ao título de onde você veio.');
}
function startQuiz(id) {
  if (!M.triesLeft(S.marks, id, Date.now())) return toast('Tentativas de hoje usadas. Amanhã libera de novo.');
  const d = data(), e = d.all.find(x => x.id === id), qz = M.buildQuiz(e, d.p, (S.marks.fails[id] || []).length + 1);
  S.overlay = { type: 'quiz', id, q: M.startQuiz(qz), from: S.overlay?.from ?? S.focus };
  // Not enough spoiler-safe questions: no quiz, the mark still works but stays out of achievements.
  if (qz.questions.length < 2) S.overlay.how = 'direct';
  S.focus = S.overlay.how ? 'dt-today' : 'qz-0'; render();
}
function save(date) {
  const o = S.overlay, before = data(), e = before.all.find(x => x.id === o.id);
  S.marks = M.withSeen(S.marks, e, Date.now(), date, o.how); persist();
  const got = M.unlocks(before.ach, data().ach), when = M.fmtDate(M.watchedDate(e, date));
  S.focus = o.from; S.overlay = null; render();
  toast(`${esc(e.title)} marcado como visto em ${when}${date === 'release' ? ' (data de lançamento)' : ''} · enviado ao Trakt e ao Simkl${o.how === 'quiz' ? '' : '<small>Sem as perguntas, não conta para conquistas.</small>'}`);
  if (got.length) setTimeout(() => aviso(got[0], before.p), REPLAY ? 0 : 1400);
}

const ACT = {
  tab: el => focus(el),
  cap: el => { const d = data(), c = d.ch[+el.dataset.i]; setYear(d.years.find(t => t.year >= c.from).year); S.focus = 'regua'; render(); },
  noise: () => { S.noise = !S.noise; render(); },
  credit: el => open({ type: 'sheet', id: el.dataset.id }),
  yearGrid: el => { S.filter = { ...M.DEFAULT_FILTER, year: +el.dataset.year }; S.tab = 'filmografia'; S.focus = 'f-year'; render(); },
  filter: el => { S.filter = M.cycle(S.filter, el.dataset.f); render(); },
  clearYear: () => { S.filter = { ...S.filter, year: null }; S.focus = 'f-sort'; render(); },
  follow: () => { const p = M.PERSONS[S.pid]; S.follow[p.id] = !S.follow[p.id]; persist(); render(); toast(S.follow[p.id] ? `Você vai saber quando sair algo novo de ${esc(p.name)}.` : 'Você deixou de seguir.'); },
  bio: () => open({ type: 'bio' }),
  person: el => toast(`No app: abre a ficha de ${esc(el.dataset.name)}.`),
  ach: el => toast(`Abre em Perfil › Conquistas.<small>Chave ${esc(el.dataset.key)}</small>`),
  close: back,
  'sh-open': () => toast('No app: abre a página do título.'),
  'sh-play': () => toast('No app: continua de onde parou.'),
  'sh-quiz': () => startQuiz(S.overlay.id),
  'sh-locked': () => toast('Tentativas de hoje usadas. Amanhã libera de novo.'),
  'sh-unmark': () => { S.marks = M.withoutSeen(S.marks, S.overlay.id); persist(); render(); toast('Marca removida · também do Trakt e do Simkl.'); },
  'sh-want': () => { S.marks = M.toggleWant(S.marks, S.overlay.id); persist(); render(); },
  'qz-answer': el => {
    const o = S.overlay; o.q = M.answer(o.q, +el.dataset.i);
    if (o.q.result === 'pass') o.how = 'quiz';
    if (o.q.result === 'fail') { S.marks = M.withFail(S.marks, o.id, Date.now()); persist(); }
    S.focus = o.how ? 'dt-today' : o.q.result ? 'qz-want' : 'qz-0'; render();
  },
  'qz-skip': () => { S.overlay.q = M.skip(S.overlay.q); S.focus = 'qz-0'; render(); },
  'qz-direct': () => { S.overlay.how = 'direct'; S.focus = 'dt-today'; render(); },
  'qz-save': el => save(el.dataset.date),
  'qz-want': () => { const id = S.overlay.id; if (!S.marks.want[id]) S.marks = M.toggleWant(S.marks, id); persist(); back(); toast('Guardado em Quero ver.'); },
  'qz-retry': () => startQuiz(S.overlay.id),
};
document.addEventListener('click', ev => {
  const el = ev.target.closest('[data-act]');
  if (el && ACT[el.dataset.act]) ACT[el.dataset.act](el);
});
// Returns true when the key was handled.
function press(k) {
  const el = document.activeElement;
  if (k === 'd' || k === 'D') { S.sources = !S.sources; render(); return true; }
  if (S.overlay?.type === 'quiz' && !S.overlay.how && /^[1-4]$/.test(k)) { key('qz-' + (k - 1))?.click(); return true; }
  if (k === 'Enter') { if (el?.dataset.k) el.click(); return true; }
  if (k === 'Escape' || k === 'Backspace') { back(); return true; }
  if (k === 'PageUp' || k === 'PageDown') { chapterJump(k === 'PageDown' ? 1 : -1); return true; }
  const dir = { ArrowLeft: 'left', ArrowRight: 'right', ArrowUp: 'up', ArrowDown: 'down' }[k];
  if (!dir) return false;
  if (el?.dataset.k === 'regua' && !['up', 'down'].includes(dir)) stepYear(dir === 'right' ? 1 : -1);
  else if (el?.dataset.k === 'regua' && dir === 'up') focus(layer().querySelector('.caps .on'));
  else move(dir);
  return true;
}
document.addEventListener('keydown', ev => {
  if (ev.target.closest?.('.labbar') || ev.metaKey || ev.ctrlKey || ev.altKey) return;
  if (press(ev.key)) ev.preventDefault();
});

document.querySelectorAll('[data-pessoa]').forEach(b => b.addEventListener('click', () => {
  Object.assign(S, { pid: b.dataset.pessoa, tab: 'historia', focus: 'regua', overlay: null, filter: { ...M.DEFAULT_FILTER } });
  persist(); render();
}));
$('#fontes').addEventListener('click', () => { S.sources = !S.sources; render(); });
$('#reiniciar').addEventListener('click', () => {
  try { localStorage.removeItem(STORE); } catch {}
  Object.assign(S, { marks: M.emptyMarks(), follow: {}, year: {}, noise: false, overlay: null, tab: 'historia', focus: 'regua', filter: { ...M.DEFAULT_FILTER } });
  render(); toast('Demonstração reiniciada.');
});
function scale() {
  const k = Math.min(innerWidth / 1920, (innerHeight - 44) / 1080), c = $('#caixa');
  c.style.width = 1920 * k + 'px'; c.style.height = 1080 * k + 'px';
  $('#tela').style.transform = `scale(${k})`;
}
addEventListener('resize', scale);
scale();
render();

if (REPLAY) {
  document.body.classList.add('replay');
  const KEYS = { cima: 'ArrowUp', baixo: 'ArrowDown', esq: 'ArrowLeft', dir: 'ArrowRight', ok: 'Enter', voltar: 'Escape', dados: 'D', pgdn: 'PageDown', pgup: 'PageUp' };
  for (const t of Q.get('teclas').split(',').filter(Boolean)) {
    const q = S.overlay?.q;
    if (t === 'certa' || t === 'errada') key('qz-' + (q.asked[q.i].answer + (t === 'errada')) % 4).click();
    else if (t.startsWith('abre:')) open({ type: 'sheet', id: t.slice(5) });
    else press(KEYS[t] || t);
  }
}
