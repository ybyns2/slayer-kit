#include "core/WebPages.h"

#include "core/Constants.h"

namespace sk::web {

namespace {

void Sub(std::string& s, const std::string& from, const std::string& to) {
    for (size_t p = 0; (p = s.find(from, p)) != std::string::npos; p += to.size()) s.replace(p, from.size(), to);
}

std::string Hex(const packs::Color& c) {
    char b[8];
    snprintf(b, sizeof b, "#%02X%02X%02X", (int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255));
    return b;
}

std::string Fill(std::string html, const Theme& t, const std::string& token) {
    Sub(html, "__A__", t.accent);
    Sub(html, "__B__", t.accent2);
    Sub(html, "__BG__", t.background);
    Sub(html, "__NAME__", t.name);
    Sub(html, "__LOGO__", t.logoUrl);
    Sub(html, "__TOKEN__", token);
    return html;
}

const char* kChat = R"SKW(<!doctype html><html lang="fr"><head><meta charset="utf-8"><title>Chat</title><style>
*{box-sizing:border-box}html,body{margin:0;height:100%;overflow:hidden;background:transparent;font-family:"Bahnschrift","Segoe UI",sans-serif}
body.win{background:__BG__}
#list{position:absolute;left:0;right:0;bottom:0;padding:12px;display:flex;flex-direction:column;gap:8px}
.m{display:flex;gap:10px;align-items:flex-start;padding:9px 12px;border-radius:12px;background:rgba(12,10,18,.8);border-left:3px solid __A__;
color:#f4f1f8;font-size:20px;line-height:1.3;animation:in .25s ease-out;word-break:break-word}
.m.out{animation:out .6s ease-in forwards}.p{flex:none;width:22px;height:22px;border-radius:6px;font-size:13px;font-weight:700;display:grid;place-items:center;color:#fff;margin-top:2px}
.p.t{background:#9146ff}.p.k{background:#53fc18;color:#111}.u{font-weight:700;margin-right:6px}.e{height:26px;vertical-align:middle}
@keyframes in{from{opacity:0;transform:translateX(30px)}to{opacity:1;transform:none}}@keyframes out{to{opacity:0;transform:translateX(-20px)}}
</style></head><body><div id="list"></div><script>
const P=new URLSearchParams(location.search),WIN=P.get('fenetre')==='1',T='__TOKEN__';if(WIN)document.body.classList.add('win');
const MAX=WIN?60:12,FADE=WIN?0:45,TW=P.get('twitch')||'';
const list=document.getElementById('list');
const esc=s=>s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function add(p,user,color,html){const d=document.createElement('div');d.className='m';
d.innerHTML='<span class="p '+p+'">'+(p==='t'?'T':'K')+'</span><span><span class="u" style="color:'+(color||'__B__')+'">'+esc(user)+'</span>'+html+'</span>';
list.appendChild(d);while(list.children.length>MAX)list.firstChild.remove();
if(FADE)setTimeout(()=>{d.classList.add('out');setTimeout(()=>d.remove(),700)},FADE*1000)}
function twitch(){if(!TW)return;const ws=new WebSocket('wss://irc-ws.chat.twitch.tv:443');
ws.onopen=()=>{ws.send('CAP REQ :twitch.tv/tags');ws.send('PASS SCHMOOPIIE');ws.send('NICK justinfan'+Math.floor(Math.random()*80000+1000));ws.send('JOIN #'+TW.toLowerCase())};
ws.onmessage=e=>{for(const line of e.data.split('\r\n')){if(line.startsWith('PING')){ws.send('PONG :tmi.twitch.tv');continue}
const m=line.match(/^@([^ ]+) :([^!]+)![^ ]+ PRIVMSG #[^ ]+ :(.*)$/);if(!m)continue;
const tags=Object.fromEntries(m[1].split(';').map(x=>x.split('=')));let parts=[];
if(tags.emotes)for(const e of tags.emotes.split('/')){const[id,pos]=e.split(':');for(const r of pos.split(',')){const[a,b]=r.split('-').map(Number);parts.push([a,b,id])}}
parts.sort((x,y)=>x[0]-y[0]);let out='',i=0;const ch=[...m[3]];
for(const[a,b,id]of parts){out+=esc(ch.slice(i,a).join(''))+'<img class="e" src="https://static-cdn.jtvnw.net/emoticons/v2/'+id+'/default/dark/2.0">';i=b+1}
out+=esc(ch.slice(i).join(''));add('t',tags['display-name']||m[2],tags.color,out)}};ws.onclose=()=>setTimeout(twitch,3000)}
function kick(){const es=new EventSource('/events?t='+T);es.onmessage=e=>{const d=JSON.parse(e.data);if(d.type!=='chat')return;
add('k',d.user,d.color,esc(d.text).replace(/\[emote:(\d+):[^\]]*\]/g,(_,id)=>'<img class="e" src="https://files.kick.com/emotes/'+id+'/fullsize">'))}}
twitch();kick();
</script></body></html>)SKW";

const char* kAlerts = R"SKW(<!doctype html><html lang="fr"><head><meta charset="utf-8"><title>Alertes</title><style>
html,body{margin:0;height:100%;overflow:hidden;background:transparent;font-family:"Bahnschrift","Segoe UI",sans-serif}
#a{position:absolute;left:50%;top:120px;transform:translate(-50%,-30px) scale(.9);opacity:0;transition:all .5s cubic-bezier(.2,.9,.3,1.3);
display:flex;align-items:center;gap:26px;padding:26px 44px 26px 26px;border-radius:26px;background:linear-gradient(120deg,rgba(10,8,16,.92),rgba(10,8,16,.82));
border:2px solid __A__;box-shadow:0 0 60px __A__88, inset 0 0 30px __A__33;min-width:620px}
#a.on{opacity:1;transform:translate(-50%,0) scale(1)}
#a img{width:120px;height:120px;border-radius:22px;object-fit:cover;box-shadow:0 0 30px __A__}
.k{font-size:24px;letter-spacing:4px;font-weight:700;color:__B__;text-transform:uppercase}
.u{font-size:58px;font-weight:800;color:#fff;line-height:1.05;text-shadow:0 0 24px __A__}
.x{font-size:24px;color:#ddd;margin-top:6px;max-width:700px}
.bar{position:absolute;left:0;bottom:0;height:5px;background:linear-gradient(90deg,__A__,__B__);border-radius:0 0 26px 26px;width:0}
#a.on .bar{width:100%;transition:width var(--d) linear}
</style></head><body><div id="a"><img src="__LOGO__" onerror="this.style.display='none'"><div><div class="k" id="k"></div><div class="u" id="u"></div><div class="x" id="x"></div></div><div class="bar"></div></div><script>
const T='__TOKEN__',q=[];let busy=false;const box=document.getElementById('a');
const L={follow:['NOUVEAU FOLLOW','Bienvenue sur __NAME__ !'],sub:['NOUVEL ABONNÉ','Merci pour le sub !'],resub:['RÉABONNEMENT',''],
giftsub:['SUB OFFERT','Merci au généreux donateur !'],gift:['SUBS OFFERTS',''],bits:['BITS',''],raid:['RAID','Bienvenue aux raiders !']};
function chime(kind){try{const c=new (window.AudioContext||window.webkitAudioContext)();const notes=kind==='raid'||kind==='gift'?[523,659,784,1047]:[659,880,1175];
notes.forEach((f,i)=>{const o=c.createOscillator(),g=c.createGain();o.type='triangle';o.frequency.value=f;g.gain.setValueAtTime(0,c.currentTime+i*.12);
g.gain.linearRampToValueAtTime(.25,c.currentTime+i*.12+.02);g.gain.exponentialRampToValueAtTime(.001,c.currentTime+i*.12+.6);o.connect(g).connect(c.destination);o.start(c.currentTime+i*.12);o.stop(c.currentTime+i*.12+.7)})}catch(e){}}
function next(){if(busy||!q.length)return;busy=true;const d=q.shift(),l=L[d.kind]||['ALERTE',''];
let extra=l[1];if(d.kind==='resub')extra=d.months+' mois'+(d.message?' · '+d.message:'');if(d.kind==='gift')extra=d.amount+' subs offerts !';
if(d.kind==='bits')extra=d.amount+' bits'+(d.message?' · '+d.message.replace(/cheer\d+/gi,'').trim():'');if(d.kind==='raid')extra=d.amount+' viewers débarquent !';
document.getElementById('k').textContent=l[0];document.getElementById('u').textContent=d.user;document.getElementById('x').textContent=extra;
const dur=d.kind==='raid'||d.kind==='gift'?8000:6000;box.style.setProperty('--d',dur+'ms');chime(d.kind);
requestAnimationFrame(()=>box.classList.add('on'));setTimeout(()=>{box.classList.remove('on');setTimeout(()=>{busy=false;next()},700)},dur)}
const es=new EventSource('/events?t='+T);es.onmessage=e=>{const d=JSON.parse(e.data);if(d.type==='alert'){q.push(d);next()}};
</script></body></html>)SKW";

const char* kRemote = R"SKW(<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<meta name="theme-color" content="__BG__"><title>Slayer Kit · Télécommande</title><style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}body{margin:0 auto;max-width:560px;background:__BG__;color:#f4f1f8;font-family:"Bahnschrift","Segoe UI",system-ui,sans-serif;padding:18px 16px 40px}html{background:__BG__}
header{display:flex;align-items:center;gap:12px;margin-bottom:16px}header img{width:44px;height:44px;border-radius:12px}h1{font-size:20px;margin:0}small{color:#a49db2}
.st{display:flex;gap:10px;margin:14px 0}.pill{flex:1;padding:12px;border-radius:14px;background:#ffffff0d;text-align:center;font-size:14px}
.pill b{display:block;font-size:18px;margin-top:2px}.live b{color:#ff4d6a}
h2{font-size:12px;letter-spacing:2px;color:#6e6880;margin:22px 0 10px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button{border:0;border-radius:16px;padding:18px 10px;font:inherit;font-size:16px;font-weight:700;color:#fff;background:#ffffff12;transition:transform .08s}
button:active{transform:scale(.96)}button.cur{background:linear-gradient(120deg,__A__,__B__)}
.big{grid-column:span 2;padding:22px}.danger{background:#ff4d6a26;color:#ff8aa0}.ok{background:#3ddc9726;color:#7dfac0}
#msg{position:fixed;left:16px;right:16px;bottom:16px;padding:14px;border-radius:14px;background:#000c;text-align:center;opacity:0;transition:opacity .3s}
</style></head><body><header><img src="__LOGO__" onerror="this.remove()"><div><h1>__NAME__</h1><small>Télécommande Slayer Kit</small></div></header>
<div id="off" style="display:none;padding:18px;border-radius:16px;background:#ffc14522;color:#ffd37a;margin-bottom:12px">OBS est fermé.<br><button class="big ok" style="width:100%;margin-top:12px" onclick="launch()">Ouvrir OBS avec le kit</button></div>
<div class="st"><div class="pill" id="live">Live<b>—</b></div><div class="pill" id="mic">Micro<b>—</b></div></div>
<h2>SCÈNES</h2><div class="grid" id="scenes"></div>
<h2>CONTRÔLES</h2><div class="grid">
<button id="bmic" onclick="act('mute')">Couper / remettre le micro</button><button onclick="act('clip')">Clip (dernières secondes)</button>
<button class="big ok" id="bstream" onclick="stream()">Démarrer le live</button>
<button onclick="act('test-alert')">Tester une alerte</button><button onclick="load()">Actualiser</button></div>
<div id="msg"></div><script>
const T='__TOKEN__';let S={},armed=0;
const msg=t=>{const m=document.getElementById('msg');m.textContent=t;m.style.opacity=1;clearTimeout(m._t);m._t=setTimeout(()=>m.style.opacity=0,2200)};
async function api(p,method='POST'){const r=await fetch('/api/'+p+(p.includes('?')?'&':'?')+'t='+T,{method});const j=await r.json().catch(()=>({}));if(!r.ok)throw new Error(j.error||'Erreur');return j}
async function load(){try{S=await api('state','GET');document.getElementById('off').style.display=S.obs===false?'block':'none';if(S.error)msg(S.error);render()}catch(e){msg(e.message)}}
async function launch(){try{const j=await api('launch');msg(j.message||'OK');setTimeout(load,6000)}catch(e){msg(e.message)}}
function render(){const g=document.getElementById('scenes');g.innerHTML='';(S.scenes||[]).forEach(n=>{const b=document.createElement('button');b.textContent=n;if(n===S.current)b.className='cur';
b.onclick=async()=>{try{await api('scene?name='+encodeURIComponent(n));S.current=n;render()}catch(e){msg(e.message)}};g.appendChild(b)});
document.querySelector('#live b').textContent=S.streaming?'EN LIVE':'Hors ligne';document.getElementById('live').className='pill'+(S.streaming?' live':'');
document.querySelector('#mic b').textContent=S.micMuted?'Coupé':'Actif';const bs=document.getElementById('bstream');bs.textContent=S.streaming?'Couper le live (appuie 2 fois)':'Démarrer le live';bs.className='big '+(S.streaming?'danger':'ok')}
async function act(a){try{const j=await api(a);msg(j.message||'OK');setTimeout(load,400)}catch(e){msg(e.message)}}
function stream(){if(S.streaming&&Date.now()-armed>2500){armed=Date.now();msg('Appuie encore pour couper le live');return}act('stream')}
load();setInterval(load,3000);
</script></body></html>)SKW";

}

Theme FromPack(const packs::Pack* p) {
    Theme t{"#FF2E4D", "#FF8A3D", "#0B0910", "Slayer Kit", ""};
    if (!p) return t;
    t.accent = Hex(p->accent);
    t.accent2 = Hex(p->accent2);
    t.background = Hex(p->background);
    t.name = p->name;
    t.logoUrl = Narrow(k::kPacksRawBase) + "packs/" + p->id + "/" + (p->logo.empty() ? "logo.png" : p->logo);
    return t;
}

std::string ChatPage(const Theme& t, const std::string& token) { return Fill(kChat, t, token); }
std::string AlertsPage(const Theme& t, const std::string& token) { return Fill(kAlerts, t, token); }
std::string RemotePage(const Theme& t, const std::string& token) { return Fill(kRemote, t, token); }

}
