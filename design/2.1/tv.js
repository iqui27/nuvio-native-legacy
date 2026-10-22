// Escala cada TV 1920x1080 para caber na janela, injeta ícones e move o foco
// com as setas (vizinho mais próximo na direção, como o D-pad do app).
const IC={
home:'<path d="M3 10.5 12 3l9 7.5"/><path d="M5 9.5V21h14V9.5"/>',
explorar:'<circle cx="12" cy="12" r="9"/><path d="m15.5 8.5-2 5-5 2 2-5z"/>',
guia:'<rect x="3" y="5" width="18" height="13" rx="2"/><path d="M8 21h8"/>',
busca:'<circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/>',
biblio:'<path d="M5 4h4v16H5zM10 4h4v16h-4z"/><path d="m15 5 3.8-1 2.2 15.5-3.8 1z"/>',
agenda:'<rect x="3" y="5" width="18" height="16" rx="2"/><path d="M8 3v4M16 3v4M3 10h18"/>',
perfil:'<circle cx="12" cy="8" r="4"/><path d="M4 21c1.5-4 4.5-6 8-6s6.5 2 8 6"/>',
ajustes:'<path d="M4 6h9M17 6h3M4 12h3M11 12h9M4 18h12M20 18h0"/><circle cx="15" cy="6" r="2"/><circle cx="9" cy="12" r="2"/><circle cx="18" cy="18" r="2"/>',
play:'<path d="M7 4.5v15l12.5-7.5z" fill="currentColor"/>',
mais:'<path d="M12 5v14M5 12h14"/>',
ok:'<path d="m5 12.5 4.5 4.5L19 7"/>',
x:'<path d="M6 6l12 12M18 6 6 18"/>',
info:'<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>',
alerta:'<path d="M12 3.5 2.5 20h19z"/><path d="M12 10v4.5M12 17.2v.3"/>',
recarrega:'<path d="M20 12a8 8 0 1 1-2.3-5.7L20 8.5M20 3.5v5h-5"/>',
dir:'<path d="m9 6 6 6-6 6"/>',
grade:'<rect x="4" y="4" width="7" height="7" rx="1.5"/><rect x="13" y="4" width="7" height="7" rx="1.5"/><rect x="4" y="13" width="7" height="7" rx="1.5"/><rect x="13" y="13" width="7" height="7" rx="1.5"/>',
lista:'<path d="M9 6h11M9 12h11M9 18h11M4.5 6h.5M4.5 12h.5M4.5 18h.5"/>',
olho:'<path d="M3 3l18 18M10.6 6.1A10 10 0 0 1 12 6c5 0 9 6 9 6a16 16 0 0 1-3 3.5M6.6 6.6C4.2 8.2 3 12 3 12s4 6 9 6c1.6 0 3-.4 4.3-1"/>',
salvo:'<path d="M6 3h12v18l-6-4-6 4z"/>',
brilho:'<path d="M12 3l1.9 5.1L19 10l-5.1 1.9L12 17l-1.9-5.1L5 10l5.1-1.9z"/><path d="M19 16v4M17 18h4"/>',
boia:'<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="4"/><path d="m5.6 5.6 3.6 3.6M14.8 14.8l3.6 3.6M18.4 5.6l-3.6 3.6M9.2 14.8l-3.6 3.6"/>',
envia:'<path d="M21 3 10 14M21 3l-7 18-4-7-7-4z"/>',
celular:'<rect x="7" y="2.5" width="10" height="19" rx="2.5"/><path d="M11 18h2"/>',
pular:'<path d="M5 5v14l10-7zM19 5v14"/>',
camadas:'<path d="m12 3 9 5-9 5-9-5z"/><path d="m3 13 9 5 9-5"/>',
relogio:'<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
wifi:'<path d="M2 9a15 15 0 0 1 20 0M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0M12 19.5h.01"/>',
raio:'<path d="M13 2 4 14h7l-1 8 9-12h-7z"/>',
idioma:'<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c3 3.5 3 14.5 0 18M12 3c-3 3.5-3 14.5 0 18"/>',
disco:'<rect x="3" y="13" width="18" height="7" rx="2"/><path d="M5 13 7 5h10l2 8M7 16.5h.01"/>',
importa:'<path d="M12 3v12M7 10l5 5 5-5M4 21h16"/>',
filtro:'<path d="M3 5h18l-7 8v6l-4 2v-8z"/>',
estrela:'<path d="m12 3 2.8 5.7 6.2.9-4.5 4.4 1 6.2L12 17.3 6.5 20.2l1-6.2L3 9.6l6.2-.9z"/>',
tela:'<rect x="2.5" y="4" width="19" height="13" rx="2"/><path d="M8 21h8M12 17v4"/>',
som:'<path d="M4 9v6h4l5 4V5L8 9z"/><path d="M16.5 8.5a5 5 0 0 1 0 7M19 6a8.5 8.5 0 0 1 0 12"/>',
legenda:'<rect x="3" y="5" width="18" height="14" rx="2"/><path d="M7 13h4M13 13h4M7 16h10"/>',
fecha:'<path d="M18 6 6 18"/><circle cx="12" cy="12" r="9"/>',
copia:'<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V5a1 1 0 0 0-1-1H5a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h3"/>',
pessoas:'<circle cx="9" cy="8" r="3.5"/><path d="M2.5 20c1-3.5 3.5-5.5 6.5-5.5s5.5 2 6.5 5.5"/><circle cx="17" cy="9" r="2.6"/><path d="M16 14.6c2.6.2 4.6 2 5.5 5.4"/>',
cadeado:'<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>',
trofeu:'<path d="M8 4h8v6a4 4 0 0 1-8 0z"/><path d="M8 6H4v1a4 4 0 0 0 4 4M16 6h4v1a4 4 0 0 1-4 4M12 14v4M8 21h8M9.5 18h5"/>',
claquete:'<rect x="3" y="9" width="18" height="11" rx="1.5"/><path d="m3.5 9 1.8-4.5 14.2 3-1 1.5M8 4.8 10 9M13 5.8 15 9"/>',
chat:'<path d="M4 5h16v11H10l-5 4v-4H4z"/>',
fantasma:'<path d="M5 21V11a7 7 0 0 1 14 0v10l-3-2.5-2 2.5-2-2.5-2 2.5-2-2.5z"/><path d="M9.5 11h.01M14.5 11h.01"/>',
bandeira:'<path d="M5 21V4M5 5h12l-2.5 4L17 13H5"/>'
};
document.querySelectorAll('[data-i]').forEach(e=>{e.classList.add('ic');e.innerHTML='<svg viewBox="0 0 24 24">'+(IC[e.dataset.i]||'')+'</svg>'});

// QR ilustrativo: padrões de localização reais, miolo pseudoaleatório fixo (não codifica nada)
document.querySelectorAll('#qr,.qrm').forEach(svg=>{let s=7,r='';const rnd=()=>(s=(s*16807)%2147483647)/2147483647;
  const fin=(x,y)=>`<rect x="${x}" y="${y}" width="7" height="7"/><rect x="${x+1}" y="${y+1}" width="5" height="5" fill="#fff"/><rect x="${x+2}" y="${y+2}" width="3" height="3"/>`;
  for(let y=0;y<29;y++)for(let x=0;x<29;x++){const f=(x<8&&y<8)||(x>20&&y<8)||(x<8&&y>20);if(!f&&rnd()<.48)r+=`<rect x="${x}" y="${y}" width="1" height="1"/>`}
  svg.innerHTML=r+fin(0,0)+fin(22,0)+fin(0,22)});

const caixas=[...document.querySelectorAll('.caixa')];
function escala(){const k=Math.min(innerWidth/1920,(innerHeight-90)/1080);
  caixas.forEach(c=>{c.style.width=1920*k+'px';c.style.height=1080*k+'px';c.firstElementChild.style.transform='scale('+k+')'})}
addEventListener('resize',escala);escala();

// barra: acento dinâmico (nunca fixo) e material vidro/sólido
const ACENTOS=[['#EC407A','#fff'],['#3D8BFF','#fff'],['#7C5CFF','#fff'],['#00A88F','#fff'],['#F5F5F5','#111']];
const barra=document.createElement('div');barra.className='barra';
barra.innerHTML=(document.body.dataset.indice?'':'<a href="index.html">← telas</a>')+'<span>acento</span>'+ACENTOS.map((a,i)=>`<button class="cor" style="background:${a[0]}" data-c="${i}" aria-pressed="${!i}" aria-label="acento ${a[0]}"></button>`).join('')+
  '<span>|</span><button data-m="vidro" aria-pressed="true">vidro</button><button data-m="solido" aria-pressed="false">sólido</button>'+
  (document.querySelector('[data-d]')?'<span>|</span><button data-dados aria-pressed="false">dados (D): <b style="color:#4caf50">hoje</b> · <b style="color:#ffb74d">nosso</b> · <b style="color:#ef5350">bloqueado</b></button>':'')+'<span>| setas movem o foco</span>';
document.body.appendChild(barra);document.body.dataset.mat='vidro';
const dados=()=>barra.querySelector('[data-dados]').setAttribute('aria-pressed',document.body.classList.toggle('dados'));
barra.addEventListener('click',e=>{const b=e.target.closest('button');if(!b)return;if(b.hasAttribute('data-dados'))return dados();
  if(b.dataset.c){const a=ACENTOS[b.dataset.c];document.documentElement.style.setProperty('--ac',a[0]);document.documentElement.style.setProperty('--tinta',a[1])}
  if(b.dataset.m)document.body.dataset.mat=b.dataset.m;
  b.parentNode.querySelectorAll(b.dataset.c?'[data-c]':'[data-m]').forEach(x=>x.setAttribute('aria-pressed',x===b))});

// D-pad: a TV ativa é a que está mais visível; o foco salta para o vizinho mais perto na direção
let ativa=caixas[0];
const vista=new IntersectionObserver(es=>es.forEach(e=>{if(e.isIntersecting)ativa=e.target}),{threshold:.6});
caixas.forEach(c=>vista.observe(c));
function focar(el){ativa.querySelectorAll('.foco').forEach(e=>e.classList.remove('foco'));el.classList.add('foco')}
document.addEventListener('click',e=>{const el=e.target.closest('[data-f]');if(el){ativa=el.closest('.caixa');focar(el)}});
document.addEventListener('keydown',e=>{
  if((e.key==='d'||e.key==='D')&&barra.querySelector('[data-dados]'))return dados();
  if(e.key==='Enter'){const a=ativa.querySelector('.foco');if(a&&a.href)location=a.href;return}
  const d={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]}[e.key];if(!d)return;
  const cur=ativa.querySelector('.foco');if(!cur)return;e.preventDefault();
  const c=cur.getBoundingClientRect(),cx=c.x+c.width/2,cy=c.y+c.height/2;let best,bd=1e9;
  ativa.querySelectorAll('[data-f]').forEach(el=>{if(el===cur)return;const r=el.getBoundingClientRect(),dx=r.x+r.width/2-cx,dy=r.y+r.height/2-cy;
    const ao=dx*d[0]+dy*d[1];if(ao<=4)return;const lat=Math.abs(dx*d[1]+dy*d[0]);const s=ao+lat*2.5;if(s<bd){bd=s;best=el}});
  if(best)focar(best)});
