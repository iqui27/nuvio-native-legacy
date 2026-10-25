// Reuse the mock's deterministic person and history for native visual proof.
import * as M from '../docs/prototypes/filmografia/model.mjs';
const p = M.PERSONS.mara;
const entries = M.prepare(p);
const credits = entries.map((e, i) => ({
  id: i + 1, title: e.title, name: e.title, media_type: e.kind,
  release_date: e.release, first_air_date: e.release, order: e.order,
  character: e.character, vote_count: e.votes,
}));
const crew = entries.flatMap((e, i) => e.roles.filter(r => r !== 'Acting').map(r => ({
  ...credits[i], job: r === 'Directing' ? 'Director' : 'Screenplay',
})));
const person = { name: p.name, known_for_department: p.dept, birthday: p.born,
  place_of_birth: p.place, biography: p.bio, also_known_as: p.aka,
  combined_credits: { cast: credits.filter((_, i) => entries[i].roles.includes('Acting')), crew } };
console.log(JSON.stringify({ ...person, entries: entries.map((e, i) => ({
  ...M.statusOf(e, p.seen, M.emptyMarks()),
  id: i + 1, noise: e.noise ? 1 : 0, eps: e.eps, epsTotal: e.epsTotal,
})), awards: p.awards }));
