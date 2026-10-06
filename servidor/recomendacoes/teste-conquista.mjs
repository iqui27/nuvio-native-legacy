import { filmesDosCreditos, classe } from "./src/conquista.js";
import assert from "node:assert";
const c = { crew: [{id:1,title:"A",job:"Director",release_date:"2010-01-01",genre_ids:[18]},{id:1,title:"A",job:"Writer",release_date:"2010-01-01"},{id:2,title:"F",job:"Director",release_date:"2999-01-01"},{id:3,title:"T",job:"Director",release_date:"2005-01-01",genre_ids:[10770]}],
  cast: [{id:9,title:"Doc",character:"Himself",release_date:"2001-01-01"},{id:8,title:"X",character:"Bob",release_date:"2001-01-01"}] };
const d = filmesDosCreditos(c, "dir", 20261006);
assert.deepEqual(d.map(f=>f.tmdb), [3,1]); assert.equal(d[0].classe, 3);
assert.deepEqual(filmesDosCreditos(c,"ator",20261006).map(f=>f.tmdb),[8]);
assert.equal(classe([99],0),1); assert.equal(classe([18],30),2);
console.log("conquista.js ok");
