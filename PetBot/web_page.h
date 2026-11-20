// PetBot er phone page (http://petbot.local). Alada file e, karon Arduino IDE .ino er bhitore boro HTML e gondogol kore.
// Upore 3 ta mode (Normal / Game / Clock), niche Settings. Robot er API: /api/...
#pragma once

// Browser tab er chhoto robot (favicon)
static const char FAVICON_SVG[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><line x1="32" y1="4" x2="32" y2="12" stroke="#ffd33d" stroke-width="3"/><circle cx="32" cy="5" r="4" fill="#ffd33d"/><rect x="4" y="12" width="56" height="46" rx="15" fill="#2f81f7"/><rect x="13" y="24" width="15" height="17" rx="6" fill="#fff"/><rect x="36" y="24" width="15" height="17" rx="6" fill="#fff"/><path d="M24 48q8 6 16 0" stroke="#fff" stroke-width="3.5" fill="none" stroke-linecap="round"/></svg>)SVG";

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="bn"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>PetBot</title><link rel="icon" type="image/svg+xml" href="/favicon.svg">
<meta name="theme-color" content="#0d1117"><meta name="apple-mobile-web-app-title" content="PetBot">
<style>
:root{--bg:#0d1117;--card:#161b22;--card2:#1c2230;--line:#2a3140;--tx:#e6edf3;--mut:#8b949e;--acc:#2f81f7;--ok:#2ea043;--warn:#d29922;--bad:#f85149;--yel:#ffd33d}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--tx);-webkit-tap-highlight-color:transparent}
.wrap{max-width:560px;margin:0 auto;padding:0 12px 90px}
header{background:linear-gradient(180deg,#132036,#0d1117);border-bottom:1px solid var(--line);padding:12px}
.top{display:flex;align-items:center;gap:10px;max-width:560px;margin:0 auto}
.top img{width:38px;height:38px}.top h1{margin:0;font-size:19px;line-height:1.1}.top small{color:var(--mut);font-size:12px}
.dot{margin-left:auto;width:10px;height:10px;border-radius:50%;background:var(--mut)}.dot.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.chips{display:flex;gap:6px;flex-wrap:wrap;max-width:560px;margin:8px auto 0}
.chip{font-size:12px;background:var(--card2);border:1px solid var(--line);border-radius:20px;padding:3px 9px;white-space:nowrap}
.date{max-width:560px;margin:6px auto 0;font-size:12.5px;color:var(--mut)}
.modes{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:14px 0 4px}
.mode{border:1px solid var(--line);background:var(--card);border-radius:16px;padding:12px 6px;color:var(--tx);text-align:center;cursor:pointer;position:relative}
.mode b{display:block;font-size:15px;margin-top:2px}.mode span{font-size:26px}.mode i{display:block;font-style:normal;font-size:11px;color:var(--mut);margin-top:2px}
.mode.sel{border-color:var(--acc);background:#14233d}
.mode.live::after{content:"robot";position:absolute;top:6px;right:8px;font-size:9px;background:var(--ok);color:#fff;border-radius:6px;padding:1px 5px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:14px;margin-top:12px}
.card h2{font-size:14.5px;margin:0 0 10px;color:#9ecbff;display:flex;align-items:center;gap:6px}.card h2 em{margin-left:auto;font-style:normal;font-size:11px;color:var(--mut);font-weight:400}
.g4{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}.g3{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.g2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
button{font:inherit;border:0;border-radius:12px;padding:10px 4px;background:var(--card2);color:var(--tx);font-size:12.5px;cursor:pointer;border:1px solid transparent}
button:active{transform:scale(.97);background:#22304a}button.on{border-color:var(--acc);background:#14233d}
.e{font-size:24px;display:block;line-height:1.25}
.pri{background:var(--acc);color:#fff;font-weight:600;padding:10px 16px}.grn{background:var(--ok);color:#fff;font-weight:600;padding:10px 16px}.red{background:#3a1d1f;color:#ffb3ad}
.row{display:flex;gap:8px}.row>*{flex:1}.row>button{flex:0 0 auto}
input{font:inherit;width:100%;padding:10px 12px;border-radius:12px;border:1px solid var(--line);background:var(--bg);color:var(--tx);font-size:15px}
.game{text-align:left;padding:10px;display:flex;gap:10px;align-items:center}.game .e{font-size:26px}.game b{display:block;font-size:13.5px}.game i{font-style:normal;font-size:11px;color:var(--mut)}
.ans{margin-top:10px;min-height:22px;font-size:15px;text-align:center;color:var(--yel)}
.big{font-size:44px;font-weight:700;text-align:center;letter-spacing:2px;font-variant-numeric:tabular-nums;margin:2px 0 10px}
.pr{display:grid;grid-template-columns:1fr auto;gap:6px 10px;font-size:15px}.pr div{padding:6px 10px;border-radius:10px}.pr .nx{background:#14233d;color:#9ecbff;font-weight:600}
.kv{display:grid;grid-template-columns:auto 1fr;gap:6px 12px;font-size:13px}.kv span{color:var(--mut)}
.songs button{text-align:left;padding:10px}
.srow{display:grid;grid-template-columns:1fr auto 70px;gap:6px;align-items:center;margin-bottom:8px;font-size:13px}.srow input{padding:8px 6px;font-size:14px}.srow button{padding:9px 2px}
.tp{display:inline-flex;gap:3px;align-items:center;font-weight:600}.tp select{font:inherit;font-weight:600;padding:7px 3px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--tx);font-size:15px}.tp .ap{color:var(--yel)}
.rem{display:flex;gap:8px;align-items:center;background:var(--card2);border-radius:12px;padding:8px 10px;margin-bottom:6px;font-size:13.5px}.rem b{flex:1;font-weight:500}.rem span{color:#9ecbff;font-size:12px;white-space:nowrap}.rem button{padding:6px 8px}
details{margin-top:12px}summary{list-style:none;cursor:pointer;background:var(--card);border:1px solid var(--line);border-radius:16px;padding:14px;font-weight:600;color:#9ecbff}
summary::-webkit-details-marker{display:none}details[open] summary{border-radius:16px 16px 0 0}
details .card{margin-top:0;border-radius:0;border-top:0}details .card:last-child{border-radius:0 0 16px 16px}
.hide{display:none}.mut{color:var(--mut);font-size:12px}
#toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#21262d;border:1px solid var(--line);padding:9px 16px;border-radius:20px;opacity:0;transition:.25s;pointer-events:none;max-width:90%;text-align:center;font-size:14px}
</style></head><body>
<header><div class="top"><img src="/favicon.svg" alt=""><div><h1>PetBot</h1><small>by Al-Mamun</small></div><div class="dot" id="dot" title="robot"></div></div>
<div class="chips" id="chips"><span class="chip">...</span></div><div class="date" id="date"></div></header>
<div class="wrap">
<div class="modes">
<div class="mode" data-m="normal"><span>😊</span><b>Normal</b><i>face, gaan, message</i></div>
<div class="mode" data-m="game"><span>🎮</span><b>Game</b><i>8 ta khela</i></div>
<div class="mode" data-m="clock"><span>⏰</span><b>Clock</b><i>shomoy, timer, namaz</i></div>
</div>
<div class="mut" style="text-align:center">Mode e chap dile robot o oi mode e chole jabe</div>

<!-- ===== NORMAL ===== -->
<div id="p-normal" class="pane">
<div class="card"><h2>💬 Message pathao <em>robot er screen e dekhabe</em></h2>
<div class="row"><input id="m" maxlength="140" placeholder="Khete esho! (English akkhor)"><button class="pri" onclick="sendMsg()">Send</button></div></div>
<div class="card"><h2>⚡ Quick</h2><div class="g4">
<button onclick="api('funny')"><span class="e">😂</span>Funny</button>
<button onclick="api('song')"><span class="e">🎵</span>Next gaan</button>
<button onclick="api('mood?m=3')"><span class="e">😍</span>Love</button>
<button onclick="api('weather')"><span class="e">🌤️</span>Weather</button>
<button onclick="api('sleep')"><span class="e">😴</span>Ghum</button>
<button onclick="api('wake')"><span class="e">☀️</span>Jagao</button>
<button onclick="api('mood?m=2')"><span class="e">😆</span>Hasi</button>
<button onclick="api('mood?m=28')"><span class="e">😗</span>Siti</button>
</div></div>
<div class="card"><h2>😊 Face <em id="fcnt"></em></h2><div class="g4" id="faces"></div></div>
<div class="card"><h2>🎶 Gaan</h2><div class="g2 songs" id="songs"></div></div>
</div>

<!-- ===== GAME ===== -->
<div id="p-game" class="pane hide">
<div class="card"><h2>🕹️ Robot e khelo <em>touch / jhakuni / kaat</em></h2><div class="g2" id="games"></div>
<div class="row" style="margin-top:8px"><button onclick="api('game')">📋 Robot e menu</button><button class="red" onclick="api('game?g=off')">🚪 Khela bondho</button></div></div>
<div class="card"><h2>📱 Phone theke khelo</h2>
<div class="row"><input id="q" placeholder="Mone proshno likho (ichchhe)..."><button class="pri" onclick="ask()">🎱 Jiggesh</button></div>
<div class="g4" style="margin-top:8px">
<button onclick="dice()"><span class="e">🎲</span>Dice</button>
<button onclick="rps('rock')"><span class="e">✊</span>Pathor</button>
<button onclick="rps('paper')"><span class="e">✋</span>Kagoj</button>
<button onclick="rps('scissors')"><span class="e">✌️</span>Kanchi</button></div>
<div class="ans" id="ans"></div></div>
<div class="card"><h2>🏆 Record <em>robot chalu thaka porjonto</em></h2><div class="kv" id="rec"></div></div>
</div>

<!-- ===== CLOCK ===== -->
<div id="p-clock" class="pane hide">
<div class="card"><h2>🖥️ Robot er screen</h2><div class="g3" id="views"></div></div>
<div class="card"><h2>⏱️ Timer <em id="tmst"></em></h2><div class="big" id="tmbig">00:00</div>
<div class="row"><input id="tm" type="number" min="1" max="99" value="5" style="max-width:90px"><button class="grn" onclick="api('timer?min='+tm.value)">▶ Start (min)</button></div>
<div class="g3" style="margin-top:8px"><button onclick="api('timer?a=toggle')">⏯️ Pause/Cholo</button><button onclick="api('timer?a=add')">➕ 1 min</button><button class="red" onclick="api('timer?a=reset')">⏹️ Reset</button></div></div>
<div class="card"><h2>⏰ Alarm <em>4 ta porjonto, je kono shomoy</em></h2><div id="alist"></div>
<div class="row" style="margin-top:8px;align-items:center"><span id="anew" class="tp"></span><button id="aonce" onclick="this.classList.toggle('on');this.textContent=this.classList.contains('on')?'1 bar':'Protidin'">Protidin</button><button class="pri" onclick="addAlarm()">➕ Add</button></div>
<div class="mut" style="margin-top:6px">Bajle robot e touch korle thambe. Shokal e shurjo uthbe, onno shomoy ghonta bajbe.</div></div>
<div class="card"><h2>🌙 Ghum, 🎂 Jonmodin</h2>
<div class="srow"><span>🌙 Ghumer shomoy</span><span id="bt" class="tp"></span><button id="btb">Bondho</button></div>
<div class="srow"><span>🎂 Jonmodin</span><input type="date" id="bd"><button onclick="sched('bday='+(bd.value||'off'))">Save</button></div>
<div class="mut">Ghumer shomoy robot mone koriye nijeo ghumiye pore.</div></div>
<div class="card"><h2>📝 Reminder <em>3 ta porjonto</em></h2><div id="rems"></div>
<input id="rt" maxlength="38" placeholder="Ki mone korabo? (jemon: Pani khao)" style="margin-top:8px">
<div class="row" style="margin-top:8px;align-items:center"><span id="rtm" class="tp"></span><button class="pri" onclick="addRem()">➕ Add</button><button onclick="sched('remin=30&t='+encodeURIComponent(rt.value));rt.value=''">+30 min</button></div></div>
<div class="card"><h2>🕌 Namaz (<span id="prcity">...</span>) <em id="ram"></em></h2><div class="pr" id="pr"><div class="mut">...</div></div></div>
<div class="card"><h2>📅 Tarikh</h2><div class="kv" id="cal"></div></div>
</div>

<!-- ===== SETTINGS ===== -->
<details><summary>⚙️ Settings, Telegram, info</summary>
<div class="card"><h2>🔧 Setting</h2><div class="g3">
<button id="b-snd" onclick="api('mute')"><span class="e">🔊</span>Sound</button>
<button id="b-lng" onclick="api('lang')"><span class="e">🌐</span>Bhasha</button>
<button id="b-azn" onclick="api('azan').then(info)"><span class="e">🔔</span>Namaz alert</button>
<button id="b-fmt" onclick="api('fmt').then(()=>{info();sched('')})"><span class="e">🕐</span>12 ghonta</button>
<button onclick="api('qr')"><span class="e">🐙</span>GitHub QR</button>
<button onclick="api('battery')"><span class="e">🔋</span>Battery</button></div>
<div class="mut" style="margin-top:8px">Namaz alert = namaz er shomoy Telegram e message</div></div>
<div class="card"><h2>✈️ Telegram <em>je kono jayga theke</em></h2><div id="tgs" style="font-size:13px;margin-bottom:10px;line-height:1.6">...</div>
<div class="row"><input id="tk" type="password" autocomplete="off" placeholder="BotFather er token boshao"><button class="pri" onclick="saveTk()">Save</button></div>
<div class="row" style="margin-top:8px"><button onclick="api('tg/pair')">🔗 Pair (10 min)</button><button class="red" onclick="if(confirm('Telegram token muche felbo?'))api('tg/forget')">🗑️ Forget</button></div></div>
<div class="card"><h2>ℹ️ Robot</h2><div class="kv" id="dev"></div></div>
</details>
<div style="text-align:center;margin:22px 0 4px;color:var(--mut);font-size:12px;line-height:1.6"><b style="color:#9ecbff;letter-spacing:2px">PETBOT</b><br>Designed &amp; Developed by <a style="color:#9ecbff" href="https://github.com/999mamun999">Al-Mamun</a><br>A Personal Project • 2026</div>
</div>
<div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const F=[[0,'😐','Normal'],[1,'😊','Happy'],[2,'😆','Laugh'],[3,'😍','Love'],[4,'😉','Wink'],[5,'😢','Sad'],[6,'😭','Cry'],[7,'😤','Angry'],
[8,'😲','Surprise'],[9,'😱','Scared'],[10,'🤔','Confused'],[11,'🤨','Suspicious'],[12,'😑','Bored'],[13,'🤩','Excited'],[14,'😪','Sleepy'],
[15,'😴','Sleep'],[16,'😵','Dizzy'],[17,'🥶','Cold'],[18,'🥵','Hot'],[19,'🌧️','Rainy'],[20,'⛈️','Stormy'],[21,'😎','Sunny'],
[23,'🥺','Please'],[24,'😒','Annoyed'],[25,'🤧','Sneeze'],[26,'🙆','Stretch'],[27,'🎵','Hum'],[28,'😗','Whistle']];
const G=[[1,'🎱','Magic Ball','jhankao, uttor pao'],[2,'🎲','Ludu Chhokka','jhankao ba tap'],[3,'✊','Pathor-Kagoj-Kanchi','tap bachai, chepe khelo'],
[4,'⚡','Ke Beshi Chotpote?','shada hole touch'],[5,'⭐','Tara Dhoro','kaat kore jhuri sorao'],[6,'🐦','Pakhi Orao','touch korle ure'],
[7,'🧠','Mone Rakho','tir er order kaat kore'],[8,'⚫','Gorte Bol','kaat kore bol gorao']];
const V=[[0,'🏠','Home'],[1,'📆','Date'],[2,'🇧🇩','Bangla'],[3,'🌙','Hijri'],[4,'🕌','Namaz'],[5,'📅','Din'],[6,'🌤️','Weather'],[7,'🌡️','Ghor er tap'],[8,'⏱️','Timer']];
faces.innerHTML=F.map(f=>`<button onclick="api('mood?m=${f[0]}')"><span class="e">${f[1]}</span>${f[2]}</button>`).join('');fcnt.textContent=F.length+' ta';
games.innerHTML=G.map(g=>`<button class="game" onclick="api('game?start=${g[0]}')"><span class="e">${g[1]}</span><span><b>${g[2]}</b><i>${g[3]}</i></span></button>`).join('');
views.innerHTML=V.map(v=>`<button id="v${v[0]}" onclick="api('view?v=${v[0]}')"><span class="e">${v[1]}</span>${v[2]}</button>`).join('');
let S={},I={},tab=null,tmLeft=0,tmRun=0,tmAt=0;
function toast(t){const x=$('toast');x.textContent=t;x.style.opacity=1;clearTimeout(x._t);x._t=setTimeout(()=>x.style.opacity=0,1600)}
function api(p,quiet){return fetch('/api/'+p).then(r=>r.text()).then(t=>{if(!quiet)toast(t);setTimeout(st,400);return t}).catch(()=>{toast('✗ robot pawa jay ni');return ''})}
function show(m){tab=m;document.querySelectorAll('.pane').forEach(p=>p.classList.toggle('hide',p.id!='p-'+m));
document.querySelectorAll('.mode').forEach(b=>b.classList.toggle('sel',b.dataset.m==m))}
document.querySelectorAll('.mode').forEach(b=>b.onclick=()=>{show(b.dataset.m);api('mode?m='+b.dataset.m)});
function sendMsg(){const v=m.value.trim();if(!v)return;api('msg?t='+encodeURIComponent(v));m.value=''}
m.addEventListener('keydown',e=>{if(e.key=='Enter')sendMsg()});
function ask(){api('game?g=8ball',1).then(t=>{ans.textContent=(q.value.trim()?'„'+q.value.trim()+'“ → ':'🎱 ')+t;q.value=''})}
function dice(){api('game?g=dice',1).then(t=>{const n=parseInt(t.replace(/\D/g,''))||0;ans.textContent='🎲 '+('⚀⚁⚂⚃⚄⚅'[n-1]||'')+' '+n+(n==6?'  ছক্কা! 🎉':'')})}
function rps(w){api('game?g='+w,1).then(t=>{ans.textContent=t.replace(/\n+/g,'  ')})}
fetch('/api/songs').then(r=>r.json()).then(s=>{songs.innerHTML=s.map((n,i)=>`<button onclick="api('song?i=${i}')">🎵 ${n}</button>`).join('')});
function saveTk(){const v=tk.value.trim();if(!v)return;fetch('/api/tg/token',{method:'POST',body:new URLSearchParams({token:v})}).then(r=>r.text()).then(t=>{toast(t);tk.value='';setTimeout(st,500)}).catch(()=>toast('✗ robot pawa jay ni'))}
function tgShow(s){let h;if(s.tg=='off')h='⚪ Token nei. BotFather er token boshao.';
else{h=(s.tg=='ok'?'🟢 Connected':'🟡 '+s.tg)+(s.tgBot?` · <a style="color:#9ecbff" href="https://t.me/${s.tgBot}">@${s.tgBot}</a>`:'');
h+='<br>'+(s.tgPair?`🔗 Pair mode (${s.tgPair} min): Telegram e bot ke <b>/start</b> pathao`:(s.tgOwner?'👤 Malik: '+s.tgOwner:'👤 Malik nei: 🔗 Pair chapo'))}tgs.innerHTML=h}
const mmss=x=>String(Math.floor(x/60)).padStart(2,'0')+':'+String(x%60).padStart(2,'0');
function tmDraw(){let l=tmLeft;if(tmRun)l=Math.max(0,tmLeft-Math.floor((Date.now()-tmAt)/1000));tmbig.textContent=mmss(l);
tmst.textContent=S.tmAlarm?'⏰ TIME UP!':(tmRun?'▶ cholche':(l?'⏸ thama':'bondho'))}
function st(){fetch('/api/status').then(r=>r.json()).then(s=>{S=s;dot.className='dot on';
const md={normal:'😊 '+s.mood,game:'🎮 khela cholche',clock:'⏰ clock'}[s.mode]||s.mood;
chips.innerHTML=[md,'🕐 '+s.time,'🌤️ '+s.weather,s.bat>=0?(s.bat<=15?'🪫 ':'🔋 ')+s.bat+'%':'',s.sound=='mute'?'🔇 mute':'🔊 sound',s.lang=='EN'?'🌐 English':'🌐 বাংলা'].filter(c=>c).map(c=>`<span class="chip">${c}</span>`).join('');
document.querySelectorAll('.mode').forEach(b=>b.classList.toggle('live',b.dataset.m==s.mode));
if(!tab)show(s.mode);
V.forEach(v=>{const b=$('v'+v[0]);if(b)b.classList.toggle('on',s.view==v[0])});
tmLeft=s.tm;tmRun=s.tmRun;tmAt=Date.now();tmDraw();
$('b-snd').classList.toggle('on',s.sound!='mute');$('b-snd').querySelector('.e').textContent=s.sound=='mute'?'🔇':'🔊';
$('b-lng').querySelector('.e').textContent=s.lang=='EN'?'A':'অ';
$('b-fmt').lastChild.textContent=s.h24?'24 ghonta':'12 ghonta';$('b-fmt').querySelector('.e').textContent=s.h24?'🕓':'🕐';tgShow(s)}).catch(()=>{dot.className='dot'})}
function info(){fetch('/api/info').then(r=>r.json()).then(i=>{I=i;
if(i.city)prcity.textContent=i.city;
date.textContent=i.bn?`📅 ${i.bn} · ${i.sea} · 🌙 ${i.hj}`+(i.sp?` · 🎉 ${i.sp}`:''):'';
cal.innerHTML=i.bn?`<span>বাংলা</span><b>${i.bn} (${i.sea})</b><span>হিজরি</span><b>${i.hj}</b>`+(i.sp?`<span>আজ</span><b>🎉 ${i.sp}</b>`:'')+(i.next?`<span>পরের</span><b>${i.next} — ${i.nextDays} din baki</b>`:''):'<span>...</span><b>shomoy pai ni</b>';
if(i.pr){let nx=i.pr.findIndex(p=>p[2]>i.nowMin);if(nx<0)nx=0;
pr.innerHTML=i.pr.map((p,k)=>`<div class="${k==nx?'nx':''}">${p[0]}</div><div class="${k==nx?'nx':''}">${p[1]}</div>`).join('')+(i.ram?`<div class="nx">🌙 সাহরি শেষ</div><div class="nx">${i.sehri}</div><div class="nx">🌅 ইফতার</div><div class="nx">${i.iftar}</div>`:'');
ram.textContent=i.ram?'🌙 রমজান':'Karachi, Hanafi'}
const r=i.rec;rec.innerHTML=`<span>⚡ Chotpote</span><b>${r.react?r.react+' ms':'—'}</b><span>⭐ Tara</span><b>${r.catch||'—'}</b><span>🐦 Pakhi</span><b>${r.flap||'—'}</b><span>🧠 Mone rakho</span><b>${r.simon?'level '+r.simon:'—'}</b><span>⚫ Gorte bol</span><b>${r.hole?'level '+r.hole:'—'}</b><span>✊ Tumi-Ami</span><b>${r.rps}</b>`;
$('b-azn').classList.toggle('on',!!i.azan);$('b-azn').querySelector('.e').textContent=i.azan?'🔔':'🔕';
const sig=i.rssi>-60?'darun':i.rssi>-72?'bhalo':'durbol';
dev.innerHTML=`<span>Thikana</span><b><a style="color:#9ecbff" href="http://petbot.local">petbot.local</a> · ${i.ip}</b><span>WiFi</span><b>${i.ssid} (${sig}, ${i.rssi} dBm)</b><span>Chalu</span><b>${Math.floor(i.up/60)} ghonta ${i.up%60} min</b><span>Battery</span><b>${S.bat>=0?S.bat+'% ('+S.batV+' V)':'tar lagano nei (LiPo B+ → 150k → A0)'}</b><span>Free RAM</span><b>${(i.heap/1024).toFixed(1)} KB</b>`}).catch(()=>{})}
let SC=null;const esc=s=>s.replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
function sched(q){return fetch('/api/sched'+(q?'?'+q:'')).then(r=>r.json()).then(s=>{SC=s;schedShow();if(q)toast('✓ save holo')}).catch(()=>toast('✗ robot pawa jay ni'))}
function onOff(b,on){b.textContent=on?'✓ Chalu':'Bondho';b.classList.toggle('on',!!on)}
// ---- PetBot er format (12 ba 24 ghonta) e shomoy bachai ----
const p2=n=>String(n).padStart(2,'0');
function tpHtml(v){let [H,M]=(v||'07:00').split(':').map(Number);const h24=SC&&SC.h24;let o='';
if(h24)for(let h=0;h<24;h++)o+=`<option value="${h}"${h==H?' selected':''}>${p2(h)}</option>`;
else for(const h of [12,1,2,3,4,5,6,7,8,9,10,11])o+=`<option value="${h}"${h==(H%12||12)?' selected':''}>${h}</option>`;
let m='';for(let k=0;k<60;k++)m+=`<option value="${k}"${k==M?' selected':''}>${p2(k)}</option>`;
return `<select class="hh">${o}</select>:<select class="mm">${m}</select>`+(h24?'':`<select class="ap"><option${H<12?' selected':''}>AM</option><option${H>=12?' selected':''}>PM</option></select>`)}
function tpVal(el){let h=+el.querySelector('.hh').value;const m=+el.querySelector('.mm').value,ap=el.querySelector('.ap');if(ap)h=h%12+(ap.value=='PM'?12:0);return p2(h)+':'+p2(m)}
const busy=el=>{const a=document.activeElement;return a&&a.tagName=='SELECT'&&el.contains(a)};   // bachai cholle notun kore aanki na
let tpFmt=null;
function schedShow(){const fmtChg=tpFmt!==SC.h24;tpFmt=SC.h24;
if(fmtChg||!anew.firstChild)anew.innerHTML=tpHtml(anew.firstChild?tpVal(anew):'07:00');
if(fmtChg||!rtm.firstChild){const d=new Date(Date.now()+3600000);rtm.innerHTML=tpHtml(rtm.firstChild?tpVal(rtm):p2(d.getHours())+':00')}
if(!busy(bt)&&(fmtChg||!bt.firstChild||tpVal(bt)!=SC.bed))bt.innerHTML=tpHtml(SC.bed);
bd.value=SC.bday||'';onOff(btb,SC.bedOn);
if(!busy(alist))alist.innerHTML=SC.alarms.length?SC.alarms.map(a=>`<div class="rem"><span class="tp" onchange="sched('aset=${a.i}&t='+tpVal(this))">${tpHtml(a.t)}</span><span>${a.once?'1 bar':'protidin'}</span><b></b><button class="${a.on?'on':''}" onclick="sched('aon=${a.i}&v=${a.on?0:1}')">${a.on?'✓':'Bondho'}</button><button onclick="sched('adel=${a.i}')">🗑️</button></div>`).join(''):'<div class="mut">Kono alarm nei</div>';
rems.innerHTML=SC.rem.length?SC.rem.map(r=>`<div class="rem"><span>🕐 ${r.when}</span><b>${esc(r.t)}</b><button onclick="sched('del=${r.i}')">🗑️</button></div>`).join(''):'<div class="mut">Kono reminder nei</div>'}
function addAlarm(){if(SC&&SC.alarms.length>=4){toast('😅 4 ta alarm already ache');return}sched('aadd='+tpVal(anew)+'&once='+(aonce.classList.contains('on')?1:0))}
btb.onclick=()=>sched('bed='+tpVal(bt)+'&on='+(SC&&SC.bedOn?0:1));
bt.onchange=()=>{if(SC&&SC.bedOn)sched('bed='+tpVal(bt)+'&on=1')};
function addRem(){sched('rem='+tpVal(rtm)+'&t='+encodeURIComponent(rt.value));rt.value=''}
sched('');setInterval(()=>{if(tab=='clock')sched('')},30000);
st();info();setInterval(st,3000);setInterval(info,60000);setInterval(tmDraw,1000);
</script></body></html>)HTML";
