import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from './model.mjs';

const load = id => {
  const p = M.PERSONS[id], all = M.prepare(p), core = all.filter(e => !e.noise);
  return { p, all, core, st: (e, marks = M.emptyMarks()) => M.statusOf(e, p.seen, marks) };
};

test('cast and crew rows of one title merge into one entry with both roles', () => {
  const { all } = load('mara');
  const paper = all.filter(e => e.title === 'Paper Tides');
  assert.equal(paper.length, 1);
  assert.deepEqual(paper[0].roles.sort(), ['Acting', 'Writing']);
  assert.equal(M.roleLabel(paper[0]), 'Roteiro · Protagonista');
});

test('talk shows, "Self" and 1-2 episode guest spots are noise; voice work is not', () => {
  const { all } = load('rosa');
  assert.equal(M.noiseReason(all.find(e => e.genre === 'Talk')), 'Aparição como si');
  assert.equal(M.noiseReason(all.find(e => e.title === 'Harbor Patrol')), 'Participação especial');
  const voice = all.find(e => e.title === 'The Lantern Fox');
  assert.equal(voice.noise, '');
  assert.equal(M.roleLabel(voice), 'Voz');
  assert.ok(all.length > 300 && all.filter(e => !e.noise).length < 200);
});

test('milestones: one per title, ordered by year; noise changes the first appearance', () => {
  const { all, core } = load('mara');
  const ms = M.milestones(core);
  assert.deepEqual(ms.map(m => m.type), ['debut', 'firstDirecting', 'firstWriting', 'firstLead', 'breakout', 'latest']);
  assert.equal(new Set(ms.map(m => m.id)).size, ms.length);
  assert.equal(ms.find(m => m.type === 'breakout').title, 'The Quiet Harbor');
  const withNoise = M.milestones(all);
  assert.equal(withNoise[0].type, 'first');
  assert.equal(withNoise[0].title, 'Harbor Lights');
  assert.equal(withNoise[1].title, 'Low Tide');
});

test('chapters cover the whole career without gaps and never exceed the cap', () => {
  for (const id of ['mara', 'rosa']) {
    const { core } = load(id);
    const ch = M.chapters(core, M.milestones(core));
    const years = core.map(e => e.year);
    assert.ok(ch.length >= 3 && ch.length <= M.MAX_CHAPTERS);
    assert.equal(ch[0].from, Math.min(...years));
    assert.equal(ch.at(-1).to, Math.max(...years));
    ch.forEach((c, i) => i && assert.equal(c.from, ch[i - 1].to + 1));
  }
});

test('one credit: one chapter, one year, debut story, ring 0 of 1', () => {
  const { p, core, st } = load('theo');
  const ms = M.milestones(core);
  assert.deepEqual(ms.map(m => m.type), ['debut']);
  assert.deepEqual(M.chapters(core, ms).map(c => c.title), ['Estreia']);
  const [y] = M.timeline(core);
  assert.equal(M.yearStory(y.year, y.items, ms), 'Estreia no cinema com Northern Signal, como Kai.');
  assert.deepEqual(M.summary(core, st), { seen: 0, progress: 0, total: 1, roles: { Acting: { seen: 0, total: 1 } } });
  assert.equal(p.bio, '');
});

test('year story mentions milestones and awards of that year', () => {
  const { p, core } = load('mara');
  const ms = M.milestones(core);
  const y = M.timeline(core).find(t => t.year === 2024);
  const s = M.yearStory(2024, y.items, ms, p.awards);
  assert.match(s, /The Quiet Harbor vira o trabalho mais votado/);
  assert.match(s, /Prêmio: Guldbagge de atuação, por The Quiet Harbor/);
});

test('summary ignores upcoming titles and counts each role a title has', () => {
  const { core, st } = load('mara');
  const s = M.summary(core, st);
  assert.equal(s.total, core.length - 1);
  assert.equal(M.statusOf(core.find(e => e.title === 'Salt Bridge'), {}, M.emptyMarks()).state, 'upcoming');
  assert.equal(s.roles.Directing.total, 3);
  assert.equal(s.progress, 1);
});

test('next to watch: in-progress first, then the title that closes a level', () => {
  const { p, core, st } = load('mara');
  const ach = M.achievements(p, core, st);
  assert.equal(M.nextToWatch(core, st, 'Acting', ach).e.title, 'Gray Season');
  const done = e => (e.title === 'Gray Season' ? { state: 'seen', src: 'nuvio', at: '2026-01-01' } : st(e));
  const n = M.nextToWatch(core, done, 'Acting', M.achievements(p, core, done));
  assert.equal(n.reason, 'Falta 1 para “Filmografia completa”');
  assert.equal(n.e.title, 'Still Water');
});

test('levels follow cqregra_nivel and the manual mark unlocks without "Em ordem"', () => {
  assert.deepEqual([[0, 4], [1, 4], [2, 4], [3, 4], [4, 4]].map(([v, t]) => M.level(v, t, 0)), [0, 1, 2, 3, 4]);
  assert.equal(M.level(4, 4, 1), 5);
  const { p, core, st } = load('mara');
  const still = core.find(e => e.title === 'Still Water');
  const before = M.achievements(p, core, st);
  const marks = M.withSeen(M.emptyMarks(), still, 1000);
  const after = M.achievements(p, core, e => st(e, marks));
  const got = M.unlocks(before, after);
  assert.deepEqual(got.map(a => [a.key, a.level]), [['dir:1204418', 4]]);
  // Marked without the quiz: seen in "Você já viu", worth nothing in achievements.
  const direct = M.withSeen(M.emptyMarks(), still, 1000, 'today', 'direct');
  assert.equal(st(still, direct).state, 'seen');
  assert.equal(M.summary(core, e => st(e, direct)).seen, M.summary(core, st).seen + 1);
  assert.deepEqual(M.unlocks(before, M.achievements(p, core, e => st(e, direct))), []);
  assert.ok(after.every(a => /^(dir|ator):[A-Za-z0-9._/-]{1,80}$/.test(a.key)));
});

test('quiz: spoiler-free, plausible options, correct answer present once', () => {
  const { p, all } = load('mara');
  const harbor = all.find(e => e.title === 'The Quiet Harbor');
  for (let seed = 1; seed < 30; seed++) {
    const qz = M.buildQuiz(harbor, p, seed);
    assert.equal(qz.questions.length, 2);
    assert.notEqual(qz.questions[0].type, qz.questions[1].type);
    for (const q of [...qz.questions, ...qz.reserve]) {
      assert.equal(q.options.length, 4);
      assert.equal(new Set(q.options).size, 4);
      assert.ok(q.answer >= 0);
      assert.ok(!q.options.some(o => !M.spoilerFree(harbor, String(o))));
    }
  }
  // The raw TMDB keywords do carry spoilers; only the blocklist keeps them out.
  assert.deepEqual(harbor.keywords.filter(k => !M.spoilerFree(harbor, k)), ['o irmão está vivo', 'twist ending']);
  const kinds = e => new Set(Array.from({ length: 29 }, (_, i) => M.buildQuiz(e, p, i + 1)).flatMap(q => [...q.questions, ...q.reserve]).map(q => q.type));
  assert.ok(kinds(harbor).has('still'));
  assert.ok(!kinds(all.find(e => e.title === 'Night Ferry')).has('still'), 'blocked scene never asked');
  assert.notDeepEqual(M.buildQuiz(harbor, p, 1), M.buildQuiz(harbor, p, 2), 'retry asks something else');
  for (const e of all) {
    assert.ok(e.keywords.every(k => typeof k === 'string') && /-(0[1-9]|1\d|2[0-8])$/.test(e.release), e.title);
    for (const q of M.buildQuiz(e, p, 1).questions) assert.ok(q.options.every(o => o != null), e.title);
  }
});

test('quiz flow: 2/2 passes, 0/2 fails, 1/2 earns one tiebreak, skip once', () => {
  const { p, all } = load('mara');
  const qz = M.buildQuiz(all.find(e => e.title === 'Paper Tides'), p, 3);
  const right = s => M.answer(s, s.asked[s.i].answer);
  const wrong = s => M.answer(s, (s.asked[s.i].answer + 1) % 4);
  assert.equal(right(right(M.startQuiz(qz))).result, 'pass');
  assert.equal(wrong(wrong(M.startQuiz(qz))).result, 'fail');
  const tie = wrong(right(M.startQuiz(qz)));
  assert.equal(tie.result, null);
  assert.equal(tie.bonus, true);
  assert.equal(right(tie).result, 'pass');
  assert.equal(wrong(tie).result, 'fail');
  const s = M.skip(M.startQuiz(qz));
  assert.equal(s.skipped, true);
  assert.equal(M.skip(s), s, 'only one skip per attempt');
});

test('two failed tries per title per 24 h; a mark carries the date sent to Trakt/Simkl and clears "Quero ver"', () => {
  let m = M.emptyMarks();
  assert.equal(M.triesLeft(m, 'x', 0), 2);
  m = M.withFail(M.withFail(m, 'x', 0), 'x', 10);
  assert.equal(M.triesLeft(m, 'x', 100), 0);
  assert.equal(M.triesLeft(m, 'x', M.QUIZ.windowMs + 11), 2);
  const y = { id: 'y', release: '2019-03-08' };
  m = M.withSeen(M.toggleWant(m, 'y'), y, 5);
  assert.deepEqual([m.want.y, m.manual.y.watched, m.manual.y.date], [undefined, M.TODAY, 'today']);
  assert.deepEqual(M.statusOf(y, {}, m), { state: 'seen', src: 'manual', how: 'quiz', watched: M.TODAY, date: 'today' });
  // "Não lembro": the release date goes out, flagged, and never a date in the future.
  assert.deepEqual(M.statusOf(y, {}, M.withSeen(m, y, 6, 'release')).watched, '2019-03-08');
  assert.equal(M.watchedDate({ release: '2027-01-01' }, 'release'), M.TODAY);
  assert.equal(M.withoutSeen(m, 'y').manual.y, undefined);
});

test('a series is seen when every episode the person is in was watched', () => {
  assert.equal(M.seriesState(1, 1).state, 'seen');
  assert.equal(M.seriesState(1, 0).state, 'none');
  assert.deepEqual(M.seriesState(10, 6), { state: 'progress', pct: 60, epsSeen: 6, epsIn: 10 });
  const { core, st } = load('mara');
  const hollow = core.find(e => e.title === 'Hollow Kingdom'), gray = core.find(e => e.title === 'Gray Season');
  assert.deepEqual([hollow.eps, hollow.epsTotal, st(hollow).state], [8, 24, 'seen'], '8 of 24 episodes is the whole of her part');
  assert.deepEqual([st(gray).state, st(gray).epsSeen, st(gray).epsIn], ['progress', 6, 10]);
  // "Já vi" on a half-watched series covers her remaining episodes.
  assert.equal(st(gray, M.withSeen(M.emptyMarks(), gray, 1)).src, 'manual');
});

test('grid filters and sort', () => {
  const { all, st } = load('rosa');
  const voice = M.filterGrid(all, st, { ...M.DEFAULT_FILTER, role: 'voice' });
  assert.ok(voice.length > 80 && voice.every(M.isVoice));
  const old = M.filterGrid(all, st, { ...M.DEFAULT_FILTER, sort: 'old', type: 'movie' });
  assert.equal(old[0].title, 'Blue Station');
  assert.deepEqual(M.cycle(M.DEFAULT_FILTER, 'sort').sort, 'old');
  assert.equal(M.filterGrid(all, st, { ...M.DEFAULT_FILTER, year: 1998 }).every(e => e.year === 1998), true);
});

test('dates and age are gender-neutral and handle death', () => {
  assert.equal(M.lifeText(M.PERSONS.mara), 'Nascimento: 3 abr. 1982 · 44 anos');
  assert.equal(M.lifeText({ born: '1961-11-08', died: '2026-02-14' }), '8 nov. 1961 – 14 fev. 2026 · 64 anos');
});
