const MANIFEST_URL = "firmwares-v2-089.json";
const DFU = {
  DETACH: 0,
  DNLOAD: 1,
  UPLOAD: 2,
  GETSTATUS: 3,
  CLRSTATUS: 4,
  GETSTATE: 5,
  ABORT: 6,
};

const DFU_STATE = {
  0: "appIDLE",
  1: "appDETACH",
  2: "dfuIDLE",
  3: "dfuDNLOAD-SYNC",
  4: "dfuDNBUSY",
  5: "dfuDNLOAD-IDLE",
  6: "dfuMANIFEST-SYNC",
  7: "dfuMANIFEST",
  8: "dfuMANIFEST-WAIT-RESET",
  9: "dfuUPLOAD-IDLE",
  10: "dfuERROR",
};

const state = {
  manifest: null,
  catalog: [],
  device: null,
  lastDeviceProductId: null,
  lastDeviceSerial: null,
  busy: false,
  dfuInterfaceNumber: null,
  dfuAltSetting: null,
};

const elements = {
  firmwareSelect: document.querySelector("#firmwareSelect"),
  firmwareDetails: document.querySelector("#firmwareDetails"),
  connectButton: document.querySelector("#connectButton"),
  flashButton: document.querySelector("#flashButton"),
  confirmCheckbox: document.querySelector("#confirmCheckbox"),
  deviceStatus: document.querySelector("#deviceStatus"),
  progress: document.querySelector("#progress"),
  progressText: document.querySelector("#progressText"),
  log: document.querySelector("#log"),
};

function log(message) {
  const time = new Date().toLocaleTimeString();
  elements.log.textContent += `[${time}] ${message}\n`;
  elements.log.scrollTop = elements.log.scrollHeight;
}

function setProgress(percent, text) {
  elements.progress.value = Math.max(0, Math.min(100, percent));
  elements.progressText.textContent = text;
}

function selectedFirmware() {
  const mode = elements.firmwareSelect.value;
  return state.catalog.find((firmware) => firmware.mode === mode);
}

function updateFlashEnabled() {
  const firmware = selectedFirmware();
  let compatible = false;
  if (firmware && state.device) {
    try { validateFirmwareTarget(firmware, state.device); compatible = true; } catch (_) { /* Show on install/connect. */ }
  }
  elements.connectButton.disabled = state.busy;
  elements.firmwareSelect.disabled = state.busy;
  elements.confirmCheckbox.disabled = state.busy;
  elements.flashButton.disabled = !(
    !state.busy && compatible &&
    elements.confirmCheckbox.checked
  );
}

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / 1024 / 1024).toFixed(2)} MiB`;
}

function updateFirmwareDetails() {
  const firmware = selectedFirmware();
  if (!firmware) {
    elements.firmwareDetails.textContent = "No firmware selected.";
    return;
  }

  const recommended = firmware.recommended ? " Recommended default." : "";
  elements.firmwareDetails.textContent =
    `${firmware.description} PID=${firmware.productId}, ${firmware.rate}, ` +
    `${firmware.geometry}, ${formatBytes(firmware.bytes)}.${recommended}`;
  updateFlashEnabled();
}

function firmwareOption(firmware) {
  const option = document.createElement("option");
  option.value = firmware.mode;
  option.textContent = firmware.recommended
    ? `${firmware.label} (recommended)`
    : firmware.label;
  return option;
}

async function loadManifest() {
  const response = await fetch(MANIFEST_URL, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Cannot load ${MANIFEST_URL}: HTTP ${response.status}`);
  }
  state.manifest = await response.json();
  state.catalog = [...state.manifest.firmwares];

  const groups = ["v2", "v1"].map((hardware) => {
    const group = document.createElement("optgroup");
    group.label = hardware === "v2" ? "Voice DSP+ V2" : "Original Voice DSP+ V1";
    group.append(...state.catalog.filter(f => f.hardware === hardware).map(firmwareOption));
    return group;
  });
  elements.firmwareSelect.replaceChildren(...groups);
  const requested = new URLSearchParams(location.search).get("firmware");
  const chosen = state.catalog.find(f => f.mode === requested) || state.catalog.find(f => f.recommended && f.hardware === "v2");
  if (chosen) elements.firmwareSelect.value = chosen.mode;
  updateFirmwareDetails();
  log(`Loaded ${state.manifest.firmwares.length} USB firmware image(s).`);
}

function productFilters() {
  // Match any PI_AI_MIC/XMOS runtime PID. Older firmware can expose a PID that
  // is not in the current public release matrix, but still has the same DFU
  // upgrade interface.
  return [{ vendorId: Number.parseInt(state.manifest.vendorId, 16) }];
}

function describeDevice(device) {
  const pid = device.productId.toString(16).padStart(4, "0");
  const vid = device.vendorId.toString(16).padStart(4, "0");
  return `${device.productName || "USB device"} [${vid}:${pid}]`;
}

function findDfuAlternate(device) {
  const expected = state.manifest.dfu;
  const interfaces = device.configuration?.interfaces || [];
  let fallback = null;

  for (const iface of interfaces) {
    for (const alternate of iface.alternates) {
      const isDfuClass = alternate.interfaceClass === 0xfe && alternate.interfaceSubclass === 0x01;
      const isExpectedInterface = iface.interfaceNumber === expected.interfaceNumber;
      const isExpectedAlt = alternate.alternateSetting === expected.altSetting;
      const candidate = {
        interfaceNumber: iface.interfaceNumber,
        alternateSetting: alternate.alternateSetting,
        classCode: alternate.interfaceClass,
        subclassCode: alternate.interfaceSubclass,
      };

      if (isDfuClass && isExpectedInterface && isExpectedAlt) return candidate;
      if (!fallback && isDfuClass && isExpectedAlt) fallback = candidate;
    }
  }

  if (fallback && selectedFirmware()?.hardware !== "v2") return fallback;
  throw new Error(
    `No DFU interface found. Expected interface ${expected.interfaceNumber}, alt ${expected.altSetting}.`,
  );
}

async function connectDevice() {
  if (!("usb" in navigator)) {
    throw new Error("WebUSB is not available. Use Chrome/Chromium over HTTPS or localhost.");
  }

  const device = await navigator.usb.requestDevice({ filters: productFilters() });
  await openDfuDevice(device);
}

async function openDfuDevice(device) {
  validateFirmwareTarget(selectedFirmware(), device);
  await device.open();
  if (!device.configuration) {
    await device.selectConfiguration(1);
  }

  const dfu = findDfuAlternate(device);
  await device.claimInterface(dfu.interfaceNumber);
  try {
    await device.selectAlternateInterface(dfu.interfaceNumber, dfu.alternateSetting);
  } catch (error) {
    log(
      `WARN: unable to select DFU alt ${dfu.alternateSetting}: ${error.message}. Retrying once...`,
    );
    await sleep(250);
    try {
      await device.selectAlternateInterface(dfu.interfaceNumber, dfu.alternateSetting);
    } catch (retryError) {
      await device.close();
      throw new Error(`Cannot select the upgrade partition; download refused: ${retryError.message}`);
    }
  }

  state.device = device;
  state.lastDeviceProductId = device.productId;
  state.lastDeviceSerial = device.serialNumber;
  state.dfuInterfaceNumber = dfu.interfaceNumber;
  state.dfuAltSetting = dfu.alternateSetting;
  elements.deviceStatus.textContent =
    `${describeDevice(device)} connected, DFU interface ${dfu.interfaceNumber}, alt ${dfu.alternateSetting}.`;
  log(elements.deviceStatus.textContent);
  updateFlashEnabled();
}

async function sha256Hex(buffer) {
  const digest = await crypto.subtle.digest("SHA-256", buffer);
  return [...new Uint8Array(digest)]
    .map((value) => value.toString(16).padStart(2, "0"))
    .join("");
}

async function loadFirmwareImage(firmware) {
  setProgress(2, `Downloading ${firmware.mode}...`);
  log(`Fetching ${firmware.path}`);
  const response = await fetch(firmware.path, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Cannot load ${firmware.path}: HTTP ${response.status}`);
  }
  const buffer = await response.arrayBuffer();
  if (buffer.byteLength !== firmware.bytes) {
    throw new Error(
      `Firmware size mismatch: expected ${firmware.bytes}, got ${buffer.byteLength}`,
    );
  }

  setProgress(5, "Verifying SHA256...");
  const actualSha = await sha256Hex(buffer);
  if (actualSha.toLowerCase() !== firmware.sha256.toLowerCase()) {
    throw new Error(`SHA256 mismatch: expected ${firmware.sha256}, got ${actualSha}`);
  }
  log(`SHA256 OK: ${actualSha}`);
  return new Uint8Array(buffer);
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function getStatus() {
  const result = await state.device.controlTransferIn(
    {
      requestType: "class",
      recipient: "interface",
      request: DFU.GETSTATUS,
      value: 0,
      index: state.dfuInterfaceNumber,
    },
    6,
  );
  if (result.status !== "ok" || result.data.byteLength !== 6) {
    throw new Error(`DFU_GETSTATUS failed: ${result.status}`);
  }
  const data = result.data;
  return {
    status: data.getUint8(0),
    pollTimeoutMs: data.getUint8(1) | (data.getUint8(2) << 8) | (data.getUint8(3) << 16),
    state: data.getUint8(4),
    iString: data.getUint8(5),
  };
}

async function clearStatus() {
  await state.device.controlTransferOut({
    requestType: "class",
    recipient: "interface",
    request: DFU.CLRSTATUS,
    value: 0,
    index: state.dfuInterfaceNumber,
  });
}

async function abortDfu() {
  await state.device.controlTransferOut({
    requestType: "class",
    recipient: "interface",
    request: DFU.ABORT,
    value: 0,
    index: state.dfuInterfaceNumber,
  });
}

async function detachDfu() {
  await state.device.controlTransferOut({
    requestType: "class",
    recipient: "interface",
    request: DFU.DETACH,
    value: 1000,
    index: state.dfuInterfaceNumber,
  });
}

async function closeCurrentDevice() {
  if (!state.device) return;
  try {
    await state.device.close();
  } catch (error) {
    log(`Device close returned: ${error.message}`);
  }
  state.device = null;
  state.dfuInterfaceNumber = null;
  state.dfuAltSetting = null;
  elements.deviceStatus.textContent = "Waiting for DFU reconnect...";
  updateFlashEnabled();
}

async function reconnectAuthorizedDevice() {
  const vendorId = Number.parseInt(state.manifest.vendorId, 16);
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const devices = await navigator.usb.getDevices();
    const match = devices.find((device) => {
      if (device.vendorId !== vendorId) return false;
      if (state.lastDeviceProductId && device.productId !== state.lastDeviceProductId) return false;
      if (state.lastDeviceSerial && device.serialNumber !== state.lastDeviceSerial) return false;
      return true;
    });
    if (match) {
      await openDfuDevice(match);
      return;
    }
    await sleep(250);
  }
  throw new Error("DFU detach completed, but the XMOS device did not reappear in WebUSB.");
}

async function ensureDfuDownloadMode() {
  let status = await getStatus();
  log(`Initial DFU state: ${DFU_STATE[status.state] || status.state}`);

  if (status.state === 10) {
    log("Clearing DFU error state.");
    await clearStatus();
    status = await pollUntilReady("after clear status");
  }

  if (status.state === 0 || status.state === 1) {
    log("Device is in DFU runtime state; sending DFU_DETACH before download.");
    await detachDfu();
    try {
      await state.device.reset();
    } catch (error) {
      log(`Device reset after detach returned: ${error.message}`);
    }
    await closeCurrentDevice();
    await sleep(1000);
    await reconnectAuthorizedDevice();
    status = await getStatus();
    log(`DFU state after detach/reconnect: ${DFU_STATE[status.state] || status.state}`);
  }

  if (status.state === 5) {
    log("Aborting stale DNLOAD-IDLE state before starting a new download.");
    await abortDfu();
    status = await pollUntilReady("after abort");
  }

  if (status.state !== 2) {
    throw new Error(`Device is not ready for DFU download: ${DFU_STATE[status.state] || status.state}`);
  }
}

async function pollUntilReady(context) {
  for (let attempt = 0; attempt < 120; attempt += 1) {
    const status = await getStatus();
    const stateName = DFU_STATE[status.state] || `state-${status.state}`;
    if (status.status !== 0) {
      await clearStatus();
      throw new Error(`${context}: DFU error status=${status.status}, state=${stateName}`);
    }
    if (status.state !== 4) {
      return status;
    }
    await sleep(Math.min(Math.max(status.pollTimeoutMs, 5), 500));
  }
  throw new Error(`${context}: timed out waiting for DFU device`);
}

async function downloadBlock(blockNumber, chunk, transferSize) {
  try {
    const result = await state.device.controlTransferOut(
      {
        requestType: "class",
        recipient: "interface",
        request: DFU.DNLOAD,
        value: blockNumber,
        index: state.dfuInterfaceNumber,
      },
      chunk,
    );
    if (result.status !== "ok") {
      throw new Error(result.status);
    }
  } catch (error) {
    const wrapped = new Error(
      `DFU_DNLOAD block ${blockNumber} failed (${chunk.byteLength} bytes, transfer size ${transferSize}): ${error.message}`,
    );
    wrapped.cause = error;
    throw wrapped;
  }
}

async function recoverAfterFirstBlockFailure(context) {
  log(`${context}: trying DFU recovery before next transfer size.`);
  await clearStatus().catch((error) => log(`CLRSTATUS during recovery returned: ${error.message}`));
  await abortDfu().catch((error) => log(`ABORT during recovery returned: ${error.message}`));
  await clearStatus().catch(() => undefined);
  await sleep(100);
  const status = await getStatus();
  log(`DFU state after recovery: ${DFU_STATE[status.state] || status.state}`);
  if (status.state === 10) {
    await clearStatus();
    await pollUntilReady("after recovery clear status");
    return;
  }
  if (status.state !== 2) {
    throw new Error(`${context}: recovery did not return to dfuIDLE`);
  }
}

async function flashWithTransferSize(image, transferSize) {
  const totalBlocks = Math.ceil(image.byteLength / transferSize);
  log(`Trying DFU transfer size ${transferSize} bytes, ${totalBlocks} blocks.`);

  for (let block = 0; block < totalBlocks; block += 1) {
    const start = block * transferSize;
    const end = Math.min(start + transferSize, image.byteLength);
    const chunk = image.slice(start, end);
    try {
      await downloadBlock(block, chunk, transferSize);
    } catch (error) {
      error.firstDataBlock = block === 0;
      throw error;
    }
    await pollUntilReady(`block ${block}`);
    const percent = 5 + ((block + 1) / totalBlocks) * 90;
    setProgress(percent, `Flashing block ${block + 1}/${totalBlocks}...`);
  }

  setProgress(97, "Finalizing DFU manifest...");
  try {
    await downloadBlock(totalBlocks, new Uint8Array(), transferSize);
  } catch (error) {
    error.firstDataBlock = false;
    throw error;
  }
  const finalStatus = await pollUntilReady("manifest");
  log(`Final DFU state: ${DFU_STATE[finalStatus.state] || finalStatus.state}`);
}

async function flashFirmware() {
  const firmware = selectedFirmware();
  if (!firmware) throw new Error("No firmware selected.");
  if (!state.device) throw new Error("No Voice DSP+ device connected.");
  validateFirmwareTarget(firmware, state.device);
  if (state.dfuAltSetting !== 1 || (firmware.hardware === "v2" && state.dfuInterfaceNumber !== 4)) {
    throw new Error("Not connected to the correct DFU upgrade interface. Reconnect the board.");
  }

  const image = await loadFirmwareImage(firmware);
  log(`Starting DFU download: ${firmware.mode}, ${image.byteLength} bytes.`);

  if (firmware.hardware === "v2") await stopV2Audio();
  await ensureDfuDownloadMode();

  // XMOS's writer programs one 256-byte page per block; smaller chunks are
  // not a recovery strategy and can hit the old firmware's page-size assert.
  const preferredTransferSize = firmware.dfu?.transferSize || state.manifest.dfu.transferSize;
  if (preferredTransferSize !== 256 || image.byteLength % 256 !== 0) {
    throw new Error("Expected a page-aligned XMOS DFU upgrade image and 256-byte transfers.");
  }
  const transferSizes = [256];
  let lastError = null;
  for (let attempt = 0; attempt < transferSizes.length; attempt += 1) {
    const transferSize = transferSizes[attempt];
    try {
      await flashWithTransferSize(image, transferSize);
      lastError = null;
      break;
    } catch (error) {
      lastError = error;
      log(`DFU transfer size ${transferSize} failed: ${error.message}`);
      if (error.firstDataBlock !== true || attempt === transferSizes.length - 1) {
        throw error;
      }
      await recoverAfterFirstBlockFailure(`transfer size ${transferSize}`);
    }
  }
  if (lastError) {
    throw lastError;
  }

  setProgress(99, "Resetting device...");
  try {
    await state.device.reset();
  } catch (error) {
    log(`Device reset returned: ${error.message}`);
  }
  setProgress(100, "Flash command completed. Reconnect the USB audio device if needed.");
  log("Flash complete.");
}

async function runSafely(action) {
  if (state.busy) return;
  state.busy = true;
  updateFlashEnabled();
  try {
    await action();
  } catch (error) {
    setProgress(0, "Error.");
    log(`ERROR: ${error.message}`);
    console.error(error);
  } finally {
    state.busy = false;
    updateFlashEnabled();
  }
}

function validateFirmwareTarget(firmware, device) {
  if (!firmware) throw new Error("Select the firmware matching your hardware first.");
  const isV2 = /^VDSP2-/.test(device.serialNumber || "");
  if (firmware.hardware === "v2") {
    if (!isV2 || !(firmware.serialPrefixes || []).some(prefix => device.serialNumber.startsWith(prefix))) {
      throw new Error("This firmware requires a recognized Voice DSP+ V2 USB identity. Do not install it on V1.");
    }
    if (device.productId !== Number.parseInt(firmware.productId, 16)) throw new Error("V2 USB product mismatch.");
  } else if (isV2) {
    throw new Error("Legacy V1 firmware cannot be installed on the connected V2.");
  }
  if (device.vendorId !== Number.parseInt(firmware.vendorId || state.manifest.vendorId, 16)) {
    throw new Error("USB vendor mismatch.");
  }
  if (firmware.imageType && firmware.imageType !== "dfu-upgrade") {
    throw new Error("Only DFU upgrade images are accepted, never factory or SPI images.");
  }
}

async function stopV2Audio() {
  const setup = {requestType:"vendor", recipient:"device", request:0, value:17, index:0x30};
  const before = await state.device.controlTransferIn({...setup,value:0x92},56);
  if (before.status !== "ok" || !before.data || before.data.byteLength !== 56 ||
      before.data.getUint8(0) !== 2 || !before.data.getUint8(1)) throw new Error("Cannot read V2 audio state; no flash started.");
  const route = before.data.getUint8(2), level = before.data.getUint8(44), flags = before.data.getUint8(40) & 2;
  if (route < 1 || route > 4 || level > 120) throw new Error("Invalid V2 audio state.");
  // Mute is transient. Do not save OFF/-30 dB over the user's persistent settings.
  const write = await state.device.controlTransferOut(setup, new Uint8Array([2,route,level,flags|1]));
  if (write.status !== "ok" || write.bytesWritten !== 4) throw new Error("Cannot mute audio before DFU.");
  const ack = await state.device.controlTransferIn(setup,1);
  if (ack.status !== "ok" || !ack.data || ack.data.byteLength !== 1 || ack.data.getUint8(0) !== 0)
    throw new Error("Audio mute command refused; no flash started.");
  for (let i=0;i<40;i++) {
    const reply = await state.device.controlTransferIn({...setup,value:0x92},56);
    if (reply.status !== "ok" || !reply.data || reply.data.byteLength !== 56)
      throw new Error("Cannot verify audio shutdown.");
    if (reply.data.getUint8(0) === 2 && (reply.data.getUint8(5) & 32) &&
        reply.data.getUint32(16,true) === reply.data.getUint32(20,true)) {
      log("V2 audio muted; output, level and filter unchanged.");
      await sleep(3200); // Let the existing settings journal settle before DFU.
      return;
    }
    await sleep(50);
  }
  throw new Error("Audio mute did not apply; no flash started.");
}

if (navigator.usb) navigator.usb.addEventListener("disconnect", (event) => {
  if (event.device === state.device) {
    state.device = null;
    elements.deviceStatus.textContent = "Device disconnected. Reconnect before installing again.";
    updateFlashEnabled();
  }
});

elements.firmwareSelect.addEventListener("change", updateFirmwareDetails);
elements.confirmCheckbox.addEventListener("change", updateFlashEnabled);
elements.connectButton.addEventListener("click", () => runSafely(connectDevice));
elements.flashButton.addEventListener("click", () => runSafely(flashFirmware));

runSafely(loadManifest);
