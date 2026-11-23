'use strict';
const GROUPS = {diagnostico:'Farol · diagnóstico', gosto:'Farol · gosto', correcoes:'Correções e portes', prs:'PRs do Poke', ux:'UX/UI'};
const STATES = {A_FAZER:'A fazer',IMPLEMENTANDO:'Implementando',REVISAO:'Em revisão',CORRECAO:'Em correção',APROVADA:'Aprovada',LOTE:'Em lote',PR_ABERTA:'PR aberta',FEITO:'Integrado',BLOQUEADA:'Bloqueado',POR_ULTIMO:'Por último'};
const WAVES = {A:'Fundações',B:'Conectar os motores',C:'Diagnóstico na tela',D:'Costurar a experiência',E:'Frase, gosto e retomada'};
const PAGES = {
  overview:['Tudo no mesmo rumo.','Da primeira branch até a TV. O que avançou, o que falta e o que precisa de você.'],
  board:['O trabalho, sem perder o fio.','Um cartão por item. Abra para ver dependências, referências e o que ainda não foi verificado.'],
  waves:['Uma onda de cada vez.','A–E é a sequência de execução da VPS. A fase numérica do plano aparece no detalhe.'],
  gates:['A próxima parte é com você.','Testes reais e deploys que só o Henrique pode confirmar. Código integrado não encerra esta lista.'],
  integration:['Da branch à integração.','Aprovado não é integrado. Um lote só muda de estado quando houver um novo relato.']
};
let data = null, currentView = 'overview', group = '', wave = '', status = '', query = '', activeItem = null;
const $ = s => document.querySelector(s);
const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const short = value => value ? String(value).slice(0,9) : 'não informado';
const itemLabel = i => i.displayId || (i.id.startsWith('farol-') ? `FAROL ${i.number}` : i.id.startsWith('issue-') ? `ISSUE #${i.number}` : i.id.startsWith('pr-') ? `PR #${i.number || i.id.slice(3)}` : i.id);
const badge = state => `<span class="badge status-${esc(state)}">${esc(STATES[state] || state)}</span>`;
const itemById = id => data.items.find(i => i.id === id);
const done = items => items.filter(i => i.status === 'FEITO').length;
const pendingGates = () => data.gates.filter(g => g.status !== 'feito');
const date = (value, full = false) => {
  const d = new Date(value);
  return Number.isNaN(d.valueOf()) ? 'não informada' : d.toLocaleString('pt-BR', {timeZone:'America/Sao_Paulo',day:'2-digit',month:'2-digit',...(full ? {year:'numeric'} : {}),hour:'2-digit',minute:'2-digit'});
};
const safeLink = value => {
  try { const u = new URL(value); return u.protocol === 'https:' && ['github.com','app.devin.ai'].includes(u.hostname) ? esc(u.href) : ''; } catch { return ''; }
};
const githubLink = i => safeLink(i.url || '');
const dependencyList = i => i.allDependencies ? data.items.filter(x => x.id !== i.id && (x.group === 'diagnostico' || (i.group === 'gosto' && x.group === 'gosto')) && !x.allDependencies).map(x => x.id) : i.dependencies || [];
const waiting = i => dependencyList(i).filter(id => itemById(id)?.status !== 'FEITO');
const meter = (value, total, cls = '') => `<progress class="meter ${cls}" value="${value}" max="${total || 1}" aria-label="${value} de ${total} integrados"></progress>`;

function setView(view, newGroup = '', newWave = '') {
  currentView = view; group = newGroup; wave = newWave; status = ''; query = '';
  render(); window.scrollTo({top:0,behavior:'instant'});
}

function render() {
  if (!data) return;
  $('#page-title').textContent = group && currentView === 'board' ? GROUPS[group] : PAGES[currentView][0];
  $('#page-description').textContent = PAGES[currentView][1];
  $('#gate-count').textContent = pendingGates().length;
  document.querySelectorAll('#nav button').forEach(b => {b.classList.toggle('active', b.dataset.view === currentView && !group);b.setAttribute('aria-current',b.classList.contains('active')?'page':'false');});
  $('#fronts').innerHTML = Object.entries(GROUPS).map(([key,name]) => `<button data-group="${key}" class="${group === key?'active':''}"><span class="front-dot ${key}"></span>${name.replace('Farol · ','Farol ')}<span class="nav-count">${data.items.filter(i=>i.group===key).length}</span></button>`).join('');
  $('#view').innerHTML = ({overview:overview,board:board,waves:waves,gates:gates,integration:integration})[currentView]();
}

function overview() {
  const farol = data.items.filter(i => ['diagnostico','gosto'].includes(i.group));
  const fd = done(farol), approved = data.items.filter(i=>i.status==='APROVADA').length;
  const totalDone = done(data.items), percent = Math.round(fd / farol.length * 100);
  const batchCount = data.items.filter(i=>i.status==='LOTE').length;
  const fc = s => farol.filter(i=>s.includes(i.status)).length;
  const segments = [[fd,'done'],[fc(['APROVADA','REVISAO','PR_ABERTA']),'review'],[fc(['LOTE']),'batch'],[fc(['IMPLEMENTANDO','CORRECAO']),'active'],[fc(['BLOQUEADA']),'blocked']];
  const activeCount=fc(['IMPLEMENTANDO','CORRECAO']), blockedCount=fc(['BLOQUEADA']);
  const accessibleSummary=`Integrados: ${fd}; em revisão ou aprovados: ${fc(['APROVADA','REVISAO','PR_ABERTA'])}; em lote: ${fc(['LOTE'])}; em andamento: ${activeCount}; bloqueados: ${blockedCount}; a fazer ou por último: ${fc(['A_FAZER','POR_ULTIMO'])}. Total ${farol.length} itens.`;
  let cursor = 0;
  const svg = segments.map(([n,c]) => {const x=cursor;cursor+=n;return `<rect x="${x}" y="0" width="${n}" height="8" class="fill-${c}"/>`;}).join('');
  return `<section class="metrics" aria-label="Resumo da release">
    <div class="metric"><div class="metric-label">Farol integrado <span class="metric-icon">◴</span></div><div class="metric-value">${fd} <small>/ ${farol.length}</small></div><div class="metric-note">${percent}% do escopo Farol informado</div></div>
    <div class="metric"><div class="metric-label">Na fila de integração <span class="metric-icon">⑂</span></div><div class="metric-value">${approved + batchCount}<small> itens</small></div><div class="metric-note">${approved} aprovados · ${batchCount} em lote</div></div>
    <div class="metric"><div class="metric-label">PRs do Poke abertas <span class="metric-icon">↗</span></div><div class="metric-value">${data.items.filter(i=>i.group==='prs'&&(i.githubState==='open'||i.status==='PR_ABERTA')).length}<small> PRs</small></div><div class="metric-note">Separadas do escopo da VPS</div></div>
    <div class="metric"><div class="metric-label">Depende de você <span class="alert-icon">◷</span></div><div class="metric-value">${pendingGates().length}<small> pendências</small></div><div class="metric-note">TV física e publicação do worker</div></div>
  </section>
  <div class="overview-grid"><section class="panel"><div class="panel-heading"><div><h2>O Farol está tomando forma</h2><p class="subheading">Diagnóstico + gosto · do planejamento à integração</p></div><button class="text-link" data-view="board">Ver quadro ↗</button></div>
    <div class="progress-summary"><strong>${percent}%</strong><span>integrado · ${farol.length-fd} itens pela frente</span></div>
    <svg class="stacked-progress" viewBox="0 0 ${farol.length} 8" preserveAspectRatio="none" role="img" aria-label="${esc(accessibleSummary)}"><rect width="${farol.length}" height="8" class="fill-pending"/>${svg}</svg>
    <div class="progress-legend"><span><i class="legend-dot"></i>Integrado ${fd}</span><span><i class="legend-dot review"></i>Revisão / aprovado ${fc(['APROVADA','REVISAO','PR_ABERTA'])}</span><span><i class="legend-dot batch"></i>Em lote ${fc(['LOTE'])}</span>${activeCount?`<span><i class="legend-dot active"></i>Em andamento ${activeCount}</span>`:''}${blockedCount?`<span><i class="legend-dot blocked"></i>Bloqueado ${blockedCount}</span>`:''}<span>A fazer + fechos ${fc(['A_FAZER','POR_ULTIMO'])}</span></div>
    ${Object.entries(GROUPS).map(([key,name])=>{const list=data.items.filter(i=>i.group===key);return `<div class="front-row"><div class="front-title"><span class="front-dot ${key}"></span><div><button data-group="${key}">${name}</button><small>${key==='prs'?'Frente paralela · escopo sinalizado por item':key==='ux'?'Tokens, primitivas e coerência visual':`${list.length} itens identificados`}</small></div></div>${meter(done(list),list.length)}<span class="fraction">${done(list)}/${list.length}</span></div>`;}).join('')}
  </section><aside class="attention-column"><section class="panel attention"><div class="panel-heading"><h2><span class="alert-icon">◷</span> Precisa do Henrique</h2><span class="badge status-LOTE">${pendingGates().length}</span></div>
    ${pendingGates().filter(g=>['tv-680','tv-778','worker'].includes(g.id)).map(g=>`<button class="gate-preview" data-view="gates"><strong>${esc(g.title)} ↗</strong><span>${g.kind==='tv'?'Teste em TV física pendente':'Deploy manual · não confirmado'}</span></button>`).join('')}
    <div class="attention-bottom"><button class="text-link" data-view="gates">Ver todas as pendências →</button></div></section>
    <section class="panel small-panel"><div class="eyebrow">INTEGRAÇÃO PUBLICADA</div><div class="integration-ref">⑂ ${esc(data.meta.integration.branch)}</div><p><a href="${data.meta.repository}/commit/${esc(data.meta.integration.sha)}" target="_blank" rel="noopener"><code>${esc(data.meta.integration.sha)}</code> ↗</a> · PR #${data.meta.integration.pr}</p><p class="ref-caption">${totalDone} itens identificados integrados</p></section>
  </aside></div>
  <div class="section-heading"><div><h2>As próximas ondas</h2><span class="muted tiny">Sequência de execução</span></div><button class="text-link" data-view="waves">Ver dependências ↗</button></div>
  <div class="wave-strip">${Object.entries(WAVES).map(([key,title])=>{const list=farol.filter(i=>i.wave===key);return `<button class="wave-tile" data-wave="${key}"><span class="wave-id">ONDA ${key}</span><strong>${title}</strong><small>${list.length} itens · ${list.filter(i=>waiting(i).length===0).length} sem dependência aberta</small>${meter(done(list),list.length)}</button>`;}).join('')}</div>
  <div class="section-heading"><h2>Dois pontos no caminho crítico</h2><span class="muted tiny">Destravem estes, o resto anda</span></div>
  <div class="critical-list">${['farol-14','farol-52'].map(id=>{const i=itemById(id);return `<button class="critical-card" data-item="${id}"><span class="item-id">${itemLabel(i)} · ONDA ${i.wave}</span><strong>${esc(i.title)} ↗</strong><p>${waiting(i).length} dependências ainda não integradas · ${esc(STATES[i.status])}</p></button>`;}).join('')}</div>
  <div class="notice"><strong>Transparência do escopo.</strong> ${esc(data.meta.coverageNote)} Os números são calculados do JSON, não de uma estimativa. Relato VPS e estado no GitHub não substituem validação funcional.</div>`;
}

function filteredItems() {
  const q = query.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase();
  return data.items.filter(i=>(!group||i.group===group)&&(!wave||(wave==='none'?!i.wave:i.wave===wave))&&(!status||i.status===status)&&(!q||`${itemLabel(i)} ${i.title} ${i.note} ${i.branch||''} ${i.sha||''}`.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase().includes(q)));
}

function toolbar() {
  return `<div class="toolbar"><label class="search"><span aria-hidden="true">⌕</span><input id="search" type="search" aria-label="Buscar item, PR ou commit" placeholder="Buscar item, PR ou commit…" value="${esc(query)}"></label>
    <select id="group-filter" aria-label="Filtrar frente"><option value="">Todas as frentes</option>${Object.entries(GROUPS).map(([k,v])=>`<option value="${k}" ${group===k?'selected':''}>${v}</option>`).join('')}</select>
    <select id="wave-filter" aria-label="Filtrar onda"><option value="">Todas as ondas</option>${Object.entries(WAVES).map(([k])=>`<option value="${k}" ${wave===k?'selected':''}>Onda ${k}</option>`).join('')}<option value="none" ${wave==='none'?'selected':''}>Sem onda informada</option></select>
    <select id="status-filter" aria-label="Filtrar estado"><option value="">Todos os estados</option>${Object.entries(STATES).map(([k,v])=>`<option value="${k}" ${status===k?'selected':''}>${v}</option>`).join('')}</select>
    ${group||wave||status||query?'<button class="clear" id="clear">Limpar filtros</button>':''}</div>`;
}

function card(i) {
  const deps=waiting(i).length, gates=data.gates.filter(g=>g.status!=='feito'&&g.items.includes(i.id));
  return `<button class="item-card" data-item="${esc(i.id)}"><span class="card-top"><span class="item-id">${esc(itemLabel(i))}</span>${badge(i.status)}</span><h3>${esc(i.title)}</h3><span class="card-bottom"><span class="front-dot ${i.group}"></span>${i.wave?`Onda ${esc(i.wave)}`:i.batch?`Lote ${esc(i.batch)}`:esc(GROUPS[i.group])}<span class="dependency-count">${deps?`⑂ ${deps} pendente${deps>1?'s':''}`:'↗'}</span></span>${gates.length?'<div class="card-flag">◷ Depende do Henrique</div>':i.scope==='a_confirmar'?'<div class="card-flag">Escopo da rodada a confirmar</div>':''}</button>`;
}

function board() {
  const list = filteredItems();
  const columns=[['A fazer','pending',['A_FAZER','POR_ULTIMO']],['Em andamento','active',['IMPLEMENTANDO','CORRECAO','BLOQUEADA']],['Revisão e PRs','review',['REVISAO','APROVADA','PR_ABERTA']],['Em lote','batch',['LOTE']],['Integrado','done',['FEITO']]];
  return `${toolbar()}<div class="board-meta"><span>${list.length} de ${data.items.length} itens identificados${wave?` · ${wave==='none'?'sem onda':`onda ${esc(wave)}`}`:''}</span><span>FEITO = na integração, não necessariamente testado na TV</span></div>${!list.length?'<div class="no-results">Nenhum item com estes filtros.<br><button class="text-link" id="clear">Limpar filtros →</button></div>':`<div class="board">${columns.map(([title,cls,states])=>{const rows=list.filter(i=>states.includes(i.status));return `<section class="board-column"><h2 class="column-heading"><span class="column-dot ${cls}"></span>${title}<span class="nav-count">${rows.length}</span></h2>${rows.map(card).join('')||'<div class="empty-column">Nenhum item por aqui</div>'}</section>`;}).join('')}</div>`}`;
}

function waves() {
  const waveKeys=[...Object.keys(WAVES),'none'];
  return `<div class="notice view-note">Dependências só ficam concluídas com <strong>FEITO</strong>. APROVADA e LOTE ainda não destravam a integração; isto não muda o estado de execução relatado pela VPS.</div>${waveKeys.map(key=>{
    const list=data.items.filter(i=>key==='none'?!i.wave:i.wave===key);
    return `<section class="wave-detail"><header><div><h2>${key==='none'?'Sem onda informada / já integrados':`Onda ${key} · ${WAVES[key]}`}</h2><p class="subheading">${list.length} itens · ${done(list)} integrados</p></div>${key!=='none'?`<button class="text-link" data-wave="${key}">Abrir no quadro ↗</button>`:''}</header><div class="wave-rows">${list.map(i=>{const deps=dependencyList(i);return `<button class="wave-row" data-item="${esc(i.id)}"><span class="item-id">${esc(itemLabel(i))}</span><span class="row-title">${esc(i.title)}<small>${i.allDependencies?'Fecho: depende de todas as entregas anteriores':deps.length?`Depende de ${deps.map(id=>itemLabel(itemById(id))).join(', ')} · ${waiting(i).length} em aberto`:i.group==='prs'?'Dependências não informadas':'Sem dependências informadas'}</small></span>${badge(i.status)}</button>`;}).join('')}</div></section>`;
  }).join('')}`;
}

function gates() {
  return `<div class="view-note">As pendências são confirmadas pela coordenação a partir do seu retorno. Este painel não executa deploy nem marca testes como feitos automaticamente.</div><div class="gates-grid">${pendingGates().map(g=>`<article class="gate-card"><span class="badge ${g.kind==='tv'?'status-LOTE':'status-REVISAO'}">${g.kind==='tv'?'◷ Teste em TV física':g.kind==='deploy'?'⑂ Deploy do worker':g.kind==='functional'?'◷ Teste funcional':'◇ Decisão de produto'}</span><h3>${esc(g.title)}</h3><p>${esc(g.note)}</p><div class="gate-links">${g.items.map(id=>`<button data-item="${esc(id)}">${esc(itemLabel(itemById(id)))} ↗</button>`).join('')||'<span class="tiny muted">Sem issue numerada no relato</span>'}</div></article>`).join('')}</div><div class="notice">Decisões de produto aparecerão aqui quando forem reportadas. A inclusão de #821 e #830 nesta rodada ainda não foi confirmada; ambas já estão identificadas como trabalho do Poke no quadro.</div>`;
}

function integration() {
  return `<div class="notice view-note"><strong>Publicado:</strong> ${esc(data.meta.integration.branch)} @ ${esc(data.meta.integration.sha)} · <a href="${data.meta.repository}/pull/${data.meta.integration.pr}" target="_blank" rel="noopener">PR #${data.meta.integration.pr} ↗</a>. O painel não faz merge nem deploy do produto.</div><div class="batch-list">${data.batches.map(b=>`<article class="batch-card"><div class="batch-header"><h2>${esc(b.title)}</h2>${badge(b.status)}</div><p>${esc(b.note)}</p><div class="batch-ref"><code>${esc(b.branch || 'integracao/2.1')}@${esc(b.sha || 'sem ponta informada')}</code>${b.pr?`<a class="text-link" href="${data.meta.repository}/pull/${b.pr}" target="_blank" rel="noopener">PR #${b.pr} ↗</a>`:'<span class="tiny muted">PR não confirmada</span>'}</div><div class="batch-items">${data.items.filter(i=>i.batch===b.id).map(i=>`<button data-item="${esc(i.id)}">${esc(itemLabel(i))}</button>`).join('')}</div></article>`).join('')}</div><details class="outside"><summary>Fora desta rodada e para depois</summary><ul>${data.outside.map(o=>`<li><strong>${esc(o.title)}</strong> — ${esc(o.note)}</li>`).join('')}</ul></details><div class="section-heading"><h2>Últimos relatos recebidos</h2></div><div class="history-list">${data.events.length?data.events.slice(0,30).map(ev=>`<div class="history-row"><small>${date(ev.at,true)} · horário de Brasília</small><span class="event-report">${esc(ev.report)}</span></div>`).join(''):'<p class="tiny muted">O escopo inicial está carregado. Novos relatos da VPS aparecerão aqui após a atualização do JSON.</p>'}</div>`;
}

function showDetail(id) {
  const i=itemById(id); if (!i) return;
  activeItem=id;
  const deps=dependencyList(i), downstream=data.items.filter(x=>(x.dependencies||[]).includes(id));
  const gates=data.gates.filter(g=>g.items.includes(id)&&g.status!=='feito');
  const link=githubLink(i);
  $('#detail-body').innerHTML=`<div class="detail-id">${esc(itemLabel(i))} · ${esc(GROUPS[i.group])}</div><h2 id="detail-title">${esc(i.title)}</h2>${badge(i.status)}
    <dl class="detail-fields"><div><dt>Onda de execução</dt><dd>${i.wave?`Onda ${esc(i.wave)}`:'Não informada'}</dd></div><div><dt>Fase do plano</dt><dd>${i.phase!==undefined&&i.phase!==null?esc(i.phase):'Não informada'}</dd></div><div><dt>Lote previsto / atual</dt><dd>${esc(i.batch||'Não informado')}</dd></div><div><dt>Responsável</dt><dd>${esc(i.owner||'VPS')}</dd></div><div><dt>Branch</dt><dd><code>${esc(i.branch||'Não informada')}</code></dd></div><div><dt>Commit</dt><dd>${i.sha?`<a href="${data.meta.repository}/commit/${esc(i.sha)}" target="_blank" rel="noopener"><code>${esc(short(i.sha))}</code> ↗</a>`:'Não informado'}</dd></div></dl>
    <div class="detail-section"><h3>Observações e verificação</h3><p>${esc(i.note||'Sem observações adicionais no relato.')}</p>${i.commitRole==='base'?'<p>O SHA informado é a base de trabalho; ainda não há commit de implementação.</p>':''}${i.githubState?`<p>GitHub: ${i.githubState==='open'?'PR aberta':i.githubState==='merged'?'PR mesclada':'PR fechada'} (última consulta). A ordem de integração é independente deste estado.</p>`:''}<p>${i.integrationVerified?'Ancestralidade na integração verificada. ':''}Validação funcional: ${i.verificationSummary?esc(i.verificationSummary):i.verified?'verificada':'não verificada'}. ${i.allDependencies?'Fecho: depende de todas as entregas anteriores.':''}</p></div>
    ${i.previousNotes?.length?`<details class="outside"><summary>Contexto dos relatos anteriores</summary>${i.previousNotes.map(note=>`<p class="tiny muted">${esc(note)}</p>`).join('')}</details>`:''}
    ${gates.length?`<div class="notice"><strong>Depende do Henrique</strong><br>${gates.map(g=>`${esc(g.title)} — ${esc(g.note)}`).join('<br>')}</div>`:''}
    <div class="detail-section"><h3>Dependências ${deps.length?`· ${waiting(i).length} em aberto`:''}</h3>${deps.length?deps.map(dep=>{const d=itemById(dep);return `<button class="dependency-button" data-item="${esc(dep)}"><span>${esc(itemLabel(d))} · ${esc(d.title)}</span>${badge(d.status)}</button>`;}).join(''):'<p>Nenhuma informada. Não é confirmação de ausência de dependências.</p>'}</div>
    ${downstream.length?`<div class="detail-section"><h3>Destrava diretamente · ${downstream.length} itens</h3><div class="gate-links">${downstream.map(x=>`<button data-item="${esc(x.id)}">${esc(itemLabel(x))} ↗</button>`).join('')}</div></div>`:''}
    ${link?`<a class="detail-link" href="${link}" target="_blank" rel="noopener">Abrir no GitHub ↗</a>`:''}<div class="detail-source">Fonte: ${esc(i.source)}${i.updatedAt?` · ${date(i.updatedAt,true)}`:''}</div>`;
  if (!$('#detail').open) $('#detail').showModal();
  $('#close-detail').focus();
}

document.addEventListener('click', event=>{
  const item=event.target.closest('[data-item]'); if (item) {showDetail(item.dataset.item);return;}
  const front=event.target.closest('[data-group]'); if (front) {setView('board',front.dataset.group);return;}
  const w=event.target.closest('[data-wave]'); if (w) {setView('board','',w.dataset.wave);return;}
  const v=event.target.closest('[data-view]'); if (v) {setView(v.dataset.view);return;}
  if (event.target.closest('#clear')) {group='';wave='';status='';query='';render();}
  if (event.target.closest('.brand')) {event.preventDefault();setView('overview');}
});
document.addEventListener('input',event=>{
  if (event.target.id==='search') {
    const position=event.target.selectionStart; query=event.target.value;
    $('#view').innerHTML=board(); const input=$('#search');input.focus();
    try {input.setSelectionRange(position,position);} catch {}
  }
});
document.addEventListener('change',event=>{
  if (event.target.id==='group-filter') group=event.target.value;
  else if (event.target.id==='wave-filter') wave=event.target.value;
  else if (event.target.id==='status-filter') status=event.target.value;
  else return;
  render();
});
$('#close-detail').addEventListener('click',()=>$('#detail').close());
$('#detail').addEventListener('click',event=>{if(event.target===$('#detail')){const r=event.target.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)event.target.close();}});
$('#detail').addEventListener('close',()=>activeItem=null);
$('#refresh').addEventListener('click',()=>load(true));
$('#export').addEventListener('click',()=>{
  if (!data) return;
  const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));
  const a=document.createElement('a');a.href=url;a.download='nuvio-2.1.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
});
async function load(manual=false) {
  try {
    const response=await fetch(`release.json?t=${Date.now()}`,{cache:'no-store'});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const fresh=await response.json();
    if (!Array.isArray(fresh.items)||!fresh.meta||!Array.isArray(fresh.gates)||!Array.isArray(fresh.batches)||!Array.isArray(fresh.events)||!Array.isArray(fresh.outside)) throw new Error('JSON incompleto');
    const changed=JSON.stringify(data)!==JSON.stringify(fresh);data=fresh;
    $('#sync').textContent=`Dados de ${date(data.meta.updatedAt)} · Brasília`;
    $('#error').hidden=true;
    if (changed||manual) {
      const focus=document.activeElement?.id, selection=document.activeElement?.selectionStart;
      const scroll=window.scrollY, boardScroll=$('.board')?.scrollLeft||0;
      render();
      if (activeItem) {const d=$('#detail');const top=d.scrollTop;showDetail(activeItem);d.scrollTop=top;}
      if (focus&&document.getElementById(focus)) {const el=document.getElementById(focus);el.focus({preventScroll:true});if (selection!=null&&el.setSelectionRange)try{el.setSelectionRange(selection,selection);}catch{}}
      if ($('.board')) $('.board').scrollLeft=boardScroll;
      window.scrollTo(0,scroll);
    }
  } catch (error) {
    $('#error').hidden=false;$('#error').textContent=data?'Não foi possível atualizar. Os últimos dados continuam na tela; tente atualizar novamente.':'Não foi possível carregar o acompanhamento. Use Atualizar dados para tentar novamente.';
    $('#sync').textContent='Atualização indisponível';
    if (!data) $('#view').innerHTML='<div class="no-results">Os dados ainda não estão disponíveis.</div>';
  }
}
load();setInterval(()=>load(),30000);
