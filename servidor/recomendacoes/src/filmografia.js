// Same authenticated account/profile identity as the other personal routes.
const positive = x => /^\d+$/.test(String(x)) && Number.isSafeInteger(Number(x)) && Number(x)>0;
async function wikidata(id, language, fetchFn) {
  const response=await fetchFn(`https://www.wikidata.org/w/api.php?action=wbgetentities&ids=${id}&languages=${language}|en&format=json`);
  if(!response.ok) throw new Error('wikidata');
  return (await response.json()).entities || {};
}
export async function rotaFilmografiaPessoal(rota, metodo, env, quem, corpo, url, h, fetchFn) {
  if(rota==='/v1/filmografia/seguir' && metodo==='POST') {
    if(!positive(corpo?.pessoa) || ![0,1].includes(corpo?.seguir)) return h.erro('invalid follow',400);
    await env.DB.prepare('INSERT INTO pessoa_seguida (dono, tmdb, seguindo) VALUES (?, ?, ?) ON CONFLICT(dono, tmdb) DO UPDATE SET seguindo=excluded.seguindo').bind(quem.id,Number(corpo.pessoa),corpo.seguir).run();
    return h.json({ok:1,seguindo:corpo.seguir});
  }
  if(rota==='/v1/filmografia/quiz' && metodo==='GET') {
    if(!positive(url.searchParams.get('tmdb')) || !['movie','tv'].includes(url.searchParams.get('tipo'))) return h.erro('invalid title',400);
    // Editorial additions are versioned; the client also applies its spoiler filter.
    const key=`${url.searchParams.get('tipo')}:${url.searchParams.get('tmdb')}`;
    return h.json({blocked:env.FILMO_SPOILER_BLOCKED?.[key] || []});
  }
  if(rota!=='/v1/filmografia/pessoa' || metodo!=='GET') return null;
  const id=url.searchParams.get('id');
  if(!positive(id)) return h.erro('invalid person',400);
  if(!env.TMDB_KEY) return h.erro('missing tmdb key',503);
  const language=/^[a-z]{2}$/.test(url.searchParams.get('language') || '')?url.searchParams.get('language'):'en';
  try {
    const response=await fetchFn(`https://api.themoviedb.org/3/person/${id}/external_ids?api_key=${env.TMDB_KEY}`);
    if(!response.ok) return h.erro('tmdb unavailable',502);
    const {wikidata_id:q}=await response.json();
    const follow=await env.DB.prepare('SELECT seguindo FROM pessoa_seguida WHERE dono=? AND tmdb=?').bind(quem.id,Number(id)).first();
    const out={seguindo:follow?.seguindo || 0,familia:[],premios:[]};
    if(/^Q\d+$/.test(q || '')) {
      const entities=await wikidata(q,language,fetchFn), claims=entities[q]?.claims || {};
      const rows=[];
      for(const [property,kind] of [['P26','familia'],['P22','familia'],['P25','familia'],['P40','familia'],['P3373','familia'],['P166','premios']])
        for(const claim of (claims[property] || []).slice(0,12)) {
          const target=claim.mainsnak?.datavalue?.value?.id;
          if(/^Q\d+$/.test(target || '')) rows.push({id:target,kind,claim});
        }
      const ids=[...new Set(rows.map(x=>x.id))].slice(0,48);
      const names=ids.length?await wikidata(ids.join('|'),language,fetchFn):{};
      for(const row of rows) {
        const nome=names[row.id]?.labels?.[language]?.value || names[row.id]?.labels?.en?.value;
        if(nome && out[row.kind].length<12) out[row.kind].push({nome,rel:'',ano:Number((row.claim.qualifiers?.P585?.[0]?.datavalue?.value?.time || '').slice(1,5)) || 0});
      }
    }
    return h.json(out);
  } catch { return h.erro('person data unavailable',502); }
}
