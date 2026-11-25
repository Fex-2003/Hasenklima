#pragma once

namespace klima::web {

inline constexpr char page[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Hasenklima</title><link rel="stylesheet" href="/app.css"><script defer src="/app.js"></script>
</head>
<body>
<header><div class="frame top"><a class="brand" href="/">Hasenklima</a><nav aria-label="Hauptnavigation"><a href="/">Info</a><a href="/setup">Setup</a><a href="/debug">Debug</a></nav><span id="connection" class="status">Verbindung wird aufgebaut</span></div></header>
<main class="frame">
<section id="info" hidden>
  <h1>Klimaübersicht</h1><div class="notice" id="infoAlarm" role="status">Messwerte werden geladen.</div>
  <div class="columns">
    <article><h2>Bodenplatte</h2><p class="muted">Ansicht von oben, Eingang unten</p><div class="plate" id="plate"></div><dl id="plateStats"></dl></article>
    <article><h2>Betrieb</h2><dl id="operation"></dl><div id="remaining"></div></article>
  </div>
  <div class="climates" id="climates"></div>
  <article><h2>Temperaturverlauf</h2><div id="infoTrend"></div></article>
</section>
<section id="setup" hidden>
  <h1>Setup</h1>
  <form id="login" class="toolbar"><label>Gerätepasswort <input name="password" type="password" autocomplete="current-password" required></label><button>Anmelden</button></form>
  <div id="signedIn" class="toolbar" hidden><span>Bedienung freigegeben</span><button id="logout" type="button">Abmelden</button><button id="stop" class="danger" type="button">Hauptfunktionen AUS</button></div>
  <p id="message" role="status" aria-live="polite"></p>
  <details open><summary>Funktionsfreigaben</summary><div id="functions" class="toolbar"></div></details>
  <details><summary>Sensoren</summary><p>Plattenpositionen P1–P9: Ansicht von oben, Eingang unten. Zuordnung und Scan erfordern ausgeschaltete Hauptaktoren.</p><button id="scan" type="button">Sensorbusse scannen</button><p id="scanStatus"></p><div id="sensorConfig"></div></details>
  <details><summary>Regelung</summary><p>PI-Stellgröße ist der Tastgrad. Die Startwerte für Kp und Ti dienen der Entwicklung.</p><form id="controlForm"></form></details>
  <details><summary>Manuelle Bedienung</summary><dl id="manualState"></dl><form id="manualForm" class="fields">
    <label>Heiz-PWM (%)<input name="pwm" type="number" min="0" max="100" step="any" required value="0"></label>
    <label>Dauer (s)<input name="duration" type="number" min="60" max="3600" required></label>
    <label class="check"><input name="cooling" type="checkbox">Kühlpumpe</label>
    <label class="check"><input name="evaporation" type="checkbox">Verdunstungspumpe</label>
    <label class="check"><input name="override" type="checkbox">Taupunkt für diese Sitzung übergehen</label>
    <label class="check" id="extendChoice" hidden><input name="extend" type="checkbox">Laufzeit ausdrücklich verlängern</label>
    <div class="toolbar"><button id="manualSubmit">Sitzung starten</button><button id="manualStop" type="button">Sitzung stoppen</button></div>
  </form></details>
  <details><summary>Strom und Leistung</summary><dl id="powerState"></dl><form id="powerForm"></form></details>
  <details><summary>Energie</summary><dl id="energyState"></dl><form id="energyForm"></form><button id="resetEnergy" type="button">Energiezähler zurücksetzen</button></details>
  <details><summary>Gate-Treiber-Heizung</summary><dl id="driverState"></dl><form id="driverForm"></form></details>
  <details><summary>Sprungantwort</summary><p>Messraster 10 s. Ein gültiger neuer Start ersetzt den letzten Versuch. Versuche vor Ort und ohne Tiere beaufsichtigen.</p><form id="testForm" class="fields">
    <label>Vorlauf (min)<input name="baseline" type="number" min="0" max="60" required value="0"></label>
    <label>Heizphase (min)<input name="heat" type="number" min="1" max="500" required placeholder="nicht gesetzt"></label>
    <label>Nachlauf (min)<input name="cooldown" type="number" min="0" max="60" required value="0"></label>
    <label>Test-PWM (%)<input name="pwm" type="number" min="0" max="100" step="any" required></label>
    <div id="testPreview"></div><div class="toolbar"><button id="testStart">Start prüfen</button><button id="testStop" type="button">Versuch stoppen</button></div>
  </form><div id="testState"></div><a class="button" id="download" href="/api/v1/test.csv" hidden>CSV herunterladen</a></details>
  <details><summary>Verläufe</summary><div id="setupTrend"></div></details>
  <details><summary>MQTT</summary><p>Lokale Telemetrie. MQTT 3.1.1 über TCP.</p><form id="mqttForm"></form></details>
  <details id="faultSection"><summary>Fehler und Historie</summary><div id="faultState"></div></details>
  <details><summary>Netzwerk und Zugriff</summary><p id="networkState"></p><form id="networkForm"></form></details>
  <button id="reloadConfig" type="button">Gespeicherte Werte laden</button>
</section>
<section id="debug" hidden>
  <h1>Diagnose</h1><div class="columns" id="debugItems"></div>
</section>
</main>
</body></html>)HTML";

inline constexpr char style[] = R"CSS(
:root{font-family:system-ui,"Segoe UI",sans-serif;line-height:1.5;color:#22332b;background:#f4f6ef;--accent:#29634a;--edge:#d9e3d8;--white:#fff;--muted:#516457;--red:#a0352f;--orange:#8a5214;color-scheme:light}
*{box-sizing:border-box}body{margin:0}header{background:#fff;border-bottom:1px solid var(--edge)}main{padding-bottom:4rem}.frame{max-width:1120px;margin:auto;padding-inline:clamp(1rem,3vw,2rem)}.top{min-height:4rem;display:flex;align-items:center;gap:clamp(.7rem,3vw,2rem);flex-wrap:wrap;padding-block:.8rem}.brand{font-weight:750;color:#214e38;font-size:1.25rem;text-decoration:none}.top .status{margin-left:auto;color:var(--muted);font-size:.86rem}.top nav{display:flex;gap:.3rem}.top nav a{padding:.35rem .65rem;border-radius:.4rem;color:#284536;text-decoration:none}.top nav a[aria-current]{background:#e4efe5;color:#214e38;font-weight:650}
h1{font-size:clamp(1.8rem,5vw,2.65rem);letter-spacing:-.03em;margin:1.15rem 0 .65rem}h2{font-size:1.18rem;margin:.1rem 0 .65rem}p{margin:.55rem 0}.muted{color:var(--muted)}.columns{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:1rem;margin:1rem 0}.climates{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:1rem;margin:1rem 0}article,details,.notice,form#login{background:#fff;border:1px solid var(--edge);border-radius:.7rem;padding:1rem}details{margin:.65rem 0}summary{font-weight:700;cursor:pointer}details[open] summary{padding-bottom:.65rem}.notice{border-left:5px solid #41835d}.notice.warning{border-left-color:var(--orange)}.notice.error{border-left-color:var(--red)}.warning{color:var(--orange)}.error{color:var(--red)}.plate{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:.5rem;margin:.75rem 0}.sensor{background:#e9f2e9;border:1px solid #d6e5d7;border-radius:.5rem;padding:.45rem .35rem;text-align:center;min-width:0}.sensor b{display:block;font-size:1.12rem;font-variant-numeric:tabular-nums}.sensor small{display:block;font-size:.77rem}.sensor.error{background:#fff0ec;border-color:#ebc2b9}.sensor.disabled{background:#eff0ed;color:#555f56;border-color:#dfe2dc}.sensor.stale{background:#fff5dd;border-color:#f0dabc;color:#754b14}
dl{display:grid;grid-template-columns:minmax(7.5rem,auto) 1fr;gap:.25rem .7rem;margin:.55rem 0}dt{color:var(--muted)}dd{margin:0;overflow-wrap:anywhere;font-variant-numeric:tabular-nums}.toolbar{display:flex;gap:.5rem;align-items:center;flex-wrap:wrap}.fields{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(13rem,100%),1fr));gap:.65rem;margin:.65rem 0;align-items:end}.fields button{justify-self:start}label{display:flex;flex-direction:column;align-items:flex-start;gap:.25rem;font-size:.88rem}.check{flex-direction:row;align-items:center;gap:.45rem;min-height:2.5rem}.secret{display:flex;flex-direction:column;gap:.15rem}.row{display:grid;grid-template-columns:minmax(5rem,.5fr) minmax(11rem,1fr) minmax(5rem,.6fr) auto;gap:.55rem;align-items:end;padding:.5rem 0;border-bottom:1px solid var(--edge)}input,button,.button{font:inherit;border:1px solid #a9b9ad;border-radius:.4rem;min-height:2.4rem;padding:.4rem .55rem}input:not([type="checkbox"]){max-width:100%;width:100%;color:#22332b;background:#fff}label input[type="checkbox"]{min-height:auto;accent-color:var(--accent);width:1.2rem;height:1.2rem}button,.button{cursor:pointer;background:#e6efe6;color:#204934;font-weight:600;text-decoration:none;display:inline-flex;align-items:center;line-height:1.25}button:hover,.button:hover{background:#d9e9da}button.danger{background:#f9e6e1;border-color:#ac4e45;color:#8e2924}button:disabled{opacity:.58;cursor:default}button:focus-visible,input:focus-visible,a:focus-visible,summary:focus-visible{outline:2px solid #205a98;outline-offset:2px}.legend{display:flex;gap:1rem;flex-wrap:wrap;font-size:.86rem}.legend span{border-bottom:3px solid var(--ink,#29634a);padding-bottom:.1rem}canvas{display:block;width:100%;height:11.25rem}progress{width:100%;accent-color:var(--accent)}ul{padding-inline-start:1.35rem}li{margin:.28rem 0;overflow-wrap:anywhere}code{font-size:.88em;overflow-wrap:anywhere}#message{min-height:1.5rem}button [hidden],[hidden]{display:none!important}
@media(max-width:700px){.columns,.climates{grid-template-columns:1fr}.row{grid-template-columns:1fr 1fr}.row label:first-of-type{grid-column:span 2}.top .status{flex-basis:100%;margin-left:0}.plate{gap:.35rem}dl{grid-template-columns:minmax(7rem,.7fr) 1fr}}
)CSS";

inline constexpr char script[] = R"JS(
'use strict';
const $ = id => document.getElementById(id);
const node = (tag, value, cls) => {const el = document.createElement(tag); if (value !== undefined) el.textContent = value; if (cls) el.className = cls; return el;};
const page = location.pathname === '/setup' ? 'setup' : location.pathname === '/debug' ? 'debug' : 'info';
$(page).hidden = false;
for (const a of document.querySelectorAll('nav a')) if (a.pathname === location.pathname) a.setAttribute('aria-current', 'page');
const formatter = n => new Intl.NumberFormat('de-DE',{maximumFractionDigits:n,minimumFractionDigits:0});
const f1=formatter(1), f2=formatter(2), f3=formatter(3);
const finite = Number.isFinite;
const fmt = (x, unit='', prec=1) => finite(x) ? (prec===3?f3:prec===2?f2:f1).format(x) + (unit ? ' '+unit : '') : 'Nicht verfügbar';
const on = v => v ? 'Ein' : 'Aus';
const yes = v => v ? 'Ja' : 'Nein';
const age = s => s === null ? '–' : fmt(s, 'ms', 2);
const dur = v => {const sec=Math.max(0,Math.floor(v||0)); return [Math.floor(sec/3600),Math.floor(sec/60)%60,sec%60].map(x=>String(x).padStart(2,'0')).join(':');};
const modes={INITIALIZING:'Initialisierung',AUTO:'Automatik',MANUAL:'Manuell',IDENTIFICATION:'Sprungantwort',FAULT:'Störung',NEUTRAL:'Neutral',HEATING:'Heizen',COOLING:'Kühlen',EVAPORATING:'Verdunstung',COOLING_AND_EVAPORATING:'Kühlen und Verdunsten'};
const phases={IDLE:'Kein Versuch',PRECHECK:'Startprüfung',BASELINE:'Vorlauf',HEATING:'Heizphase',COOLDOWN:'Nachlauf',FINISHING:'Speichern',COMPLETE:'Abgeschlossen',ABORTED:'Abgebrochen'};
const reasons={NONE:'–',ABORTED_USER:'Durch Bedienung gestoppt',ABORTED_RESET:'Stromausfall oder Neustart',ABORTED_IO:'Schreibfehler',INCOMPLETE:'Unvollständige Aufzeichnung',TEST_INPUT_LIMIT:'PWM-Grenze unterschritten',HEATING_DISABLED:'Heizung deaktiviert',SAFETY:'Sicherheitsstopp',INVALID_READINGS:'Erforderlicher Messwert ungültig',RECORD_QUEUE_FULL:'Schreibpuffer voll',RECORD_DEADLINE_MISSED:'Messraster überschritten'};
const dewSources={ROOM:'Rückzugsraum',OUTSIDE:'Außenluft',FIXED_23:'Ersatzwert 23 °C'};
const inhibitLabels=['Plattengrenze fehlt','Plattensatz unvollständig','Störung','VIN nicht freigegeben','Heizung deaktiviert','Kühlung deaktiviert','Verdunstung deaktiviert','Vorwärmzeit','Außentemperatur fehlt','Kondensatgrenze','Verdunstungssensoren','Außentemperatur zu niedrig','Tankabstand','Strom-/PWM-Grenze','Heizung/Pumpen verriegelt','Sicherer Übergang','Konfiguration unvollständig'];
const inhibit = mask => inhibitLabels.filter((_,i)=>mask & (1<<i)).join(' · ') || 'Keine';
let latest, config, metadata, history, lastAt, zone='Europe/Berlin', ownTestPwm=false;
let session=sessionStorage.getItem('klima-session')||'';
const requestId=()=>Array.from(crypto.getRandomValues(new Uint8Array(16)),x=>x.toString(16).padStart(2,'0')).join('');
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function api(path, method='GET', data) {
    const headers={};
    if (session && method !== 'GET') headers['X-Klima-Session']=session;
    if (method !== 'GET') headers['Content-Type']='application/json';
    const abort=new AbortController(); const timer=setTimeout(()=>abort.abort(),5000);
    try {
        const res=await fetch('/api/v1'+path,{method,headers,body:method==='GET'?undefined:JSON.stringify(data||{}),signal:abort.signal,cache:'no-store'});
        const body=await res.json();
        if (!res.ok) {
            if (res.status===401 && path!=='/auth/login') setSession('');
            throw Error(body.message || `Anfrage fehlgeschlagen (${res.status})`);
        }
        return body;
    } finally { clearTimeout(timer); }
}
const errorText=e=>e instanceof TypeError || e.name==='AbortError' ? 'Gerät antwortet derzeit nicht. Gespeicherten Zustand nach der Wiederverbindung prüfen.' : e.message;
function show(text,error=false) { $('message').textContent=text; $('message').className=error?'error':''; }
function setSession(token) {
    session=token;
    if (token) sessionStorage.setItem('klima-session',token); else sessionStorage.removeItem('klima-session');
    if(page==='setup') {$('login').hidden=Boolean(token); $('signedIn').hidden=!token;}
}
async function change(path,method,body,{refresh=false,note=''}={}) {
    show('Auftrag wird gesendet …');
    try {
        let result=await api(path,method,body);
        if (result.state==='pending') {
            const deadline=Date.now()+20000;
            do {
                await sleep(200);
                result=await api('/operations/'+result.operation_id);
            } while (result.state==='pending' && Date.now()<deadline);
        }
        if (result.state==='pending') throw Error('Auftrag läuft noch. Status und gespeicherte Werte erneut prüfen.');
        if (result.state==='failed') throw Error(result.message||'Auftrag abgelehnt.');
        show((result.message||'Auftrag ausgeführt.')+(note?' '+note:''));
        if(refresh) await loadConfig();
        return result;
    } catch (e) { show(errorText(e),true); return null; }
}
function list(el, rows) {
    el.replaceChildren(...rows.flatMap(([label,value])=>[node('dt',label),node('dd',value)]));
}
function asDate(epoch) {
    const date=new Date(epoch*1000);
    try { return date.toLocaleString('de-DE',{timeZone:zone}); }
    catch (e) { return date.toLocaleString('de-DE'); }
}
function plateTiles(s) {
    const boxes=s.plate.map(p=>{
        const value=p.temperature_c;
        const cls=!p.enabled?'disabled':value.stale?'stale':!value.valid?'error':'';
        const el=node('div',undefined,'sensor '+cls);
        el.append(node('small','P'+p.position),node('b',fmt(value.value,'°C')));
        if(!p.enabled) el.append(node('small','Deaktiviert'));
        if(value.stale) el.append(node('small','Veraltet'));
        else if(!value.valid) el.append(node('small',p.rom_id?'Fehlerhaft':'Ohne Zuordnung'));
        return el;
    });
    $('plate').replaceChildren(...boxes);
    const p=s.plate_statistics;
    list($('plateStats'),[['Median',fmt(p.median_c,'°C')],['Mittelwert',fmt(p.mean_c,'°C')],['Min / Max',fmt(p.min_c,'°C')+' / '+fmt(p.max_c,'°C')],['Spreizung',fmt(p.spread_k,'K')],['Aktive Sensoren',p.enabled_count+' / 9']]);
}
function info(s) {
    const alarm=$('infoAlarm');
    const warning=s.faults.find(f=>f.active && f.severity==='WARNING');
    alarm.textContent=s.error_text || (warning&&warning.text) || 'Keine aktive Störung';
    alarm.className='notice '+(s.error_flag?'error':s.warning_flag?'warning':'');
    plateTiles(s);
    list($('operation'),[
        ['Betriebsart',modes[s.mode]||s.mode],['Automatikzustand',modes[s.auto_state]||s.auto_state],
        ['Heizung freigegeben',on(s.heater.enabled)],['Heiz-PWM Wunsch / Ausgabe',fmt(s.heater.pwm_requested_percent,'%')+' / '+fmt(s.heater.pwm_effective_percent,'%')],
        ['PWM-Grenze',fmt(s.heater.pwm_limit_percent,'%')],['Heizleistung, geschätzt',fmt(s.heater.power_est_w,'W')],
        ['Tank',fmt(s.tank_c.value,'°C')],['Kühlpumpe (Ausgang / Freigabe)',on(s.cooling_pump.commanded)+' / '+on(s.cooling_pump.enabled)],
        ['Verdunstung (Ausgang / Freigabe)',on(s.evaporation_pump.commanded)+' / '+on(s.evaporation_pump.enabled)],
        ['Taupunkt für die Regelung',fmt(s.dewpoint_control_c,'°C')+' · '+(dewSources[s.dewpoint_source]||s.dewpoint_source)],
        ['Feuchtkugel außen',fmt(s.wetbulb_outside_c,'°C')+(s.outside_pressure_fallback?' · Druckersatz 1013,25 hPa':'')],
        ['Kühlkapazität (theoretisch)',fmt(s.evaporation_capacity_k,'K')],['Sperrgründe',inhibit(s.heater.inhibit_reasons)],
        ['Gate-Treiber-Heizung',fmt(s.driver_heater.pwm_effective_percent,'%')],['VIN',fmt(s.vin_v.value,'V',2)+(s.vin_v.valid?(s.vin_calibrated?' · kalibriert':' · nominale Kalibrierung'):'')],
        ['MQTT',s.mqtt_online?'Verbunden':'Offline oder deaktiviert']]);
    const articles=[['Rückzugsraum',s.room],['Stall',s.stall],['Außen',s.outside]].map(([name,c])=>{
        const article=node('article'); const dl=node('dl'); article.append(node('h2',name),dl);
        const rows=[['Temperatur',fmt(c.temperature_c.value,'°C')],['Relative Feuchte',fmt(c.humidity_percent.value,'%')],['Taupunkt (berechnet)',fmt(c.dewpoint_c.value,'°C')]];
        if(c.pressure_pa) rows.push(['Druck',fmt(finite(c.pressure_pa.value)?c.pressure_pa.value/100:null,'hPa')]);
        list(dl,rows); return article;
    });
    $('climates').replaceChildren(...articles);
    $('remaining').textContent=s.identification.running ? (phases[s.identification.phase]||s.identification.phase)+' · Restzeit '+dur(s.identification.remaining_s) :
        s.manual.session_id ? 'Manuelle Sitzung · Restzeit '+dur(s.manual.remaining_s) : '';
    if(s.identification.running) {
        const p=node('progress'); p.max=Math.max(1,s.identification.elapsed_s+s.identification.remaining_s);p.value=s.identification.elapsed_s; $('remaining').append(p);
    }
}
function trend(root, data, channels) {
    if(!data || !data.points.length) { root.replaceChildren(node('p','Seit dem Neustart sind noch keine Verlaufspunkte vorhanden.','muted')); return; }
    const points=data.points;
    const legend=node('div',undefined,'legend');
    const canvas=node('canvas'); canvas.setAttribute('role','img'); canvas.setAttribute('aria-label','Temperaturverlauf in Grad Celsius');
    const colors=['#29634a','#356eac','#a15a22'];
    channels.forEach(([label],i)=>{const span=node('span',label);span.style.setProperty('--ink',colors[i]);legend.append(span);});
    const first=points[0][0], last=points[points.length-1][0];
    const duration=Math.round(Math.max(0,last-first)/1000000);
    const endEpoch=finite(data.utc_s)?data.utc_s-(data.uptime_us-last)/1000000:null;
    const note=finite(endEpoch)?asDate(endEpoch-duration)+' bis '+asDate(endEpoch):'Letzter Messpunkt bei '+dur(last/1e6)+' Laufzeit';
    root.replaceChildren(legend,canvas,node('p','Zeitraum '+dur(duration)+' · '+note,'muted'));
    const width=canvas.clientWidth||320, height=180, ratio=Math.min(2,window.devicePixelRatio||1);
    canvas.width=Math.round(width*ratio);canvas.height=height*ratio;
    const ctx=canvas.getContext('2d'); if(!ctx) return; ctx.scale(ratio,ratio);
    const values=points.flatMap(p=>channels.map(([,i])=>p[i]).filter(finite));
    if(!values.length) {ctx.fillStyle='#516457';ctx.fillText('Keine gültigen Messwerte im Zeitraum',14,25);return;}
    const low=Math.floor(Math.min(...values))-1, high=Math.ceil(Math.max(...values))+1;
    const top=9, bottom=height-15, left=34, right=width-12;
    ctx.fillStyle='#516457'; ctx.font='11px system-ui'; ctx.strokeStyle='#dce4dc';
    for(let j=0;j<=4;j++){
        const y=bottom-j*(bottom-top)/4;
        ctx.fillText(f1.format(low+j*(high-low)/4)+'°',0,y+4);
        ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();
    }
    channels.forEach(([,i],c)=>{
        const xOf=p=>first===last ? right : left+(p[0]-first)*(right-left)/(last-first);
        const yOf=p=>bottom-(p[i]-low)*(bottom-top)/(high-low);
        ctx.strokeStyle=ctx.fillStyle=colors[c];ctx.lineWidth=2;ctx.beginPath();let joined=false;
        for(const p of points) {
            if(!finite(p[i])) {joined=false;continue;}
            if(joined)ctx.lineTo(xOf(p),yOf(p));else ctx.moveTo(xOf(p),yOf(p));joined=true;
        }
        ctx.stroke();
        for(const p of points) if (finite(p[i])) {ctx.beginPath();ctx.arc(xOf(p),yOf(p),1.6,0,Math.PI*2);ctx.fill();}
    });
}
function renderHistory() {
    if(page==='info') trend($('infoTrend'),history,[['Rückzugsraum',1],['Außen',2]]);
    if(page==='setup' && $('setupTrend').closest('details').open) trend($('setupTrend'),history,[['Tank',3],['Plattenmedian',4],['Plattenmittelwert',5]]);
}
const sections={
    controlForm:{heating_target_c:'Heizsollwert (Platte)',cooling_target_c:'Kühlsollwert (Platte)',cooling_hysteresis_k:'Kühlhysterese',cooling_min_outside_c:'Mindesttemperatur außen',dewpoint_margin_k:'Taupunktabstand',tank_plate_margin_k:'Tank-Platten-Abstand',evaporation_min_capacity_k:'Mindestkühlkapazität',evaporation_hysteresis_k:'Verdunstungshysterese',pi_kp_duty_per_k:'PI Kp (Tastgrad pro Kelvin)',pi_ti_s:'PI Nachstellzeit',pi_integral_enabled:'I-Anteil aktiv',pi_update_interval_ms:'PI-Intervall',overtemp_release_hysteresis_k:'Freigabehysterese nach Übertemperatur',sensor_max_age_ms:'Maximales Messalter',vin_max_age_ms:'Maximales VIN-Messalter',manual_timeout_s:'Manuelle Standardlaufzeit',plate_spread_warning_k:'Warnung bei Plattenspreizung',plate_change_warning_k_per_s:'Warnung bei schneller Temperaturänderung'},
    powerForm:{heater_resistance_ohm:'Gemessener Heizdrahtwiderstand',supply_rated_current_a:'Netzteilnennstrom AIN_max',heater_input_current_limit_a:'Mittlerer Heizstromanteil',vin_valid_min_v:'Minimale zulässige VIN',vin_valid_max_v:'Maximale zulässige VIN',vin_gain:'VIN-Kalibrierfaktor',vin_offset_v:'VIN-Kalibrieroffset',vin_calibration_confirmed:'VIN-Messkalibrierung bestätigt'},
    energyForm:{energy_checkpoint_s:'Abstand der Energie-Checkpoints'},
    driverForm:{driver_heater_enabled:'Gate-Treiber-Heizung freigeben',driver_heater_on_temp_c:'T_ein (Außentemperatur)',driver_heater_slope_pct_per_k:'Kennliniensteigung',driver_warmup_s:'Vorwärmzeit'},
    mqttForm:{mqtt_enabled:'MQTT aktivieren',mqtt_host:'Broker',mqtt_port:'Port',mqtt_user:'Benutzername',mqtt_password:'MQTT-Passwort',mqtt_client_id:'Client-ID (leer: automatisch)',mqtt_prefix:'Topic-Präfix',mqtt_interval_s:'Veröffentlichungsintervall'},
    networkForm:{wifi_ssid:'WLAN-SSID',wifi_password:'WLAN-Passwort',wifi_ip:'Statische IPv4-Adresse',wifi_mask:'Netzmaske',wifi_gateway:'Gateway',wifi_dns:'DNS-Server',device_password:'Neues Gerätepasswort',ntp_server:'NTP-Server (optional)',timezone:'Anzeigezeitzone'}
};
function inputFor(form,id,label,p) {
    const inp=document.createElement('input');inp.name=id;
    const stored=config.values[id];
    const wrapper=node('label',label+(p.unit?' ('+p.unit+')':''));
    if(p.type==='bool') {wrapper.className='check';inp.type='checkbox';inp.checked=Boolean(stored);wrapper.prepend(inp);}
    else if(p.type==='string') {
        inp.type=p.secret?'password':'text';inp.autocomplete=p.secret?'new-password':'off';inp.maxLength=p.max_length;
        if(p.secret) {inp.placeholder=config.secrets_set[id]?'Gesetzt · leer lassen zum Beibehalten':'Nicht gesetzt'; if(id==='device_password')inp.minLength=12;}
        else inp.value=stored??'';
        wrapper.append(inp);
    } else {
        inp.type='number';inp.step=p.type==='uint'?'1':'any';inp.min=p.minimum;inp.max=p.maximum;
        inp.value=stored??''; inp.required=!p.allow_unset;
        if(p.allow_unset) inp.placeholder='Nicht gesetzt'; wrapper.append(inp);
    }
    if(p.secret && id!=='device_password' && config.secrets_set[id]) {
        const field=node('div',undefined,'secret');field.append(wrapper);
        const clear=node('label','Gespeichertes Passwort entfernen','check'); const c=document.createElement('input');c.type='checkbox';c.name=id+'_clear';clear.prepend(c);field.append(clear); form.append(field);
    } else form.append(wrapper);
}
function generateForms() {
    const registry=Object.fromEntries(metadata.parameters.map(x=>[x.id,x]));
    const revision=config.revision, values={...config.values};
    for(const [formId, group] of Object.entries(sections)) {
        const form=$(formId);form.className='fields';form.replaceChildren();
        for(const [key,label] of Object.entries(group)) if(registry[key]) inputFor(form,key,label,registry[key]);
        const submit=node('button','Speichern'); form.append(submit);
        form.onsubmit=async e=>{
            e.preventDefault();
            const changes={};
            for(const key of Object.keys(group)) {
                const p=registry[key], input=form.elements[key];if(!input)continue;
                if(p.secret){const clear=form.elements[key+'_clear'];if(clear?.checked)changes[key]='';else if(input.value)changes[key]=input.value;continue;}
                const value=p.type==='bool'?input.checked:p.type==='string'?input.value:input.value===''?null:input.valueAsNumber;
                if(value!==values[key]) changes[key]=value;
            }
            if(!Object.keys(changes).length) {show('Keine Änderung.'); return;}
            submit.disabled=true;
            const newIp=changes.wifi_ip||values.wifi_ip;
            await change('/config','PATCH',{expected_revision:revision,request_id:requestId(),values:changes},
                {refresh:true,note:Object.keys(changes).some(k=>k.startsWith('wifi_')) ? 'WLAN wird umgestellt. Danach '+(newIp?'http://'+newIp+' öffnen.':'die gespeicherte Adresse öffnen.') : ''});
            submit.disabled=false;
        };
    }
    if(!$('manualForm').elements.duration.value) $('manualForm').elements.duration.value=config.values.manual_timeout_s;
}
function sensorRows() {
    const el=$('sensorConfig');el.replaceChildren();
    const revision=config.revision;
    const plateList=node('datalist');plateList.id='plateRoms';
    const tankList=node('datalist');tankList.id='tankRoms';
    el.append(plateList,tankList);
    const row=(index, p)=>{
        const tank=index===0;
        const form=node('form',undefined,'row');
        const title=node('strong',tank?'Tank':'P'+index);
        const value=node('small','–');value.id=tank?'tankSetupValue':'sensorSetupValue'+index;const col=node('div');col.append(title,node('br'),value);
        const label=node('label','ROM-ID');const rom=document.createElement('input');rom.name='rom';rom.value=p.rom_id||'';rom.maxLength=16;rom.pattern='[0-9A-Fa-f]{16}';rom.setAttribute('list',tank?'tankRoms':'plateRoms');rom.autocomplete='off';label.append(rom);
        const active=node('label',tank?'Tankbus':'Aktiv','check');const cb=document.createElement('input');cb.type='checkbox';cb.checked=p.enabled;
        if(!tank)active.prepend(cb);
        const button=node('button','Übernehmen');form.append(col,label,active,button);el.append(form);
        form.onsubmit=async e=>{
            e.preventDefault();
            const disable=!tank && p.enabled&&!cb.checked;
            if(disable&&!confirm('P'+index+' ('+(p.rom_id||'keine ROM-ID')+') deaktivieren? An dieser Position entfällt die Temperaturüberwachung.'))return;
            button.disabled=true;
            const body={rom_id:rom.value.toUpperCase()||null,expected_revision:revision,request_id:requestId()};
            if(!tank)Object.assign(body,{enabled:cb.checked,confirm_disable:disable});
            await change(tank?'/sensors/tank':'/sensors/plate/'+index,'PUT',body,{refresh:true});
            button.disabled=false;
        };
    };
    config.plate.forEach((p,index)=>row(index+1,p));
    row(0,{rom_id:config.tank_rom_id,enabled:true});
}
async function readScan() {
    try {
        const scan=await api('/sensors/scan');
        if(!scan.ready) {$('scanStatus').textContent='Scanergebnis noch nicht verfügbar.';return;}
        const unassigned=scan.plate.filter(rom=>!config.plate.some(p=>p.rom_id===rom));
        $('scanStatus').textContent='Plattenbus: '+(scan.plate.join(', ')||'keine Sensoren')+'. Nicht zugeordnet: '+(unassigned.join(', ')||'keine')+'. Tankbus: '+(scan.tank.join(', ')||'keine Sensoren')+(scan.overflow?' · Scanliste begrenzt.':'.');
        for(const [list,ids] of [[$('plateRoms'),scan.plate],[$('tankRoms'),scan.tank]]){
            if(!list)continue;list.replaceChildren(...ids.map(rom=>{const opt=node('option');opt.value=rom;return opt;}));
        }
    } catch(e){$('scanStatus').textContent=errorText(e);}
}
async function loadConfig() {
    config=await api('/config');zone=config.values.timezone||zone;
    if(page==='setup') {
        if(!metadata)metadata=await api('/parameters');generateForms();sensorRows();readScan();
        if(latest)setup(latest);
    }
}
function quantized(p) {
    if (!finite(p) || p<=0) return 0;
    if(p>=100)return 100;
    return Math.min(1022,Math.floor(Math.fround(Math.fround(p)/100)*1024))*100/1024;
}
function testPreview() {
    const form=$('testForm');const p=form.elements.pwm.valueAsNumber;
    const minutes=['baseline','heat','cooldown'].reduce((acc,x)=>acc+(form.elements[x].valueAsNumber||0),0);
    const max=latest?.heater.pwm_limit_percent;
    $('testPreview').textContent='Geplant '+minutes+' min · programmierbar '+fmt(quantized(p),'%',3)+' · derzeitige Grenze '+fmt(max,'%',3);
}
function setup(s) {
    $('functions').replaceChildren(...[
        ['heating_enabled','Heizung',s.heater.enabled],['cooling_enabled','Plattenkühlung',s.cooling_pump.enabled],['evaporation_enabled','Verdunstung',s.evaporation_pump.enabled]
    ].map(([key,label,state])=>{
        const button=node('button',label+' '+on(state));button.type='button';button.setAttribute('aria-pressed',String(state));
        button.onclick=async()=>{button.disabled=true;await change('/functions','POST',{[key]:!state,expected_revision:s.config_revision,request_id:requestId()});button.disabled=false;};return button;
    }));
    for(const p of s.plate){const el=$('sensorSetupValue'+p.position); if(el)el.textContent=fmt(p.temperature_c.value,'°C')+(p.temperature_c.stale?' · veraltet':'');}
    if($('tankSetupValue'))$('tankSetupValue').textContent=fmt(s.tank_c.value,'°C');
    list($('manualState'),[['Sitzung',s.manual.session_id?String(s.manual.session_id):'Keine'],['Restzeit',dur(s.manual.remaining_s)],['Taupunkt übergangen',yes(s.manual.condensation_override)],['Heiz-PWM Wunsch / Ausgabe',fmt(s.heater.pwm_requested_percent,'%')+' / '+fmt(s.heater.pwm_effective_percent,'%')],['PWM-Grenze',fmt(s.heater.pwm_limit_percent,'%',3)],['Pumpe Kühlung / Verdunstung',on(s.cooling_pump.commanded)+' / '+on(s.evaporation_pump.commanded)],['Sperrgründe',inhibit(s.heater.inhibit_reasons)]]);
    const manual=$('manualForm');const active=Boolean(s.manual.session_id);manual.elements.duration.disabled=active&&!manual.elements.extend.checked;manual.elements.override.disabled=active;
    $('extendChoice').hidden=!active;
    if(!active)manual.elements.extend.checked=false;
    $('manualSubmit').textContent=active?'Ausgänge ändern':'Sitzung starten';$('manualStop').disabled=!active;
    list($('powerState'),[['VIN',fmt(s.vin_v.value,'V',2)+(s.vin_v.valid?(s.vin_calibrated?' · kalibriert':' · Kalibrierung unbestätigt'):'')],['Nominelle Stromreserve',fmt(s.heater.reserve_a,'A',2)+(s.heater.reserve_a===0?' · Keine Reserve für Nebenlasten':'')],['Mittlerer Heizstrom, geschätzt',fmt(s.heater.current_est_a,'A',2)],['Heizleistung, geschätzt',fmt(s.heater.power_est_w,'W')],['Leistungsgrenze, geschätzt',fmt(s.heater.power_limit_est_w,'W')],['Tastgradgrenze',fmt(s.heater.pwm_limit_percent,'%',3)]]);
    list($('energyState'),[['Heizdrahtenergie, geschätzt',fmt(s.heater.energy_est_wh,'Wh',3)+' / '+fmt(s.heater.energy_est_wh/1000,'kWh',3)],['Letzter Checkpoint',s.heater.energy_checkpoint_us?'bei '+dur(s.heater.energy_checkpoint_us/1e6)+' Laufzeit':'Keiner in diesem Boot'],['Checkpointstatus',s.heater.energy_checkpoint_error?'Fehler':'Kein Schreibfehler gemeldet'],['Messlücken',s.heater.energy_incomplete?'Schätzung unvollständig':'Keine erkannten Lücken'],['Möglicher Verlust bei Stromausfall',config?.values.energy_checkpoint_s+' s seit letztem Checkpoint; bei Schreibfehlern länger']]);
    list($('driverState'),[['Aktueller Tastgrad',fmt(s.driver_heater.pwm_effective_percent,'%')],['Außentemperatur',fmt(s.outside.temperature_c.value,'°C')],['Kennlinie gültig',yes(s.driver_heater.curve_valid)],['Vorwärmung',s.driver_heater.warming?'Aktiv':'Nicht aktiv']]);
    const t=s.identification;
    const summary=node('dl');
    list(summary,[['Test-ID',t.test_id||'–'],['Status',phases[t.phase]||t.phase],['Abbruchgrund',reasons[t.reason]||t.reason||'–'],['Testlaufzeit / Rest',dur(t.elapsed_s)+' / '+dur(t.remaining_s)],['Gültige / dauerhaft gespeicherte Punkte',t.points+' / '+t.durable_points],['Phasen (Vorlauf / Heizphase / Nachlauf)',[t.baseline_minutes,t.heat_minutes,t.cooldown_minutes].join(' / ')+' min'],['Test-PWM Wunsch / bestätigt',fmt(t.pwm_percent,'%',3)+' / '+fmt(t.confirmed_pwm_percent,'%',3)],['Start',t.start_utc_s?asDate(t.start_utc_s):'Laufzeit '+dur(t.start_uptime_us/1e6)],['PWM ein / aus / Ende',fmt(t.heat_start_ms,'ms')+' / '+fmt(t.heat_stop_ms,'ms')+' / '+fmt(t.end_ms,'ms')],['Boot und Revision',(t.boot_id||'–')+' / '+t.config_revision],['Aktivierungsmaske und ROMs',t.enabled_mask.toString(16)+' / '+t.plate_rom_ids.map(x=>x||'–').join(', ')],['R / Stromgrenze / Plattengrenze',fmt(t.heater_resistance_ohm,'Ω')+' / '+fmt(t.current_limit_a,'A')+' / '+fmt(t.max_plate_temp_c,'°C')],['Grenzwert beim Abbruch',fmt(t.trigger)],['Recorder',t.io_error?'Dateifehler':t.available?'Verfügbar':'Gesperrt']]);
    const target=$('testState');target.replaceChildren(summary);
    if(t.test_id){
        const more=node('details'), table=node('dl');more.append(node('summary','Konfiguration beim Teststart'),table);
        list(table,[['Firmware / Schema',t.firmware_version+' / '+t.schema_version],['Kalenderzeit synchronisiert',yes(t.utc_synchronized)],['VIN min / max',fmt(t.vin_valid_min_v,'V')+' / '+fmt(t.vin_valid_max_v,'V')],['VIN Faktor / Offset',fmt(t.vin_gain,'',3)+' / '+fmt(t.vin_offset_v,'V',3)],['Netzteilnennstrom',fmt(t.supply_rated_current_a,'A')],['PI Kp / Ti',fmt(t.pi_kp_duty_per_k,'1/K',3)+' / '+fmt(t.pi_ti_s,'s')],['PI-Intervall / I-Anteil',fmt(t.pi_interval_ms,'ms')+' / '+on(t.pi_integral_enabled)],['Heiz- / Kühlsollwert',fmt(t.heating_target_c,'°C')+' / '+fmt(t.cooling_target_c,'°C')],['Verlorene ungesicherte Punkte',t.unflushed_loss===null?'Unbekannt':t.unflushed_loss],['Maximales Schreibintervall',fmt(t.max_flush_interval_s,'s')],['Reihenfolge der Messpunkte',t.sample_order]]);
        const prior=target.previousSnapshotDetailsOpen===true;
        more.open=prior;
        more.ontoggle=()=>{target.previousSnapshotDetailsOpen=more.open;};
        target.append(more);
    }
    if(t.running){const progress=node('progress');progress.max=Math.max(1,t.elapsed_s+t.remaining_s);progress.value=t.elapsed_s;target.prepend(progress);}
    $('download').hidden=t.running||!t.test_id; $('testStop').disabled=!t.running;$('testStart').disabled=t.running||!t.available||t.download_active;
    if(!ownTestPwm){const input=$('testForm').elements.pwm;input.value=s.heater.pwm_limit_percent>=12.5?'12.5':'';}
    testPreview();
    $('networkState').textContent=(s.wifi_online?'WLAN verbunden':'WLAN offline')+(config?.values.wifi_ip?' · '+config.values.wifi_ip:'')+' · MQTT '+(config?.values.mqtt_enabled?(s.mqtt_online?'verbunden':'offline'):'deaktiviert');
    if(!$('faultSection').open) renderFaults(s.faults, null);
}
const severityNames={INFO:'Hinweis',WARNING:'Warnung',ERROR:'Fehler',CRITICAL:'Kritisch'};
function renderFaults(current, past) {
    const output=$('faultState');output.replaceChildren();
    const render=(target,items,canAck)=>{
        const listEl=node('ul');
        for(const fault of items){
            const cls=fault.severity==='CRITICAL'||fault.severity==='ERROR'?'error':fault.severity==='WARNING'?'warning':'';
            const item=node('li',undefined,cls);
            const title=node('strong',(severityNames[fault.severity]||fault.severity)+' · '+(fault.text||fault.code));
            const state=(fault.active?'Aktiv':fault.latched?'Ursache behoben, Quittierung offen':'Inaktiv');
            const detail=node('small',fault.code+' · '+state+' · Erst/Letzt '+dur(fault.first_uptime_us/1e6)+' / '+dur(fault.last_uptime_us/1e6)+' · '+fault.repeats+' Ereignis(se) · Sperren: '+inhibit(fault.inhibit_mask));
            item.append(title,node('br'),detail);
            if(canAck && fault.latched){
                const ack=node('button','Quittieren');ack.type='button';ack.disabled=fault.active;
                ack.onclick=async()=>{await change('/faults/'+encodeURIComponent(fault.code)+'/ack','POST',{});loadFaults();};
                item.append(node('br'),ack);
            }
            listEl.append(item);
        }
        target.append(items.length?listEl:node('p','Keine Einträge.','muted'));
    };
    output.append(node('h2','Aktuelle Meldungen'));render(output,current,true);
    if(past){output.append(node('h2','Verlauf seit Neustart'));render(output,past,false);}
}
let faultLoading=false;
async function loadFaults() {
    if(faultLoading||!$('faultSection').open)return;
    faultLoading=true;
    try {const data=await api('/faults'); if($('faultSection').open)renderFaults(data.current,data.history);}
    catch(e){$('faultState').replaceChildren(node('p',errorText(e),'error'));}
    finally {faultLoading=false;}
}
function installSetup() {
    setSession(session);
    $('login').onsubmit=async e=>{
        e.preventDefault();
        const password=e.target.elements.password;
        try {const reply=await api('/auth/login','POST',{password:password.value}); setSession(reply.session_token); show('Angemeldet.');}
        catch (err) {show(errorText(err),true);}
        finally {password.value='';}
    };
    $('logout').onclick=async()=>{try{await api('/auth/logout','POST',{});}catch(e){show(errorText(e),true);}setSession('');};
    $('stop').onclick=()=>change('/stop','POST',{}, {refresh:true});
    $('reloadConfig').onclick=()=>loadConfig().then(()=>show('Gespeicherte Werte geladen.')).catch(e=>show(errorText(e),true));
    $('scan').onclick=async()=>{if(await change('/sensors/scan','POST',{}))readScan();};
    $('manualStop').onclick=()=>change('/manual/stop','POST',{});
    $('manualForm').elements.extend.onchange=e=>{e.target.form.elements.duration.disabled=!e.target.checked;};
    $('manualForm').onsubmit=e=>{
        e.preventDefault();if(!latest)return;
        const fields=e.target.elements, active=latest.manual.session_id;
        const p=fields.pwm.valueAsNumber;
        if(p>latest.heater.pwm_limit_percent){show('Heiz-PWM überschreitet die aktuelle Grenze von '+fmt(latest.heater.pwm_limit_percent,'%',3)+'.',true);return;}
        if(!active&&!confirm('Manuelle Sitzung für '+fields.duration.value+' s starten?'+(fields.override.checked?' Die Taupunktprüfung wird übergangen.':'')))return;
        const body={heater_pwm_percent:p,cooling:fields.cooling.checked,evaporation:fields.evaporation.checked,expected_revision:latest.config_revision,request_id:requestId()};
        if(active){
            body.session_id=active;
            if(fields.extend.checked){
                if(!confirm('Manuelle Sitzung mit einer neuen Frist von '+fields.duration.value+' s ab jetzt verlängern?'))return;
                body.duration_seconds=fields.duration.valueAsNumber;
                fields.extend.checked=false;
            }
        }
        else Object.assign(body,{duration_s:fields.duration.valueAsNumber,ignore_dewpoint:fields.override.checked});
        change('/manual/'+(active?'output':'start'),'POST',body);
    };
    $('resetEnergy').onclick=()=>{if(confirm('Installationszähler für die geschätzte Heizenergie dauerhaft auf 0 setzen?'))change('/energy/reset','POST',{confirmed:true});};
    $('testForm').oninput=e=>{if(e.target.name==='pwm')ownTestPwm=true;testPreview();};
    $('testForm').onsubmit=e=>{
        e.preventDefault();if(!latest)return;
        const fields=e.target.elements;
        const base=fields.baseline.valueAsNumber,heat=fields.heat.valueAsNumber,cool=fields.cooldown.valueAsNumber,pwm=fields.pwm.valueAsNumber;
        const maximum=latest.heater.pwm_limit_percent, effective=quantized(pwm);
        if(!finite(pwm)||pwm<=0||pwm>maximum||effective<=0){show('Test-PWM liegt außerhalb der aktuellen Grenze. Maximal '+fmt(maximum,'%',3)+'.',true);return;}
        if(!confirm('Sprungantwort starten?\nVorlauf / Heizphase / Nachlauf: '+base+' / '+heat+' / '+cool+' min\nPWM Wunsch / programmierbar: '+fmt(pwm,'%',3)+' / '+fmt(effective,'%',3)+'\nDer letzte Versuch wird nach erfolgreicher Startprüfung ersetzt.'))return;
        change('/test/start','POST',{baseline_minutes:base,heat_minutes:heat,cooldown_minutes:cool,pwm_percent:pwm,replace_previous_test:true,expected_config_revision:latest.config_revision,request_id:requestId()});
    };
    $('testStop').onclick=()=>change('/test/stop','POST',{});
    $('faultSection').ontoggle=()=>loadFaults();
    $('setupTrend').closest('details').ontoggle=()=>renderHistory();
}
const resetNames={0:'Unbekannt',1:'Einschalten',2:'Externer Reset',3:'Software',4:'Panik',5:'Interrupt-Watchdog',6:'Task-Watchdog',7:'Sonstiger Watchdog',8:'Deep Sleep',9:'Brownout',10:'SDIO'};
function debug(d) {
    const articles=[];
    const block=(title,items)=>{const article=node('article'), dl=node('dl');article.append(node('h2',title),dl);list(dl,items);articles.push(article);};
    block('System',[
        ['Firmware',d.firmware_version],['Arduino-ESP32',d.arduino_core],['ESP-IDF',d.idf_version],['Boot-ID / Sequenz',d.boot_id+' / '+d.snapshot_sequence],
        ['Laufzeit',dur(d.uptime_us/1e6)],['Kalenderzeit',d.utc_s?asDate(d.utc_s):'Nicht synchronisiert'],['Resetgrund',resetNames[d.reset_reason]||d.reset_reason],['Neustartzähler',d.reboot_count],
        ['WLAN / MQTT',on(d.wifi_online)+' / '+on(d.mqtt_online)],['IP',d.wifi_ip],['HTTP / CSV bereit',yes(d.http_ready)+' / '+yes(d.csv_ready)]]);
    block('Hardware',[['Feste Plattengrenze',fmt(d.max_plate_temp_c,'°C')],['Heiz-PWM',fmt(d.heater_pwm_hz,'Hz')+' / '+d.heater_pwm_bits+' Bit'],
        ...Object.entries({heater:'Heizung',cooling:'Kühlpumpe',evaporation:'Verdunstung',driver_heater:'Gate-Treiber-Heizung',plate_bus:'Plattenbus',tank_bus:'Tankbus',vin_adc:'VIN ADC',i2c_sda:'I²C SDA',i2c_scl:'I²C SCL'}).map(([key,name])=>[name,'GPIO '+d.pins[key]])]);
    block('Speicher',[[ 'Flash',fmt(d.flash_bytes,'Byte')],['Firmware',fmt(d.firmware_bytes,'Byte')],['App-Partition',fmt(d.app_partition_bytes,'Byte')],['LittleFS-Partition',fmt(d.filesystem_partition_bytes,'Byte')],['LittleFS gemountet',yes(d.filesystem_mounted)],['Freier Heap',fmt(d.free_heap_bytes,'Byte')],['Größter freier Block',fmt(d.largest_heap_block_bytes,'Byte')],['Minimaler freier Heap',fmt(d.minimum_free_heap_bytes,'Byte')]]);
    block('Laufzeiten und Warteschlangen',[
        ['Watchdog',on(d.watchdog_active)],['Größter Abstand Steuerung',fmt(d.max_control_gap_us,'µs')],['Maximale Laufzeit Steuerung',fmt(d.max_control_run_us,'µs')],
        ['Sensor / Netzwerk / Speicher', [d.max_sensor_run_us,d.max_network_run_us,d.max_storage_run_us].map(v=>fmt(v,'µs')).join(' / ')],['Längste Flashoperation',fmt(d.max_flash_operation_us,'µs')],['Höchstes Sensoralter',fmt(d.max_sensor_age_ms,'ms')],
        ['Stackreserve Steuerung / Sensor',d.control_stack_free_bytes+' / '+d.sensor_stack_free_bytes+' Byte'],['Stackreserve Netzwerk / Speicher',d.network_stack_free_bytes+' / '+d.storage_stack_free_bytes+' Byte'],['Queue Befehle / Testpunkte',d.command_queue_used+' / '+d.record_queue_used],['Verworfene MQTT-Nachrichten',d.dropped_mqtt_telemetry]]);
    const m=d.measurement_age_ms;
    block('Regelung',[['Aktivierungsmaske',d.plate_enabled_mask.toString(2).padStart(9,'0')],['PI-Integrator',fmt(d.pi_integrator,'',3)],['PWM-Grenze',fmt(d.pwm_limit_percent,'%',3)],['Sperrgründe',inhibit(d.inhibit_reasons)]]);
    block('Messalter',[
        ...m.plate.map((v,i)=>['P'+(i+1),age(v)]),['Tank / VIN',age(m.tank)+' / '+age(m.vin)],['Raumtemperatur / Feuchte',age(m.room_temperature)+' / '+age(m.room_humidity)],
        ['Stalltemperatur / Feuchte / Druck',[m.stall_temperature,m.stall_humidity,m.stall_pressure].map(age).join(' / ')],
        ['Außentemperatur / Feuchte / Druck',[m.outside_temperature,m.outside_humidity,m.outside_pressure].map(age).join(' / ')]]);
    $('debugItems').replaceChildren(...articles);
}
let historyPending=false;
async function loadHistory() {
    if(historyPending||page==='debug')return;historyPending=true;
    try {history=await api('/history');renderHistory();}
    catch(e){if(!history){const root=$(page==='setup'?'setupTrend':'infoTrend');root.replaceChildren(node('p',errorText(e),'muted'));}}
    finally {historyPending=false;}
}
async function poll() {
    let interval=2000;
    try {
        if(page==='debug')debug(await api('/debug'));
        else {
            const s=await api('/snapshot');
            if(!latest||latest.boot_id!==s.boot_id) {history=null;renderHistory();loadHistory();}
            latest=s;
            if(page==='info')info(s);else setup(s);
            if(s.identification.running) interval=1000;
        }
        lastAt=new Date();
        $('connection').textContent='Aktuell · '+lastAt.toLocaleTimeString('de-DE');
        $('connection').className='status';
        $(page).classList.remove('stale-page');
    } catch(e){
        $('connection').textContent='Verbindung unterbrochen'+(lastAt?' · letzte Aktualisierung '+lastAt.toLocaleTimeString('de-DE'):'');
        $('connection').className='status error';
        $(page).classList.add('stale-page');
        if(page==='info')$('infoAlarm').textContent='Gerät antwortet nicht. Angezeigte Werte sind veraltet.';
    } finally {setTimeout(poll,interval);}
}
if(page==='setup') {
    installSetup();loadConfig().catch(e=>show(errorText(e),true));
    setInterval(loadFaults,12000);
} else loadConfig().catch(()=>{});
if(page!=='debug') {window.addEventListener('resize', renderHistory);setInterval(loadHistory,60000);}
poll();
)JS";

} // namespace klima::web


