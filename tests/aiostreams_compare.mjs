// Produce mock | app PNGs; native captures must already exist.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import sharp from '/Users/hrocha/node_modules/sharp/lib/index.js';
const out=process.argv[2]||'/Volumes/ExternalSSD/tmp/rev/aiostreams-png';
const mock='/Volumes/ExternalSSD/tmp/aiostreams-mock';
const files=fs.readdirSync(mock).filter(f=>/^aio-.*\.png$/.test(f));
assert.equal(files.length,15);
for(const file of files) {
  const app=path.join(out,file),reference=path.join(mock,file);
  assert(fs.existsSync(app),file+': native capture missing');
  const meta=await sharp(app).metadata(); assert.equal(meta.width,1920);assert.equal(meta.height,1080);
  const stats=await sharp(app).stats(); assert(stats.channels.some(c=>c.stdev>15),file+': blank capture');
  const target=path.join(out,file.replace('.png','-compare.png'));
  await sharp({create:{width:3840,height:1080,channels:3,background:'#0d1115'}})
    .composite([{input:reference,left:0,top:0},{input:app,left:1920,top:0}]).png().toFile(target);
  console.log(target);
}
