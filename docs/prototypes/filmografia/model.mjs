/**
 * Filmografia 2.1 — pure model for the UX prototype.
 * Fake people shaped like TMDB /person?append_to_response=combined_credits,
 * plus watched states as Nuvio/Trakt/Simkl would report them.
 * No DOM, network, storage or timers: app.js owns all of that.
 */
export const TODAY = '2026-10-13';
export const THIS_YEAR = 2026;

export const rand = seed => () => {
  seed = seed + 0x6D2B79F5 | 0;
  let t = Math.imul(seed ^ seed >>> 15, 1 | seed);
  t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
  return ((t ^ t >>> 14) >>> 0) / 4294967296;
};
export const hash = s => { let h = 2166136261; for (const c of String(s)) h = Math.imul(h ^ c.charCodeAt(0), 16777619); return h >>> 0; };
const pick = (r, a) => a[Math.floor(r() * a.length)];
const shuffle = (r, a) => { a = [...a]; for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(r() * (i + 1)); [a[i], a[j]] = [a[j], a[i]]; } return a; };

// Spoiler-safe keywords per genre (TMDB /keywords, curated). Spoilers live apart and never reach the quiz.
export const GENRES = {
  Drama: ['fazenda de família', 'irmãs', 'cidade portuária', 'carta antiga', 'coral da igreja', 'inverno rigoroso'],
  Thriller: ['balsa noturna', 'fita cassete', 'farol', 'chantagem', 'ilha isolada', 'ponte interditada'],
  Comédia: ['casamento', 'food truck', 'vizinhos', 'mudança de casa', 'karaokê', 'concurso de bolos'],
  'Ficção científica': ['estação orbital', 'sinal de rádio', 'eclipse', 'colônia em Marte', 'inteligência artificial', 'viagem no tempo'],
  Animação: ['raposa falante', 'reino submarino', 'robô jardineiro', 'floresta mágica', 'lanterna', 'dragão tímido'],
  Documentário: ['pescadores', 'arquivo de rádio', 'glaciar', 'orquestra jovem', 'fábrica fechada', 'eleição local'],
  Romance: ['livraria', 'trem noturno', 'verão em Lisboa', 'cartas', 'casamento cancelado', 'aula de dança'],
  Policial: ['delegacia', 'cofre', 'ourives', 'perseguição', 'testemunha', 'assalto a banco'],
  Talk: ['entrevista', 'plateia'], News: ['notícias'], Reality: ['competição'],
};
const NOISE_GENRES = ['Talk', 'News', 'Reality'];
const ADJ = ['Quiet', 'Paper', 'Night', 'Glass', 'Hollow', 'Low', 'Salt', 'Cold', 'Amber', 'Red', 'Gray', 'Winter', 'Still', 'Harbor', 'Iron', 'Silver', 'Northern', 'Last', 'Blue', 'Open', 'Burning', 'Small', 'Long', 'Wild', 'Pale', 'Hidden', 'Silent', 'Broken', 'Golden', 'Lost'];
const NOUN = ['Harbor', 'Tides', 'Ferry', 'Orchard', 'Kingdom', 'Tide', 'Bridge', 'Index', 'Line', 'Meridian', 'Season', 'Sound', 'Water', 'Lights', 'Garden', 'Signal', 'Room', 'Coast', 'Road', 'Letters', 'House', 'Field', 'Summer', 'Mirror', 'Station', 'Island', 'Engine', 'Valley', 'Choir', 'Lantern'];
const NAMES = ['Ines', 'Ruth', 'Maja', 'Clara', 'Nina', 'Ada', 'Sol', 'Vera', 'Jonas', 'Elias', 'Tove', 'Kai', 'Lena', 'Oskar', 'Freya', 'Iris', 'Mateo', 'Noor', 'Pia', 'Rafa', 'Saga', 'Tilde', 'Ugo', 'Wen'];
const PEOPLE = ['Jonas Ek', 'Ada Moss', 'Vera Kell', 'J. Ostrow', 'Pia Lund', 'Kai Morel', 'Noor Haddad', 'Elias Brandt', 'Tove Sand', 'Iris Okon', 'Mateo Ruiz', 'Wen Li'];
// Quiz decoys never appear in any credit, so a wrong option is never "kind of right".
// In the app they come from TMDB /similar of the same title.
const DECOY_NAMES = ['Agnes', 'Bruno', 'Celia', 'Dario', 'Edith', 'Fabian', 'Greta', 'Hugo', 'Ilse', 'Joaquim', 'Karin', 'Lucio'];
const DECOY_PEOPLE = ['Marta Quist', 'Paulo Serra', 'Hanne Berg', 'Yusuf Kaya', 'Lotte Viana', 'Gil Amaro', 'Sara Lindh', 'Theo Marsh'];

function row(o) {
  const kind = o.kind || 'movie', genre = o.genre || 'Drama';
  const id = o.id || `${kind}-${hash(o.title + o.year) % 1e6}`;
  const h = hash(id), pool = GENRES[genre];
  return {
    id, kind, title: o.title, year: o.year, genre, upcoming: !!o.upcoming,
    format: o.format || (kind === 'tv' ? 'Série' : genre === 'Documentário' ? 'Documentário' : 'Longa'),
    runtime: o.runtime || (kind === 'movie' ? 85 + h % 50 : 0),
    // eps = episodes the person is in (TMDB episode_count); epsTotal = episodes the series has.
    eps: o.eps || 0, epsTotal: o.epsTotal || o.eps || 0,
    release: o.release || `${o.year}-${String(1 + h % 12).padStart(2, '0')}-${String(1 + (h >>> 4) % 28).padStart(2, '0')}`,
    dept: o.dept || 'Acting', character: o.character || '', order: o.order ?? 9, votes: o.votes ?? 100,
    with: o.with || [], hue: o.hue ?? h % 360,
    keywords: o.keywords || [...new Set([pool[h % pool.length], pool[(h >>> 5) % pool.length]])],
    spoilers: o.spoilers || [], stillBlocked: !!o.stillBlocked,
  };
}

function fillers(r, n, used, make) {
  const out = [];
  while (out.length < n) {
    const title = `${pick(r, ADJ)} ${pick(r, NOUN)}`;
    if (used.has(title)) continue;
    used.add(title);
    out.push(row({ title, ...make(r) }));
  }
  return out;
}
const year = (r, a, b) => a + Math.floor(r() * (b - a + 1));

function mara() {
  const rows = [
    row({ title: 'Salt Bridge', year: 2026, dept: 'Directing', votes: 0, upcoming: true, runtime: 112 }),
    row({ title: 'Iron Coast', year: 2025, character: 'Freya', order: 2, votes: 700, with: ['Ada Moss'] }),
    row({ title: 'The Quiet Harbor', year: 2024, character: 'Ines Hale', order: 0, votes: 9400, runtime: 128, with: ['Jonas Ek', 'Ada Moss'], release: '2024-09-20',
      keywords: ['cidade portuária', 'irmãs', 'o irmão está vivo', 'twist ending'], spoilers: ['o irmão está vivo'] }),
    row({ title: 'Kväll med Linnea', year: 2024, kind: 'tv', genre: 'Talk', character: 'Self', eps: 1, votes: 30 }),
    row({ title: 'Gray Season', year: 2023, kind: 'tv', character: 'Ada Moss', order: 0, eps: 10, votes: 2100, genre: 'Thriller' }),
    row({ title: 'Paper Tides', year: 2022, character: 'Nora Vik', order: 0, votes: 5200, runtime: 104, with: ['J. Ostrow', 'Ada Moss'] }),
    row({ title: 'Paper Tides', year: 2022, dept: 'Writing', votes: 5200, with: ['J. Ostrow'] }),
    row({ title: 'Amber Line', year: 2020, kind: 'tv', character: 'Sol', order: 2, eps: 6, votes: 1500, genre: 'Policial' }),
    row({ title: 'Night Ferry', year: 2019, character: 'Ruth', order: 3, votes: 3300, runtime: 99, genre: 'Thriller', with: ['Jonas Ek'], stillBlocked: true }),
    row({ title: 'Cold Index', year: 2018, dept: 'Writing', votes: 1100, runtime: 96, genre: 'Policial', with: ['J. Ostrow'] }),
    row({ title: 'Cold Index', year: 2018, character: 'Liv', order: 4, votes: 1100, genre: 'Policial' }),
    row({ title: 'Glass Orchard', year: 2017, dept: 'Directing', votes: 140, runtime: 18, format: 'Curta' }),
    row({ title: 'Red Meridian', year: 2016, character: 'Vera Kell', order: 2, votes: 2600, runtime: 117, with: ['Jonas Ek', 'Ada Moss'] }),
    row({ title: 'Winter Sound', year: 2015, dept: 'Directing', votes: 900, runtime: 102 }),
    row({ title: 'Hollow Kingdom', year: 2014, kind: 'tv', character: 'Maja', order: 2, eps: 8, epsTotal: 24, votes: 1200, with: ['Jonas Ek'] }),
    row({ title: 'Still Water', year: 2012, dept: 'Directing', votes: 260, runtime: 74, genre: 'Documentário' }),
    row({ title: 'Low Tide', year: 2011, character: 'Clara', order: 5, votes: 800, runtime: 91, with: ['Jonas Ek'] }),
    row({ title: 'Harbor Lights', year: 2009, kind: 'tv', character: 'Nina', eps: 1, votes: 90 }),
  ];
  const used = new Set(rows.map(c => c.title));
  const r = rand(11);
  rows.push(...fillers(r, 30, used, r => {
    const tv = r() < 0.3;
    return { year: year(r, 2012, 2024), kind: tv ? 'tv' : 'movie', genre: pick(r, ['Drama', 'Thriller', 'Romance', 'Policial', 'Comédia']),
      character: pick(r, NAMES), order: 2 + Math.floor(r() * 10), eps: tv ? (r() < 0.5 ? 1 : 3 + Math.floor(r() * 8)) : 0,
      votes: 60 + Math.floor(r() * 2400), with: r() < 0.12 ? [pick(r, PEOPLE)] : [] };
  }));
  const seen = {
    'The Quiet Harbor': { state: 'seen', src: 'trakt', at: '2025-01-10' },
    'Night Ferry': { state: 'seen', src: 'simkl', at: '2023-05-02' },
    'Low Tide': { state: 'seen', src: 'nuvio', at: '2024-11-20' },
    'Red Meridian': { state: 'seen', src: 'trakt', at: '2022-02-01' },
    'Winter Sound': { state: 'seen', src: 'trakt', at: '2021-09-09' },
    'Glass Orchard': { state: 'seen', src: 'simkl', at: '2021-09-10' },
    'Hollow Kingdom': { state: 'seen', src: 'simkl', at: '2020-06-01' },
    'Gray Season': { state: 'progress', src: 'nuvio', pct: 60 }, // 6 of the 10 episodes she is in
  };
  rows.slice(-30).filter(c => c.eps !== 1 && c.eps !== 2).slice(0, 6)
    .forEach((c, i) => { seen[c.title] = { state: 'seen', src: ['trakt', 'simkl', 'nuvio'][i % 3], at: `202${i % 5}-0${1 + i}-15` }; });
  return {
    id: 'mara', tmdb: 1204418, name: 'Mara Lindqvist', short: 'Mara', dept: 'Acting', initials: 'ML', hue: 215,
    born: '1982-04-03', died: '', place: 'Estocolmo, Suécia', aka: ['Mara Lindquist', 'Мара Линдквист'],
    bio: 'Mara Lindqvist estudou teatro em Estocolmo e começou na televisão sueca antes de chegar ao cinema com Low Tide (2011). Alterna papéis em dramas portuários e thrillers com trabalhos atrás da câmera: dirigiu o documentário Still Water e o curta Glass Orchard, premiado em Gotemburgo. Escreve em parceria com J. Ostrow, com quem assinou Cold Index e Paper Tides. The Quiet Harbor (2024) deu a ela o Guldbagge de atuação e a projeção internacional. Em 2026 lança Salt Bridge, seu primeiro longa de ficção como diretora.',
    bioEn: '', images: 14, ids: { imdb: 'nm3920114', instagram: 'maralindqvist', wikidata: 'Q9876543' },
    family: [{ rel: 'Cônjuge', name: 'J. Ostrow', note: 'roteirista, parceiro em Cold Index e Paper Tides' }],
    awards: [
      { year: 2024, name: 'Guldbagge de atuação', for: 'The Quiet Harbor', won: true },
      { year: 2024, name: 'Prêmio do Cinema Europeu (indicação)', for: 'The Quiet Harbor', won: false },
      { year: 2022, name: 'Festival de Gotemburgo · roteiro', for: 'Paper Tides', won: true },
      { year: 2017, name: 'Festival de Gotemburgo · curta', for: 'Glass Orchard', won: true },
      { year: 2016, name: 'Guldbagge de coadjuvante (indicação)', for: 'Red Meridian', won: false },
      { year: 2012, name: 'CPH:DOX · estreante (indicação)', for: 'Still Water', won: false },
    ],
    rows, seenByTitle: seen,
  };
}

function theo() {
  return {
    id: 'theo', tmdb: 4410923, name: 'Theo Brandt', short: 'Theo', dept: 'Acting', initials: 'TB', hue: 160,
    born: '2001-07-19', died: '', place: 'Hamburgo, Alemanha', aka: ['Theodor Brandt'],
    bio: '', bioEn: 'Theo Brandt is a German actor. Northern Signal (2025) is his first feature film.',
    images: 1, ids: { imdb: 'nm1440921', instagram: '', wikidata: '' }, family: [], awards: [],
    rows: [row({ title: 'Northern Signal', year: 2025, genre: 'Ficção científica', character: 'Kai', order: 6, votes: 40, with: ['Lena Voss'] })],
    seenByTitle: {},
  };
}

function rosa() {
  const rows = [
    row({ title: 'Harbor Patrol', year: 1978, kind: 'tv', character: 'Girl on pier', eps: 1, votes: 50 }),
    row({ title: 'Blue Station', year: 1981, character: 'Passenger', order: 8, votes: 300 }),
    row({ title: 'The Copper Road', year: 1989, character: 'Adaeze', order: 0, votes: 4100, with: ['Iris Okon'] }),
    row({ title: 'The Lantern Fox', year: 1998, genre: 'Animação', character: 'Fox (voice)', order: 0, votes: 22000, with: ['Mateo Ruiz', 'Iris Okon'] }),
    row({ title: 'Small Choir', year: 2019, dept: 'Directing', genre: 'Documentário', votes: 600 }),
    row({ title: 'Long Summer', year: 2023, dept: 'Directing', votes: 1300, with: ['Iris Okon'] }),
    row({ title: 'Long Summer', year: 2023, dept: 'Writing', votes: 1300 }),
  ];
  const used = new Set(rows.map(c => c.title));
  const r = rand(23);
  const tv = (genre, a, b, eps, character) => r => ({ kind: 'tv', genre, year: year(r, a, b), eps: eps(r), character: character(r), order: 2 + Math.floor(r() * 10), votes: 20 + Math.floor(r() * 1500), with: r() < 0.1 ? [pick(r, PEOPLE)] : [] });
  rows.push(
    ...fillers(r, 80, used, tv('Drama', 1978, 2025, r => 1 + Math.floor(r() * 2), r => pick(r, NAMES))),
    ...fillers(r, 60, used, tv('Talk', 1985, 2026, () => 1, () => 'Self')),
    ...fillers(r, 90, used, tv('Animação', 1984, 2026, r => 3 + Math.floor(r() * 120), r => `${pick(r, NAMES)} (voice)`)),
    ...fillers(r, 30, used, tv('Policial', 1986, 2024, r => 6 + Math.floor(r() * 50), r => pick(r, NAMES))),
    ...fillers(r, 56, used, r => ({ year: year(r, 1982, 2025), genre: pick(r, ['Drama', 'Comédia', 'Animação', 'Romance']), character: pick(r, NAMES), order: 2 + Math.floor(r() * 10), votes: 80 + Math.floor(r() * 7000), with: r() < 0.15 ? [pick(r, PEOPLE)] : [] })),
  );
  const seen = { 'The Lantern Fox': { state: 'seen', src: 'trakt', at: '2019-12-24' }, 'The Copper Road': { state: 'seen', src: 'simkl', at: '2021-03-03' } };
  const core = rows.filter(c => !NOISE_GENRES.includes(c.genre) && c.eps !== 1 && c.eps !== 2 && !c.upcoming);
  shuffle(rand(5), core).slice(0, 24).forEach((c, i) => {
    seen[c.title] ??= i % 9 === 4 ? { state: 'progress', src: 'nuvio', pct: 30 + i } : { state: 'seen', src: ['trakt', 'simkl', 'nuvio'][i % 3], at: `20${10 + i % 15}-05-0${1 + i % 9}` };
  });
  return {
    id: 'rosa', tmdb: 30512, name: 'Rosa Okafor', short: 'Rosa', dept: 'Acting', initials: 'RO', hue: 25,
    born: '1961-11-08', died: '', place: 'Lagos, Nigéria', aka: ['Rosa N. Okafor', 'Rosie Okafor'],
    bio: 'Rosa Okafor começou como figurante em séries britânicas no fim dos anos 1970 e virou uma das vozes mais reconhecidas da animação: deu voz à raposa de The Lantern Fox (1998) e a dezenas de personagens em séries infantis. Na tela, o primeiro papel principal veio com The Copper Road (1989). Desde 2019 também dirige; Long Summer (2023) é o primeiro roteiro.',
    bioEn: '', images: 41, ids: { imdb: 'nm0644012', instagram: 'rosaokafor', wikidata: 'Q1234567' },
    family: [{ rel: 'Filha', name: 'Ama Okafor', note: 'atriz' }],
    awards: [
      { year: 1999, name: 'Annie · voz', for: 'The Lantern Fox', won: true },
      { year: 1990, name: 'BAFTA revelação (indicação)', for: 'The Copper Road', won: false },
      { year: 2023, name: 'Festival de Durban · roteiro (indicação)', for: 'Long Summer', won: false },
    ],
    rows, seenByTitle: seen,
  };
}

export const PERSONS = Object.fromEntries([mara(), theo(), rosa()].map(p => {
  p.seen = {};
  for (const c of p.rows) {
    const s = p.seenByTitle[c.title];
    // Series history is per episode: keep how many of the person's episodes were watched.
    if (s) p.seen[c.id] = c.kind === 'tv' ? { src: s.src, at: s.at, eps: s.state === 'seen' ? c.eps : Math.round(c.eps * s.pct / 100) } : s;
  }
  return [p.id, p];
}));

// TMDB returns cast and crew rows separately; one title can show up as actor and writer.
export function mergeCredits(rows) {
  const by = new Map();
  for (const c of rows) {
    const e = by.get(c.id) || { ...c, roles: [], characters: [], eps: 0, order: 99, with: [] };
    if (!e.roles.includes(c.dept)) e.roles.push(c.dept);
    if (c.character && !e.characters.includes(c.character)) e.characters.push(c.character);
    // ponytail: max of the counts; the app unions the real episode lists of cast and crew.
    e.eps = Math.max(e.eps, c.eps);
    if (c.dept === 'Acting') e.order = Math.min(e.order, c.order);
    e.with = [...new Set([...e.with, ...c.with])];
    e.votes = Math.max(e.votes, c.votes);
    by.set(c.id, e);
  }
  return [...by.values()].map(e => ({ ...e, character: e.characters.join(' / ') }));
}

const SELF = /\b(self|himself|herself|themselves)\b|\(uncredited\)|\(archive/i;
export const isVoice = e => /\((voice|voz)\)/i.test(e.character);
export function noiseReason(e) {
  if (NOISE_GENRES.includes(e.genre) || SELF.test(e.character)) return 'Aparição como si';
  if (e.kind === 'tv' && e.roles.length === 1 && e.roles[0] === 'Acting' && e.eps > 0 && e.eps <= 2) return 'Participação especial';
  return '';
}
export const prepare = p => mergeCredits(p.rows).map(e => ({ ...e, noise: noiseReason(e) }));
export const charLabel = e => e.character.replace(/\s*\((voice|voz)\)/gi, '');

export function roleLabel(e) {
  const parts = [];
  if (e.roles.includes('Directing')) parts.push('Direção');
  if (e.roles.includes('Writing')) parts.push('Roteiro');
  if (e.roles.includes('Acting')) parts.push(e.noise ? (e.noise === 'Participação especial' ? 'Participação' : 'Como si') : isVoice(e) ? 'Voz' : e.order <= 1 ? 'Protagonista' : 'Elenco');
  return parts.join(' · ');
}

export function roleLine(entries) {
  const n = r => entries.filter(e => e.roles.includes(r)).length;
  return [['Acting', 'Atuação'], ['Directing', 'Direção'], ['Writing', 'Roteiro']]
    .map(([r, l]) => ({ role: r, label: l, n: n(r) })).filter(x => x.n).sort((a, b) => b.n - a.n);
}

// Each title gets at most one milestone; Theo's single film is just "debut".
export function milestones(list) {
  if (!list.length) return [];
  const asc = [...list].sort((a, b) => a.year - b.year || b.votes - a.votes);
  const out = [];
  const add = (type, e) => { if (e && !out.some(m => m.id === e.id)) out.push({ type, id: e.id, year: e.year, title: e.title }); };
  const find = f => asc.find(f);
  add(asc[0].kind === 'movie' ? 'debut' : 'first', asc[0]);
  add('debut', find(e => e.kind === 'movie'));
  add('firstLead', find(e => e.roles.includes('Acting') && e.order <= 1 && !e.noise));
  add('firstDirecting', find(e => e.roles.includes('Directing')));
  add('firstWriting', find(e => e.roles.includes('Writing')));
  if (list.length >= 3) add('breakout', [...list].sort((a, b) => b.votes - a.votes)[0]);
  if (list.length >= 2) add('latest', asc.at(-1));
  return out.sort((a, b) => a.year - b.year);
}

export const MILESTONE = { first: 'Primeira aparição', debut: 'Estreia no cinema', firstLead: 'Primeiro papel principal', firstDirecting: 'Estreia na direção', firstWriting: 'Primeiro roteiro', breakout: 'Mais votado', latest: 'Mais recente' };
const CHAPTER = { first: 'Primeiros passos', debut: 'Estreia no cinema', firstLead: 'Protagonista', firstDirecting: 'Atrás da câmera', firstWriting: 'Também no roteiro', breakout: 'O papel que marcou', now: 'Agora' };
const PRIORITY = ['first', 'debut', 'breakout', 'firstDirecting', 'now', 'firstLead', 'firstWriting'];
export const MAX_CHAPTERS = 6;

export function chapters(list, ms) {
  const years = [...new Set(list.map(e => e.year))].sort((a, b) => a - b);
  if (!years.length) return [];
  const min = years[0], max = years.at(-1);
  if (list.length === 1) return [{ type: ms[0]?.type || 'debut', title: 'Estreia', from: min, to: max }];
  const cand = ms.filter(m => m.type !== 'latest').map(m => ({ type: m.type, year: m.year }));
  if (max - min >= 8) cand.push({ type: 'now', year: max - 2 });
  cand.sort((a, b) => PRIORITY.indexOf(a.type) - PRIORITY.indexOf(b.type));
  const starts = new Map();
  for (const c of cand) if (!starts.has(c.year) && starts.size < MAX_CHAPTERS) starts.set(c.year, c.type);
  const sorted = [...starts].sort((a, b) => a[0] - b[0]);
  return sorted.map(([from, type], i) => ({ type, title: CHAPTER[type], from, to: i + 1 < sorted.length ? sorted[i + 1][0] - 1 : max }));
}

export function timeline(list) {
  const by = new Map();
  for (const e of list) by.set(e.year, [...(by.get(e.year) || []), e]);
  return [...by].sort((a, b) => a[0] - b[0]).map(([year, items]) => ({ year, items: items.sort((a, b) => b.votes - a.votes) }));
}

export const fmtK = n => n >= 1000 ? `${(n / 1000).toFixed(1).replace('.0', '').replace('.', ',')} mil` : String(n);
const as = e => e.roles.includes('Acting') && e.character ? (isVoice(e) ? `, na voz de ${charLabel(e)}` : `, como ${charLabel(e)}`) : e.roles.includes('Directing') ? ' (direção)' : '';
const STORY = {
  first: e => `Primeira aparição na tela: ${e.title}${as(e)}.`,
  debut: e => `Estreia no cinema com ${e.title}${as(e)}.`,
  firstLead: e => `Primeiro papel principal: ${charLabel(e)} em ${e.title}.`,
  firstDirecting: e => `Estreia na direção com ${e.title}.`,
  firstWriting: e => `Assina o primeiro roteiro: ${e.title}.`,
  breakout: e => `${e.title} vira o trabalho mais votado da carreira (${fmtK(e.votes)} votos no TMDB).`,
  latest: e => e.upcoming ? `Próximo lançamento: ${e.title}.` : `O trabalho mais recente: ${e.title}.`,
};
export function yearStory(year, items, ms, awards = []) {
  const lines = ms.filter(m => m.year === year).map(m => STORY[m.type](items.find(i => i.id === m.id)));
  if (!lines.length) lines.push(items.length === 1 ? `${items[0].title}${as(items[0])}.` : `${items.length} trabalhos no ano; o mais votado é ${items[0].title}.`);
  for (const a of awards.filter(a => a.year === year)) lines.push(`${a.won ? 'Prêmio' : 'Indicação'}: ${a.name.replace(' (indicação)', '')}, por ${a.for}.`);
  return lines.join(' ');
}

export function collaborators(list) {
  const n = new Map();
  for (const e of list) for (const who of e.with) n.set(who, [...(n.get(who) || []), e.title]);
  return [...n].filter(([, t]) => t.length >= 2).sort((a, b) => b[1].length - a[1].length).slice(0, 5).map(([name, titles]) => ({ name, n: titles.length, titles }));
}

// ---- watched state: Nuvio progress, Trakt and Simkl history, then manual "Já vi".
export const emptyMarks = () => ({ manual: {}, want: {}, fails: {} });
// A series is seen when every episode the person is in was watched: in 1 episode, that 1 is enough.
export function seriesState(mine, watched) {
  if (watched <= 0) return { state: 'none' };
  return { state: watched >= mine ? 'seen' : 'progress', pct: Math.min(100, Math.round(watched / mine * 100)), epsSeen: Math.min(watched, mine), epsIn: mine };
}
export function statusOf(e, seen, marks) {
  if (e.upcoming) return { state: 'upcoming' };
  const s = seen[e.id], m = marks.manual[e.id];
  const auto = s && (e.kind === 'tv' && s.eps != null ? { ...s, ...seriesState(e.eps, s.eps) } : s);
  // A manual mark covers what the history lacks: for a series, the person's remaining episodes.
  if (m && auto?.state !== 'seen') return { state: 'seen', src: 'manual', how: m.how, watched: m.watched, date: m.date };
  if (auto && auto.state !== 'none') return auto;
  return { state: 'none', want: !!marks.want[e.id] };
}
// Achievements take history and quiz-passed marks; a mark without the quiz only shows as seen.
export const counts = s => s.state === 'seen' && (s.src !== 'manual' || s.how === 'quiz');

export function summary(core, st) {
  const roles = {};
  let seen = 0, progress = 0, total = 0;
  for (const e of core) {
    if (e.upcoming) continue;
    const s = st(e).state;
    total++;
    if (s === 'seen') seen++; else if (s === 'progress') progress++;
    for (const r of e.roles) { roles[r] ??= { seen: 0, total: 0 }; roles[r].total++; if (s === 'seen') roles[r].seen++; }
  }
  return { seen, progress, total, roles };
}

export const LEVEL_NAMES = ['Ainda não', 'Primeiro passo', 'Meio caminho', 'Reta final', 'Filmografia completa', 'Em ordem'];
// Same rule as cqregra_nivel (src/conquistasregra.c).
export function level(vistos, total, emOrdem) {
  if (total <= 0 || vistos <= 0) return 0;
  if (vistos >= total) return emOrdem ? 5 : 4;
  if (vistos * 4 >= total * 3) return 3;
  if (vistos * 2 >= total) return 2;
  return 1;
}
export const needFor = (lvl, total) => [0, 1, Math.ceil(total / 2), Math.ceil(total * 3 / 4), total, total][lvl] ?? total;

// Trails reuse the existing achievement keys (ator:/dir:); the extra ones only add a "/suffix",
// which the server regex already accepts. Quiz-passed marks count for levels 1-4, never for
// "Em ordem": the date sent to Trakt/Simkl is today's or the release date, not when it was watched.
export function achievements(p, core, st) {
  const released = core.filter(e => !e.upcoming);
  const seenList = list => list.filter(e => counts(st(e)));
  const out = [];
  for (const [role, pfx, label, min] of [['Acting', 'ator', 'Atuação', 1], ['Directing', 'dir', 'Direção', 3]]) {
    const list = released.filter(e => e.roles.includes(role));
    if (list.length < min) continue;
    const s = seenList(list);
    const dated = s.map(e => st(e)).every(x => x.at);
    const byWatch = [...s].sort((a, b) => st(a).at < st(b).at ? -1 : 1).map(e => e.year);
    const inOrder = s.length === list.length && dated && byWatch.every((y, i) => !i || y >= byWatch[i - 1]);
    out.push({ key: `${pfx}:${p.tmdb}`, kind: 'trail', name: `Filmografia · ${label}`, seen: s.length, total: list.length, level: level(s.length, list.length, inOrder) });
  }
  if (released.length >= 5) {
    const n = seenList(released).length;
    out.push({ key: `ator:${p.tmdb}/cinco`, kind: 'goal', name: `Cinco de ${p.short}`, desc: `Ver 5 títulos de ${p.name}.`, seen: Math.min(n, 5), total: 5, level: n >= 5 ? 1 : 0 });
  }
  const asc = [...released].sort((a, b) => a.year - b.year);
  if (asc.length >= 2) {
    const ends = [asc[0], asc.at(-1)];
    const n = seenList(ends).length;
    out.push({ key: `ator:${p.tmdb}/ponta`, kind: 'goal', name: 'Da estreia ao último', desc: `Ver ${ends[0].title} (${ends[0].year}) e ${ends[1].title} (${ends[1].year}).`, seen: n, total: 2, level: n === 2 ? 1 : 0 });
  }
  return out;
}
export const unlocks = (before, after) => after.filter(a => a.level > (before.find(b => b.key === a.key)?.level ?? 0));

export function nextToWatch(core, st, prefer, trails = []) {
  const prog = core.find(e => st(e).state === 'progress');
  if (prog) { const s = st(prog); return { e: prog, reason: s.epsIn ? `Continuar · ${s.epsSeen} de ${s.epsIn} episódios` : `Continuar · ${s.pct}% visto` }; }
  const cand = core.filter(e => st(e).state === 'none')
    .sort((a, b) => (b.roles.includes(prefer) - a.roles.includes(prefer)) || b.votes - a.votes);
  if (!cand.length) return null;
  for (const t of trails.filter(t => t.kind === 'trail' && t.level < 4)) {
    const missing = needFor(t.level + 1, t.total) - t.seen;
    const role = t.key.startsWith('dir:') ? 'Directing' : 'Acting';
    const e = missing === 1 && cand.find(c => c.roles.includes(role));
    if (e) return { e, reason: `Falta 1 para “${LEVEL_NAMES[level(t.seen + 1, t.total, 0)]}”` };
  }
  return { e: cand[0], reason: 'O mais votado que falta' };
}

// ---- grid filters
export const FILTERS = {
  sort: [['new', 'Mais recentes'], ['old', 'Mais antigos'], ['votes', 'Mais votados']],
  type: [['all', 'Todos'], ['movie', 'Filmes'], ['tv', 'Séries']],
  role: [['all', 'Todas'], ['Acting', 'Atuação'], ['Directing', 'Direção'], ['Writing', 'Roteiro'], ['voice', 'Voz']],
  status: [['all', 'Todos'], ['seen', 'Vistos'], ['none', 'Não vistos']],
};
export const DEFAULT_FILTER = { sort: 'new', type: 'all', role: 'all', status: 'all', year: null };
export function cycle(f, name) {
  const o = FILTERS[name], i = o.findIndex(x => x[0] === f[name]);
  return { ...f, [name]: o[(i + 1) % o.length][0] };
}
export function filterGrid(list, st, f) {
  const out = list.filter(e => (f.type === 'all' || e.kind === f.type)
    && (f.role === 'all' || (f.role === 'voice' ? isVoice(e) : e.roles.includes(f.role)))
    && (f.status === 'all' || (f.status === 'seen' ? st(e).state === 'seen' : ['none', 'progress'].includes(st(e).state)))
    && (f.year == null || e.year === f.year));
  const by = { new: (a, b) => b.year - a.year || b.votes - a.votes, old: (a, b) => a.year - b.year || b.votes - a.votes, votes: (a, b) => b.votes - a.votes };
  return out.sort(by[f.sort]);
}

// ---- quiz: a light check from TMDB data the app already fetches (credits, keywords, images).
export const QUIZ = { tries: 2, windowMs: 864e5 };
// Spoiler blocklist, applied before any TMDB keyword or image reaches a question. Terms catch the
// generic giveaways (keywords come in English and Portuguese); e.spoilers and e.stillBlocked are the
// per-title entries. ponytail: a list our server ships and grows by report; no classifier.
export const SPOILER_TERMS = /\b(twist|ending|final|reviravolta|morte|morre|death|dies|dead|murder|assassin|killer|culpad|suic|trai[çc])/i;
export const spoilerFree = (e, k) => !SPOILER_TERMS.test(k) && !e.spoilers.includes(k);
export function buildQuiz(e, p, seed = 1) {
  const r = rand(hash(e.id) ^ seed);
  const q = (type, prompt, right, wrong, extra = {}) => {
    const opts = shuffle(r, [right, ...wrong.slice(0, 3)]);
    return { type, prompt, options: opts, answer: opts.indexOf(right), ...extra };
  };
  const make = [];
  // Scenes are series/movie backdrops only (promo art), never episode stills, and not when blocked.
  if (!e.stillBlocked) make.push(() => {
    const hues = [0, 1, 2, 3].map(i => i ? (e.hue + 70 * i + Math.floor(r() * 20)) % 360 : e.hue);
    return q('still', `Qual destas cenas é de ${e.title}?`, hues[0], hues.slice(1));
  });
  if (e.roles.includes('Acting') && e.character && !e.noise) {
    const right = charLabel(e).split(' / ')[0];
    make.push(() => q('character', isVoice(e) ? `A quem ${p.short} dá voz em ${e.title}?` : `Quem ${p.short} interpreta em ${e.title}?`, right, shuffle(r, DECOY_NAMES.filter(n => n !== right))));
  }
  for (const k of e.keywords.filter(k => spoilerFree(e, k))) {
    const other = Object.entries(GENRES).filter(([g]) => g !== e.genre && !NOISE_GENRES.includes(g)).flatMap(([, v]) => v)
      .filter(w => !e.keywords.includes(w) && spoilerFree(e, w));
    make.push(() => q('keyword', `Qual destes temas aparece em ${e.title}?`, k, shuffle(r, other)));
  }
  const co = e.with.filter(n => n !== p.name);
  if (co.length) make.push(() => q('costar', `Quem também está em ${e.title}?`, pick(r, co), shuffle(r, DECOY_PEOPLE)));
  const all = shuffle(r, make).map(f => f());
  // Never two questions of the same kind in the first pair when avoidable.
  const firstPair = [all[0], all.find(x => x.type !== all[0].type) || all[1]].filter(Boolean);
  return { questions: firstPair, reserve: all.filter(x => !firstPair.includes(x)) };
}

export const startQuiz = qz => ({ asked: [...qz.questions], reserve: [...qz.reserve], i: 0, answers: [], skipped: false, result: null });
export function answer(s, choice) {
  const correct = choice === s.asked[s.i].answer;
  const n = { ...s, asked: [...s.asked], reserve: [...s.reserve], answers: [...s.answers, correct], i: s.i + 1, last: correct };
  if (n.i < n.asked.length) return n;
  const ok = n.answers.filter(Boolean).length;
  if (n.asked.length === 2 && ok === 1 && n.reserve.length) { n.asked.push(n.reserve.shift()); n.bonus = true; return n; }
  n.result = ok >= 2 ? 'pass' : 'fail';
  return n;
}
export function skip(s) {
  if (s.skipped || !s.reserve.length || s.result) return s;
  const asked = [...s.asked], reserve = [...s.reserve];
  asked[s.i] = reserve.shift();
  return { ...s, asked, reserve, skipped: true };
}

export const triesLeft = (m, id, now) => Math.max(0, QUIZ.tries - (m.fails[id] || []).filter(t => now - t < QUIZ.windowMs).length);
export const withFail = (m, id, now) => ({ ...m, fails: { ...m.fails, [id]: [...(m.fails[id] || []), now] } });
// A manual mark always goes to Trakt and Simkl, so it carries a watched date: 'today', or
// 'release' when the person does not remember. how: 'quiz' (passed) or 'direct' (no questions).
export const watchedDate = (e, date) => date === 'release' && e.release < TODAY ? e.release : TODAY;
export function withSeen(m, e, now, date = 'today', how = 'quiz') {
  const want = { ...m.want }; delete want[e.id];
  return { ...m, want, manual: { ...m.manual, [e.id]: { at: now, how, date, watched: watchedDate(e, date) } } };
}
export function withoutSeen(m, id) { const manual = { ...m.manual }; delete manual[id]; return { ...m, manual }; }
export function toggleWant(m, id) { const want = { ...m.want }; if (want[id]) delete want[id]; else want[id] = true; return { ...m, want }; }

// ---- person facts
const MONTHS = ['jan.', 'fev.', 'mar.', 'abr.', 'mai.', 'jun.', 'jul.', 'ago.', 'set.', 'out.', 'nov.', 'dez.'];
export const fmtDate = iso => { const [y, m, d] = iso.split('-').map(Number); return `${d} ${MONTHS[m - 1]} ${y}`; };
export function age(born, until = TODAY) {
  const [y, m, d] = born.split('-').map(Number), [Y, M, D] = until.split('-').map(Number);
  return Y - y - (M < m || (M === m && D < d) ? 1 : 0);
}
export const lifeText = p => p.died ? `${fmtDate(p.born)} – ${fmtDate(p.died)} · ${age(p.born, p.died)} anos` : `Nascimento: ${fmtDate(p.born)} · ${age(p.born)} anos`;

// Where each field comes from (D key in the prototype). hoje = app + TMDB/Trakt/Simkl today; nosso = needs our server; bloq = no source.
export const SOURCES = {
  hoje: 'Hoje: TMDB /person + combined_credits, external_ids, images; vistos de Nuvio, Trakt e Simkl',
  nosso: 'Nosso servidor, na 2.1: Wikidata (prêmios, família), seguir pessoa, lista de bloqueio de spoiler',
  bloq: 'Sem fonte aberta: prêmios do IMDb, bilheteria por pessoa (só raspando)',
};
