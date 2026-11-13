// Executes the real browser app with fake DOM/USB. Never opens real USB devices.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const catalog = JSON.parse(fs.readFileSync(path.join(__dirname, 'firmwares.json')));
const makeElement = () => ({addEventListener(){}, replaceChildren(){}, append(){}, value:'', textContent:'', disabled:false});
const context = vm.createContext({document:{querySelector:makeElement,createElement:makeElement},
  navigator:{}, console, setTimeout:(callback)=>setTimeout(callback,0), Date,
  Uint8Array, URLSearchParams, location:{search:'?firmware=v2-usb-mono-48k'}, crypto:crypto.webcrypto});
vm.runInContext(fs.readFileSync(path.join(__dirname,'app.js'),'utf8').replace('runSafely(loadManifest);',''), context);
const run = code => vm.runInContext(code,context);
const alt = n => ({alternateSetting:n,interfaceClass:0xfe,interfaceSubclass:1});
const v2 = catalog.firmwares.find(f=>f.hardware==='v2');
const target = {vendorId:0x20b1,productId:0x4fc7,serialNumber:'VDSP2-USBMONO-089-DEV'};
const image = Uint8Array.from(fs.readFileSync(path.join(__dirname,'../Firmware',path.basename(v2.path))));
const view = bytes => new DataView(Uint8Array.from(bytes).buffer);

(async()=>{
  context.fetch=async()=>({ok:true,json:async()=>catalog});
  await context.loadManifest();
  assert.equal(run('selectedFirmware().version'),'0.8.9-r4');
  assert.equal(crypto.createHash('sha256').update(image).digest('hex'),v2.sha256);
  assert.equal(image.byteLength,v2.bytes);
  context.validateFirmwareTarget(v2,target);
  assert.throws(()=>context.validateFirmwareTarget(v2,{...target,serialNumber:'PI_AI_MIC'}));
  assert.throws(()=>context.validateFirmwareTarget(catalog.firmwares[1],target));
  assert.throws(()=>context.validateFirmwareTarget(v2,{...target,productId:0x1234}));
  assert.throws(()=>context.validateFirmwareTarget({...v2,imageType:'factory'},target));
  const iface = n => ({configuration:{interfaces:[{interfaceNumber:n,alternates:[alt(0),alt(1)]}]}});
  assert.equal(context.findDfuAlternate(iface(4)).alternateSetting,1);
  assert.throws(()=>context.findDfuAlternate(iface(3)));
  assert.throws(()=>context.findDfuAlternate({configuration:{interfaces:[{interfaceNumber:4,alternates:[alt(0)]}]}}));
  let closed=false;
  await assert.rejects(context.openDfuDevice({...target,...iface(4),async open(){},async claimInterface(){},
    async selectAlternateInterface(){throw Error('IO');},async close(){closed=true;}}),/download refused/);
  assert.equal(closed,true);
  const transfers=[];
  let muted=false, reset=false;
  const usb={...target,
    async controlTransferOut(setup,bytes){
      transfers.push({setup,bytes:Array.from(bytes||[])});
      if(setup.requestType==='vendor')muted=true;
      return {status:'ok',bytesWritten:bytes?.length||0};
    },
    async controlTransferIn(setup,size){
      if(setup.requestType==='vendor'){
        if(size===1)return {status:'ok',data:view([0])};
        const b=new Uint8Array(56);b[0]=2;b[1]=1;b[2]=2;b[3]=2;b[5]=muted?32:0;b[40]=2;b[44]=24;
        return {status:'ok',data:view(b)};
      }
      return {status:'ok',data:view([0,0,0,0,2,0])};
    },async reset(){reset=true;}};
  context.device=usb;
  run('state.device=device;state.dfuInterfaceNumber=4;state.dfuAltSetting=1;elements.confirmCheckbox.checked=true;');
  context.fetch=async()=>({ok:true,arrayBuffer:async()=>image.buffer});
  await context.flashFirmware();
  assert.deepEqual(transfers[0].bytes,[2,2,24,3], 'mute must preserve route, level and full-speaker flag');
  const blocks=transfers.filter(t=>t.setup.requestType==='class');
  const pages=image.length/256;
  assert.deepEqual(blocks.map(t=>[t.setup.value,t.setup.index,t.bytes.length]),
    [...Array.from({length:pages},(_,i)=>[i,4,256]),[pages,4,0]]);
  assert.equal(reset,true);
  const count=transfers.length;
  context.bad={...v2,sha256:'0'.repeat(64)};
  run('state.catalog=[bad]');
  await assert.rejects(context.flashFirmware(),/SHA256 mismatch/);
  assert.equal(transfers.length,count,'wrong hash must not even mute the device');
  context.good=v2;run('state.catalog=[good];state.busy=true;');
  let executed=false;
  await context.runSafely(async()=>{executed=true;});
  assert.equal(executed,false,'concurrent operations forbidden');
  run('state.busy=false');
  await context.runSafely(async()=>{
    assert.equal(run('elements.firmwareSelect.disabled'),true);
    assert.equal(run('elements.connectButton.disabled'),true);
  });
  assert.equal(run('state.busy'),false);
  console.log(`PASS: V2 deep link, V1/V2 isolation, PID/serial/type guards, alt1/interface4, failed claim, hash, ${pages} DFU pages, transient mute preserves settings, reset and concurrency`);
})().catch(error=>{console.error(error);process.exitCode=1;});
