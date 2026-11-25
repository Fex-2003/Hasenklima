const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {webcrypto}=require('node:crypto');
const path=require('node:path');
const html=fs.readFileSync(path.join(__dirname,'../src/network/WebAssets.h'),'utf8');
const script=html.match(/R"JS\(([\s\S]*?)\)JS";/)?.[1];
assert.ok(script, 'Embedded JavaScript asset missing');
const staticIds=[...html.matchAll(/\bid="([a-zA-Z0-9]+)"/g)].map(m=>m[1]);
class Element {
  constructor(tag='div'){this.tagName=tag;this.children=[];this.elements={};this.style={setProperty(){}};this.classList={add(){},remove(){}};this.value='';this.options={};this.textContent='';}
  append(...n){this.children.push(...n);for(const item of n)if(item.name)this.elements[item.name]=item;}
  prepend(...n){this.children.unshift(...n);}
  replaceChildren(...n){this.children=n;}
  setAttribute(k,v){this.options[k]=v;}
  closest(){return this.details??=new Element('details');}
  get valueAsNumber(){return this.value===''?NaN:Number(this.value);}
  getContext(){const methods=['scale','fillText','beginPath','moveTo','lineTo','stroke','arc','fill']; return Object.fromEntries(methods.map(m=>[m,()=>{}]));}
}
const sample={value:22,valid:true,stale:false};
const climate={temperature_c:sample,humidity_percent:sample,dewpoint_c:sample,pressure_pa:{value:101325}};
const s={boot_id:'F5',config_revision:3,plate:Array.from({length:9},(_,i)=>({position:i+1,enabled:i!==0,temperature_c:i===1?{value:null,valid:false,stale:true}:sample})),
    plate_statistics:{enabled_count:8},room:climate,stall:climate,outside:climate,heater:{enabled:true,pwm_limit_percent:25,energy_est_wh:50},
    cooling_pump:{enabled:false},evaporation_pump:{enabled:true},driver_heater:{enabled:true},vin_v:sample,tank_c:sample,manual:{session_id:4},
    mode:'MANUAL',auto_state:'HEATING',faults:[],identification:{test_id:'01',phase:'ABORTED',reason:'ABORTED_USER',enabled_mask:1,plate_rom_ids:['0000000000000028']},wifi_online:true};
async function inspect(page){
    const nodes=Object.fromEntries(staticIds.map(k=>[k,new Element()]));
    for(const [formId,fields] of Object.entries({login:['password'],manualForm:['pwm','duration','override','extend','cooling','evaporation'],testForm:['baseline','heat','cooldown','pwm']})) {
        for(const key of fields){const e=new Element('input');e.name=key;e.form=nodes[formId];e.value=key==='duration'?'300':'0';nodes[formId].elements[key]=e;}
    }
    const posts=[];
    const config={revision:3, values:{manual_timeout_s:300,mqtt_enabled:false,energy_checkpoint_s:900},secrets_set:{},plate:Array.from({length:9},(_,i)=>({enabled:i!==0})),tank_rom_id:null};
    const d={boot_id:'1',snapshot_sequence:1,heater_pwm_bits:10,plate_enabled_mask:1,pins:{},measurement_age_ms:{plate:[100,100]},watchdog_active:true};
    const responses={'/snapshot':s,'/config':config,'/debug':d,'/history':{points:[[1,2,3,4,5,6],[1000000,20,null,22,22,23]],uptime_us:1000000,utc_s:1800000000},'/parameters':{parameters:[]},'/sensors/scan':{ready:true,plate:[],tank:[]}};
    const context={console,Date,Intl,crypto:webcrypto,location:{pathname:page},sessionStorage:{getItem:()=>'testtoken',setItem(){},removeItem(){}},window:{devicePixelRatio:1,addEventListener(){}},document:{getElementById:id=>nodes[id],createElement:tag=>new Element(tag),querySelectorAll:()=>[]},
       AbortController,confirm:()=>true,clearTimeout(){},setTimeout(){},setInterval(){},async fetch(url,settings){
       const route=url.slice('/api/v1'.length);if(settings.method!=='GET'){posts.push({route,body:JSON.parse(settings.body),headers:settings.headers});return {ok:true,json:async()=>({state:'applied'})};}
       return {ok:true,json:async()=>responses[route]};}};
    vm.runInNewContext(script,context);
    await new Promise(resolve=>setImmediate(resolve));
    assert.match(nodes.connection.textContent,/Aktuell/);
    if(page==='/') {assert.equal(nodes.plate.children.length,9);assert.equal(nodes.climates.children.length,3);assert.match(nodes.plate.children[0].children[2].textContent,/Deaktiviert/);assert.match(nodes.plate.children[1].children[2].textContent,/Veraltet/);}
    if(page==='/setup') {
       assert.equal(nodes.functions.children.length,3);assert.equal(nodes.download.hidden,false);
       const form=nodes.manualForm;form.elements.extend.checked=true;form.onsubmit({preventDefault(){},target:form});
       await new Promise(resolve=>setImmediate(resolve));
       assert.equal(posts[0].route,'/manual/output');assert.equal(posts[0].body.session_id,4);assert.equal(posts[0].body.duration_seconds,300);
       assert.equal(posts[0].headers['X-Klima-Session'],'testtoken');
    }
    if(page==='/debug') assert.equal(nodes.debugItems.children.length,6);
}
(async()=>{for(const route of ['/','/setup','/debug']){await inspect(route);console.log('PASS Browser script '+route);}})().catch(e=>{console.error(e);process.exitCode=1;});
