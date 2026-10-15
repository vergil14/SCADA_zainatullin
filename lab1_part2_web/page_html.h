// page_html.h — web-страница стенда (HTML + CSS + JavaScript).
// Хранится во flash-памяти ESP32 и отдаётся по запросу «/».
// Страница не использует внешние библиотеки и работает без доступа в Интернет.
#pragma once
const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="ru"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ЛР N8 · Помехоустойчивое кодирование</title>
<style>
:root{--bg:#f4f6f8;--card:#fff;--ink:#1d2733;--mute:#6b7785;--line:#dde3ea;--acc:#0f5f74;
--ok:#1f8a4c;--det:#c98a00;--und:#c62828;--P:#2563eb;--W:#d97706;--C:#059669;--I:#7c3aed}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,Segoe UI,Roboto,Arial,sans-serif}
header{background:#12303d;color:#fff;padding:14px 18px}header h1{margin:0;font-size:19px;font-weight:600}
header p{margin:2px 0 0;color:#b9cbd3;font-size:13px}
main{max-width:1100px;margin:0 auto;padding:14px;display:grid;grid-template-columns:minmax(0,1fr);gap:14px}.card{min-width:0}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px 16px}
.card h2{margin:0 0 10px;font-size:16px;color:var(--acc)}
.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}
label{font-size:13px;color:var(--mute)}input[type=text],input[type=number]{font:inherit;padding:6px 8px;border:1px solid var(--line);border-radius:6px;width:90px}
input.wide{width:180px;font-family:ui-monospace,Consolas,monospace}
button{font:inherit;padding:7px 12px;border-radius:6px;border:1px solid var(--acc);background:var(--acc);color:#fff;cursor:pointer}
button.sec{background:#fff;color:var(--acc)}button:disabled{opacity:.5;cursor:default}
.tabs{display:flex;flex-wrap:wrap;gap:6px}.tabs button{background:#fff;color:var(--ink);border-color:var(--line)}
.tabs button.on{border-color:var(--acc);background:#e3f0f3;color:var(--acc);font-weight:600}
.params{display:flex;flex-wrap:wrap;gap:16px;margin-top:10px;font-size:14px}.params b{font-family:ui-monospace,Consolas,monospace}
.bits{display:flex;flex-wrap:wrap;gap:3px;align-items:center;margin:4px 0}
.bit{width:30px;height:34px;display:flex;align-items:center;justify-content:center;border:1px solid var(--line);border-radius:5px;
font:600 17px ui-monospace,Consolas,monospace;background:#fafbfc;user-select:none}
.bit.ctl{background:#eef4ff}.bit.click{cursor:pointer}.bit.click:hover{border-color:var(--acc)}
.bit.flip{background:#ffe1e1;border-color:var(--und);color:var(--und)}.bit.bad{outline:2px solid var(--und)}
.sep{width:10px}.lbl{width:92px;font-size:13px;color:var(--mute)}
.badge{display:inline-block;padding:5px 10px;border-radius:999px;color:#fff;font-weight:600;font-size:14px}
.b-correct{background:var(--ok)}.b-detected{background:var(--det)}.b-undetected{background:var(--und)}.b-false_alarm{background:#555}
.note{font-size:13px;color:var(--mute);margin-top:6px}
.mono{font-family:ui-monospace,Consolas,monospace}
table{border-collapse:collapse;width:100%;font-size:13px}th,td{border-bottom:1px solid var(--line);padding:5px 6px;text-align:right;white-space:nowrap}
th{color:var(--mute);font-weight:600}td:first-child,th:first-child{text-align:left}
.tw{overflow-x:auto}
canvas{width:100%;height:360px;display:block}
.legend{display:flex;flex-wrap:wrap;gap:14px;font-size:13px;margin-top:6px}.legend i{display:inline-block;width:18px;height:3px;vertical-align:middle;margin-right:5px}
.err{color:var(--und);font-size:13px}
</style></head><body>
<header><h1>Простейшие коды, обнаруживающие ошибки</h1>
<p>Лабораторная работа N8 · ESP32 · чётность, постоянный вес, корреляционный и инверсный коды</p></header>
<main>

<section class="card"><h2>1. Выбор кода</h2>
<div class="tabs" id="tabs">
<button data-c="P">Чётность</button><button data-c="W">Постоянный вес</button>
<button data-c="C">Корреляционный</button><button data-c="I">Инверсный</button></div>
<div class="row" style="margin-top:10px">
<label>k (для P, C, I) <input type="number" id="k" min="1" max="12" value="5"></label>
<label>m <input type="number" id="m" min="1" max="12" value="2"></label>
<label>из n (для W) <input type="number" id="n" min="2" max="16" value="5"></label>
<button class="sec" id="apply">Применить</button><span class="err" id="perr"></span></div>
<div class="params" id="params"></div>
</section>

<section class="card"><h2>2. Передача одной комбинации</h2>
<div class="row"><label>Информация (k бит) <input type="text" class="wide" id="data" value="11011"></label>
<button id="enc">Закодировать и передать</button><button class="sec" id="rnd">Случайное сообщение</button></div>
<div style="margin-top:10px">
<div class="bits"><span class="lbl">Передано</span><span id="rowSent" class="bits"></span></div>
<div class="bits"><span class="lbl">Принято</span><span id="rowRecv" class="bits"></span></div>
<div class="note">Щёлкните по разряду в строке «Принято», чтобы исказить его (имитация помехи). Проверка выполняется на ESP32 после каждого щелчка.</div>
<div class="row" style="margin-top:8px"><button class="sec" id="noise">Случайные ошибки с p₀ из п. 3</button>
<button class="sec" id="clr">Убрать ошибки</button></div>
<div style="margin-top:12px" id="result"></div>
</div></section>

<section class="card"><h2>3. Статистический эксперимент (двоичный симметричный канал)</h2>
<div class="row"><label>p₀ <input type="text" id="p" value="0.05"></label>
<label>N <input type="number" id="N" value="100000" min="1" max="2000000" style="width:110px"></label>
<button id="run">Запустить</button><button class="sec" id="all">Все 4 кода</button>
<button class="sec" id="csv">Скачать CSV</button><button class="sec" id="hclr">Очистить</button><span id="busy" class="note"></span></div>
<div class="tw" style="margin-top:10px"><table><thead><tr>
<th>Код</th><th>k</th><th>n</th><th>p₀</th><th>N</th><th>без иск.</th><th>обнаруж.</th><th>не обнаруж.</th>
<th>K<sub>обн</sub> эксп</th><th>K<sub>обн</sub> теор</th><th>P<sub>но</sub> эксп</th><th>P<sub>но</sub> теор</th><th>мс</th></tr></thead>
<tbody id="hist"></tbody></table></div>
</section>

<section class="card"><h2>4. Сравнение кодов: доля необнаруженных ошибок 1 − K<sub>обн</sub></h2>
<canvas id="cv"></canvas>
<div class="legend" id="legend"></div>
<div class="note">Линии — теория для k, m, n из п. 1; точки — результаты экспериментов из таблицы (только при ненулевом числе необнаруженных ошибок).</div>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
const NAMES={P:'Чётность',W:'Постоянный вес',C:'Корреляционный',I:'Инверсный'};
const COL={P:'#2563eb',W:'#d97706',C:'#059669',I:'#7c3aed'};
const OUT={correct:'Принято верно',detected:'Ошибка обнаружена',undetected:'Ошибка НЕ обнаружена',false_alarm:'Ложная тревога — проверьте программу'};
let C={code:'P',k:5,m:2,n:5},info=null,sent='',recv='',theory=null,hist=[];
try{hist=JSON.parse(localStorage.getItem('fecHist')||'[]')}catch(e){hist=[]}
function saveHist(){try{localStorage.setItem('fecHist',JSON.stringify(hist))}catch(e){}}
function qs(x){return `code=${C.code}&k=${C.k}&m=${C.m}&n=${C.n}`+(x?'&'+x:'')}
async function api(p,x){const r=await fetch(`/api/${p}?${qs(x)}`);const j=await r.json();if(j.error)throw new Error(j.error);return j}
const fmt=(v,d=5)=>v==null?'—':(v!==0&&Math.abs(v)<1e-3?v.toExponential(2):(+v).toFixed(d));
function rndBits(k){let s='';for(let i=0;i<k;i++)s+=Math.random()<.5?'1':'0';return s}

// разбиение на группы для отображения
function isCtl(i){return (C.code==='P'&&i===info.k)||(C.code==='I'&&i>=info.k)}
function sepBefore(i){if(i===0)return false;if(C.code==='P'||C.code==='I')return i===info.k;if(C.code==='C')return i%2===0;return false}

function renderBits(){
  const bad=new Set();
  if(C.code==='C')for(let i=0;i<recv.length;i+=2)if(recv[i]===recv[i+1]){bad.add(i);bad.add(i+1)}
  let a='',b='';
  for(let i=0;i<sent.length;i++){
    if(sepBefore(i)){a+='<span class="sep"></span>';b+='<span class="sep"></span>'}
    const c=isCtl(i)?' ctl':'';
    a+=`<span class="bit${c}">${sent[i]}</span>`;
    b+=`<span class="bit click${c}${recv[i]!==sent[i]?' flip':''}${bad.has(i)?' bad':''}" data-i="${i}">${recv[i]}</span>`;
  }
  $('rowSent').innerHTML=a;$('rowRecv').innerHTML=b;
  $('rowRecv').querySelectorAll('.bit').forEach(e=>e.onclick=()=>{const i=+e.dataset.i;
    recv=recv.slice(0,i)+(recv[i]==='1'?'0':'1')+recv.slice(i+1);renderBits();check()});
}

function explain(){
  const w=[...recv].filter(x=>x==='1').length;
  if(C.code==='P')return `Число единиц в принятой комбинации: ${w} — ${w%2?'нечётное':'чётное'}.`;
  if(C.code==='W')return `Вес принятой комбинации: ${w}, требуемый вес m = ${info.m}.`;
  if(C.code==='C'){let bad=0;for(let i=0;i<recv.length;i+=2)if(recv[i]===recv[i+1])bad++;return `Пар с одинаковыми символами (00 или 11): ${bad}.`}
  if(C.code==='I'){const k=info.k,x=recv.slice(0,k),y=recv.slice(k);const wx=[...x].filter(t=>t==='1').length;
    const y2=wx%2?[...y].map(t=>t==='1'?'0':'1').join(''):y;let s='';for(let i=0;i<k;i++)s+=x[i]===y2[i]?'0':'1';
    return `Этап 1: в информационной группе ${wx} единиц(ы) — ${wx%2?'нечётное, контрольная группа инвертируется':'чётное, контрольная группа без изменений'}: <span class="mono">${y2}</span>.<br>Этап 2: <span class="mono">${x} ⊕ ${y2} = ${s}</span> — ${/1/.test(s)?'ненулевая сумма, комбинация бракуется':'нулевая сумма'}.`}
  return '';
}

async function check(){
  try{const j=await api('check',`sent=${sent}&word=${recv}`);
    $('result').innerHTML=`<span class="badge b-${j.outcome}">${OUT[j.outcome]}</span>
    <div class="note">Вектор ошибок: <span class="mono">${j.evec}</span>, кратность ошибки ${j.multiplicity}. Решение декодера: <b>${j.ok?'ошибок не обнаружено':'комбинация бракуется'}</b>${j.ok?', выделенная информация <span class="mono">'+j.info+'</span>':''}.</div>
    <div class="note">${explain()}</div>`;
  }catch(e){$('result').innerHTML=`<span class="err">${e.message}</span>`}
}

async function applyCode(){
  C.k=+$('k').value;C.m=+$('m').value;C.n=+$('n').value;$('perr').textContent='';
  document.querySelectorAll('#tabs button').forEach(b=>b.classList.toggle('on',b.dataset.c===C.code));
  try{info=await api('info');
    $('params').innerHTML=`<span>${NAMES[C.code]}</span><span>n = <b>${info.n}</b></span><span>k = <b>${info.k}</b></span>`+
    (C.code==='W'?`<span>C(n,m) = <b>${info.messages}</b></span>`:'')+
    `<span>R = k/n = <b>${info.R.toFixed(3)}</b></span><span>L = 1 − R = <b>${info.L.toFixed(3)}</b></span>`+
    (C.code==='W'?`<span>L по log₂C(n,m) = <b>${info.Lid.toFixed(3)}</b></span>`:'');
    if($('data').value.length!==info.k)$('data').value=rndBits(info.k);
    sent='';recv='';$('rowSent').innerHTML='';$('rowRecv').innerHTML='';$('result').innerHTML='';
  }catch(e){$('perr').textContent='Недопустимые параметры: '+e.message}
  try{theory=await (await fetch(`/api/theory?k=${C.k}&m=${C.m}&n=${C.n}`)).json()}catch(e){theory=null}
  draw();
}

async function encode(){
  try{const j=await api('encode','data='+$('data').value.replace(/\s/g,''));sent=j.word;recv=sent;renderBits();check()}
  catch(e){$('result').innerHTML=`<span class="err">${e.message}</span>`}
}

function row(h){return `<tr><td style="color:${COL[h.code]}">${NAMES[h.code]}${h.code==='W'?' '+h.m+'/'+h.n:''}</td><td>${h.k}</td><td>${h.n}</td><td>${h.p}</td><td>${h.N}</td>
<td>${h.noErr}</td><td>${h.detected}</td><td>${h.undetected}</td><td>${fmt(h.Kexp)}</td><td>${fmt(h.Kth)}</td>
<td>${fmt(h.PudExp,6)}</td><td>${fmt(h.PudTh,6)}</td><td>${h.ms}</td></tr>`}
function renderHist(){$('hist').innerHTML=hist.map(row).join('');draw()}

async function runExp(codes){
  const p=$('p').value,N=$('N').value,saved=C.code;$('busy').textContent='выполняется…';
  for(const c of codes){C.code=c;try{const j=await api('exp',`p=${p}&N=${N}`);hist.push(j)}catch(e){$('busy').textContent=e.message}}
  C.code=saved;saveHist();renderHist();$('busy').textContent='';
}

function csv(){
  const h='code;k;n;m;p0;N;noErr;detected;undetected;falseAlarm;K_exp;K_theor;Pud_exp;Pud_theor;ms';
  const t=[h].concat(hist.map(x=>[x.code,x.k,x.n,x.m,x.p,x.N,x.noErr,x.detected,x.undetected,x.falseAlarm,x.Kexp,x.Kth,x.PudExp,x.PudTh,x.ms].join(';'))).join('\n');
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([t],{type:'text/csv'}));a.download='lab8_results.csv';a.click();
}

function draw(){
  const cv=$('cv'),dpr=window.devicePixelRatio||1,W=cv.clientWidth,H=cv.clientHeight;
  cv.width=W*dpr;cv.height=H*dpr;const g=cv.getContext('2d');g.scale(dpr,dpr);g.clearRect(0,0,W,H);
  const L=58,R=14,T=12,B=40,x0=-3,x1=Math.log10(0.5),y0=-10,y1=0;
  const X=p=>L+(Math.log10(p)-x0)/(x1-x0)*(W-L-R),Y=u=>T+(y1-Math.log10(u))/(y1-y0)*(H-T-B);
  g.font='12px system-ui';g.strokeStyle='#e3e8ee';g.fillStyle='#6b7785';g.lineWidth=1;
  for(let e=y0;e<=y1;e+=1){const y=Y(Math.pow(10,e));g.beginPath();g.moveTo(L,y);g.lineTo(W-R,y);g.stroke();if(e%2===0)g.fillText('1e'+e,8,y+4)}
  [0.001,0.002,0.005,0.01,0.02,0.05,0.1,0.2,0.5].forEach(p=>{const x=X(p);g.beginPath();g.moveTo(x,T);g.lineTo(x,H-B);g.stroke();g.fillText(p,x-10,H-B+16)});
  g.fillText('p₀',W-R-16,H-6);
  if(theory){['P','W','C','I'].forEach(c=>{g.strokeStyle=COL[c];g.lineWidth=2.2;g.beginPath();let st=false;
    theory.p.forEach((p,i)=>{const u=theory[c].u[i];if(!(u>0))return;const y=Y(Math.max(u,1e-10));if(!st){g.moveTo(X(p),y);st=true}else g.lineTo(X(p),y)});g.stroke()})}
  hist.forEach(h=>{const u=h.Kexp==null?0:1-h.Kexp;if(!(u>0)||h.p<=0)return;g.fillStyle='#fff';g.strokeStyle=COL[h.code];g.lineWidth=2;
    g.beginPath();g.arc(X(h.p),Y(u),4.5,0,7);g.fill();g.stroke()});
  $('legend').innerHTML=['P','W','C','I'].map(c=>`<span><i style="background:${COL[c]}"></i>${NAMES[c]}${theory&&theory[c]?' ('+theory[c].n+','+theory[c].k+')':''}</span>`).join('');
}

document.querySelectorAll('#tabs button').forEach(b=>b.onclick=()=>{C.code=b.dataset.c;applyCode()});
$('apply').onclick=applyCode;$('enc').onclick=encode;
$('rnd').onclick=()=>{$('data').value=rndBits(info?info.k:5);encode()};
$('clr').onclick=()=>{if(!sent)return;recv=sent;renderBits();check()};
$('noise').onclick=()=>{if(!sent)return;const p=parseFloat($('p').value)||0;recv=[...sent].map(b=>Math.random()<p?(b==='1'?'0':'1'):b).join('');renderBits();check()};
$('run').onclick=()=>runExp([C.code]);$('all').onclick=()=>runExp(['P','W','C','I']);
$('csv').onclick=csv;$('hclr').onclick=()=>{hist=[];saveHist();renderHist()};
window.onresize=draw;
applyCode().then(renderHist);
</script></body></html>)rawliteral";
