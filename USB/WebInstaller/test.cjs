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
const i2s = catalog.firmwares.find(f=>f.outputMode==='i2s');
const legacy = {...v2,hardware:'v1'};
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
  assert.throws(()=>context.validateFirmwareTarget(legacy,target));
  context.validateFirmwareTarget(i2s,target);
  assert.equal(catalog.firmwares.length,2);
  assert.ok(catalog.firmwares.every(f=>f.hardware==='v2'));
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
  let fault=null;
  let idleMode=null, reads=0;
  const usb={...target,
    async controlTransferOut(setup,bytes){
      transfers.push({setup,bytes:Array.from(bytes||[])});
      if(setup.requestType==='vendor')muted=true;
      return {status:'ok',bytesWritten:bytes?.length||0};
    },
    async controlTransferIn(setup,size){
      if(setup.requestType==='vendor'){
        assert.equal(setup.index,0x30);
        if(size===1){assert.equal(setup.value,17);return {status:'ok',data:view([0])};}
        assert.equal(setup.value,0x92);
        assert.equal(size,57,'read includes transport status plus 56-byte payload');
        const b=new Uint8Array(57);b[1]=2;b[2]=1;b[3]=2;b[4]=2;b[6]=1|(muted?32:0);b[41]=muted?3:2;b[45]=24;
        const fields=new DataView(b.buffer);
        fields.setUint32(17,muted?1:0,true);
        fields.setUint32(21,muted?1:0,true);
        fields.setUint32(25,++reads*20,true);
        if(idleMode){
          b[2]=0;b[4]=0;b[6]=33;
          fields.setUint32(21,0xffffffff,true);
          if(idleMode==='stalled')fields.setUint32(25,100,true);
          if(idleMode==='active')b[4]=3;
          if(idleMode==='not-muted')b[6]=1;
          if(idleMode==='not-latched')b[41]=2;
          if(idleMode==='no-new-epoch')fields.setUint32(17,0,true);
        }
        if(fault==='transport')b[0]=3;
        if(fault==='schema')b[1]=99;
        if(fault==='hardware')b[6]|=16;
        if(fault==='io')b[6]=0;
        if(fault==='error')fields.setInt32(13,-10,true);
        const framed=fault==='unframed'?b.slice(1):b;
        const padded=new Uint8Array(framed.length+8);padded.set(framed,4);
        return {status:'ok',data:new DataView(padded.buffer,4,framed.length)};
      }
      return {status:'ok',data:view([0,0,0,0,2,0])};
    },async reset(){reset=true;}};
  context.device=usb;
  run('state.device=device;state.dfuInterfaceNumber=4;state.dfuAltSetting=1;elements.confirmCheckbox.checked=true;');
  context.fetch=async()=>({ok:true,arrayBuffer:async()=>image.buffer});
  for(const [mode,message] of [['unframed',/56\/57 bytes/],['transport',/transport status 3/],
    ['schema',/Unsupported V2 audio schema/],['hardware',/requires diagnosis/],
    ['io',/requires diagnosis/],['error',/requires diagnosis/]]){
    fault=mode;
    await assert.rejects(context.flashFirmware(),message);
    assert.equal(transfers.length,0,'invalid initial state must not mute or start DFU');
    assert.equal(reset,false);
  }
  fault=null;
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
  for(const mode of ['stalled','active','not-muted','not-latched','no-new-epoch']){
    idleMode=mode;muted=false;reset=false;transfers.length=0;
    await assert.rejects(context.flashFirmware(),/Audio mute not verified/);
    assert.equal(transfers.filter(t=>t.setup.requestType==='class').length,0);
    assert.equal(reset,false);
  }
  idleMode='healthy';muted=false;reset=false;transfers.length=0;
  await context.flashFirmware();
  assert.deepEqual(transfers[0].bytes,[2,2,24,3]);
  assert.equal(transfers.filter(t=>t.setup.requestType==='class').length,pages+1);
  assert.equal(reset,true);
  run('elements.forceCheckbox.checked=true');
  context.validateFirmwareTarget(v2,{...target,serialNumber:'PI_AI_MIC',productId:0x1234});
  context.validateFirmwareTarget(catalog.firmwares[1],target);
  assert.throws(()=>context.validateFirmwareTarget({...v2,imageType:'factory'},target));
  assert.throws(()=>context.validateFirmwareTarget(v2,{...target,vendorId:0x1234}));
  assert.equal(context.findDfuAlternate(iface(3)).interfaceNumber,3);
  assert.throws(()=>context.findDfuAlternate({configuration:{interfaces:[]}}),/No DFU interface/);
  let factoryClosed=false;
  await assert.rejects(context.openDfuDevice({...target,serialNumber:'VDSP2-FACTORY-RAM-001',
    configuration:{interfaces:[]},async open(){},async close(){factoryClosed=true;}}),/Factory RAM tester has no DFU/);
  assert.equal(factoryClosed,true);
  transfers.length=0;fault='hardware';reset=false;
  await context.flashFirmware();
  assert.equal(transfers.filter(t=>t.setup.requestType==='class').length,pages+1);
  assert.equal(reset,true,'force bypasses audio-health refusal');
  usb.serialNumber='PI_AI_MIC';transfers.length=0;
  run('state.dfuInterfaceNumber=3');
  await context.flashFirmware();
  assert.equal(transfers.filter(t=>t.setup.requestType==='vendor').length,0,'do not send V2 commands to unknown runtime');
  assert.ok(transfers.every(t=>t.setup.index===3));
  context.bad={...v2,sha256:'0'.repeat(64)};
  run('state.catalog=[bad]');
  const forceCount=transfers.length;
  await assert.rejects(context.flashFirmware(),/SHA256 mismatch/);
  assert.equal(transfers.length,forceCount,'force does not bypass hash');
  context.good=v2;
  run('state.catalog=[good];state.dfuAltSetting=0');
  await assert.rejects(context.flashFirmware(),/correct DFU upgrade interface/);
  run('elements.forceCheckbox.checked=false;state.dfuInterfaceNumber=4;state.dfuAltSetting=1');
  usb.serialNumber=target.serialNumber;fault=null;
  const i2sImage=Uint8Array.from(fs.readFileSync(path.join(__dirname,'../Firmware',path.basename(i2s.path))));
  assert.equal(crypto.createHash('sha256').update(i2sImage).digest('hex'),i2s.sha256);
  assert.equal(i2sImage.length,i2s.bytes);
  context.location.search='?firmware=v2-i2s-48k';
  context.fetch=async()=>({ok:true,json:async()=>catalog});
  await context.loadManifest();
  assert.equal(run('selectedFirmware().outputMode'),'i2s');
  context.window={confirm:()=>false};
  transfers.length=0;
  await context.flashFirmware();
  assert.equal(transfers.length,0,'cancel I2S warning performs no USB writes');
  context.window.confirm=()=>true;
  idleMode=null;muted=false;reset=false;
  context.fetch=async()=>({ok:true,arrayBuffer:async()=>i2sImage.buffer});
  await context.flashFirmware();
  assert.equal(transfers.filter(t=>t.setup.requestType==='class').length,i2sImage.length/256+1);
  assert.match(run('elements.progressText.textContent'),/USB disappearance is expected/);
  console.log('PASS: V2-only catalog, I2S deep link, pinned I2S image, cancellation, complete I2S DFU and expected USB disappearance');
  console.log('PASS: explicit force bypasses identity/PID/audio gating, retains hash/vendor/image/alt checks, reports factory without DFU, and uses advertised interface');
  console.log('PASS: healthy idle codec accepted; hardware faults, invalid I/O, errors, stalled runtime, active output and missing mute/epoch rejected before DFU');
  console.log(`PASS: V2 deep link, V1/V2 isolation, PID/serial/type guards, alt1/interface4, failed claim, hash, framed 57-byte responses, offset views, malformed state rejection, ${pages} DFU pages, transient mute preserves settings, reset and concurrency`);
})().catch(error=>{console.error(error);process.exitCode=1;});
