// N64 cart operations: high-level commands, header parsing, catalog

// Save code from n64.txt → save hint string
const N64TXT_SAVE_CODES = {0:'none',1:'sram',2:'sram96k',4:'flashram',5:'eeprom4k',6:'eeprom16k'};

// ── Catalog ──
let cartCatalog = {};
let catalogLoaded = false;

function isCatalogLoaded() { return catalogLoaded; }

function parseN64Txt(text) {
  const lines = text.split('\n');
  const db = {};
  let currentTitle = null;
  const metaRe = /^\s*([0-9A-Fa-f]{8})\s*,\s*([0-9A-Fa-f]{8})\s*,\s*([0-9A-Fa-f]{1,3})\s*,\s*([0-9A-Fa-f]{1,2})\s*$/;

  for (const raw of lines) {
    const line = raw.trim();
    if (!line) continue;
    const m = metaRe.exec(line);
    if (m && currentTitle) {
      const crc1 = m[2].toUpperCase();
      const sizeMib = parseInt(m[3], 16);
      const saveCode = parseInt(m[4], 16);
      const saveHint = N64TXT_SAVE_CODES[saveCode] || null;
      const title = currentTitle.replace(/\.(z64|n64|v64)$/i, '').trim();
      if (!db[crc1]) {
        db[crc1] = { title, save: saveHint, rom_mib: sizeMib };
      }
      currentTitle = null;
    } else {
      currentTitle = line;
    }
  }
  return db;
}

function setCatalog(db) {
  cartCatalog = db;
  catalogLoaded = true;
  return Object.keys(db).length;
}

function lookupCatalog(headerData) {
  if (headerData.length < 0x14) return null;
  const crc1 = Array.from(headerData.slice(0x10, 0x14))
    .map(b => b.toString(16).padStart(2, '0')).join('').toUpperCase();
  return cartCatalog[crc1] || null;
}

// ── Header parsing and detection ──
function parseHeader(data) {
  const titleBytes = data.slice(0x20, 0x34);
  const title = new TextDecoder('ascii').decode(titleBytes).replace(/\0+$/, '').trim();
  const idBytes = data.slice(0x3B, 0x3F);
  const gameId = new TextDecoder('ascii').decode(idBytes);
  return { title, gameId };
}

function isEmptyHeader(data) {
  if (data.length < 64) return true;
  return data.every(b => b === 0xFF) || data.every(b => b === 0x00);
}

const GS_MAGIC = [0x28, 0x43, 0x29, 0x20, 0x4D, 0x55, 0x53, 0x48, 0x52, 0x4F, 0x4F, 0x4D]; // "(C) MUSHROOM"

function isGameSharkHeader(data) {
  if (data.length < 0x20 + GS_MAGIC.length) return false;
  for (let i = 0; i < GS_MAGIC.length; i++) {
    if (data[0x20 + i] !== GS_MAGIC[i]) return false;
  }
  return true;
}

// ── High-level operations ──
async function doPing() {
  const r = await cmdRsp(CMD_PING);
  if (r.status !== 0) throw new Error(`Ping failed: status=${r.status}`);
  return r.value;
}

async function doGetVersion() {
  try {
    await drainFlush();
    await sendCmd(CMD_GET_VERSION);
    const data = await recvReliable(64);
    const r = await recvRsp(3000);
    if (r.status !== 0) return null;
    return new TextDecoder().decode(data).replace(/\0+$/, '').trim();
  } catch {
    await drainFlush();
    return null;
  }
}

async function doRescan() {
  const r = await cmdRsp(CMD_N64_RESCAN, 0, 0, 10000);
  if (r.status !== 0) throw new Error(`Rescan failed: status=0x${r.status.toString(16)}`);
  return r.value;
}

async function doStatus() {
  const r = await cmdRsp(CMD_N64_STATUS);
  if (r.status !== 0) throw new Error(`Status failed: status=0x${r.status.toString(16)}`);
  const sType = (r.value >> 12) & 0x0F;
  const sizeUnits = r.value & 0x0FFF;
  return { type: sType, typeName: SAVE_TYPES[sType] || 'unknown', size: sizeUnits * 64 };
}

async function doReadHeader() {
  await drainFlush();
  await sendCmd(CMD_N64_EXPORT_HEADER);
  const data = await recvReliable(64);
  const r = await recvRsp(5000);
  if (r.status !== 0) throw new Error(`Header read failed: status=0x${r.status.toString(16)}`);
  return data;
}

async function doExportSave(size, onProgress) {
  await drainFlush();
  await sendCmd(CMD_N64_EXPORT_SAVE);
  const data = await recvReliable(size, onProgress);
  const r = await recvRsp(5000);
  if (r.status !== 0) throw new Error(`Export failed: status=0x${r.status.toString(16)}`);
  const hostCrc = crc16(data);
  if (hostCrc !== r.value) {
    throw new Error(`CRC mismatch: host=0x${hostCrc.toString(16).toUpperCase()} pico=0x${r.value.toString(16).toUpperCase()}`);
  }
  return data;
}

async function doImportSave(data, onProgress) {
  await drainFlush();
  await sendCmd(CMD_N64_IMPORT_SAVE);
  await sendReliable(data, onProgress);
  const r = await recvRsp(15000);
  if (r.status !== 0) throw new Error(`Import failed: status=0x${r.status.toString(16)}`);
  const hostCrc = crc16(data);
  if (hostCrc !== r.value) {
    throw new Error(`CRC mismatch: host=0x${hostCrc.toString(16).toUpperCase()} pico=0x${r.value.toString(16).toUpperCase()}`);
  }
  return { status: r.status, crc: r.value };
}

async function doRomInfo() {
  const r = await cmdRsp(CMD_N64_ROM_INFO);
  if (r.status !== 0) throw new Error(`ROM info failed: status=0x${r.status.toString(16)}`);
  return r.value * 2048;
}

async function doExportRom(size, onProgress) {
  await drainFlush();
  await sendCmd(CMD_N64_EXPORT_ROM);
  const data = await recvReliable(size, onProgress);
  const r = await recvRsp(10000);
  if (r.status !== 0) throw new Error(`ROM dump failed: status=0x${r.status.toString(16)}`);
  const hostCrc = crc16(data);
  if (hostCrc !== r.value) {
    throw new Error(`CRC mismatch: host=0x${hostCrc.toString(16).toUpperCase()} pico=0x${r.value.toString(16).toUpperCase()}`);
  }
  return data;
}

async function doExportMpk(onProgress) {
  await drainFlush();
  await sendCmd(CMD_N64_EXPORT_MPK);
  const data = await recvReliable(N64_MEMPAK_SIZE, onProgress);
  const r = await recvRsp(10000);
  if (r.status !== 0) throw new Error(`MPK export failed: status=0x${r.status.toString(16)}`);
  const hostCrc = crc16(data);
  if (hostCrc !== r.value) {
    throw new Error(`CRC mismatch: host=0x${hostCrc.toString(16).toUpperCase()} pico=0x${r.value.toString(16).toUpperCase()}`);
  }
  return data;
}

async function doImportMpk(data, onProgress) {
  if (data.length !== N64_MEMPAK_SIZE) {
    throw new Error(`MPK file must be ${N64_MEMPAK_SIZE} bytes (got ${data.length})`);
  }
  await drainFlush();
  await sendCmd(CMD_N64_IMPORT_MPK);
  await sendReliable(data, onProgress, 80);
  const r = await recvRsp(15000);
  if (r.status !== 0) throw new Error(`MPK import failed: status=0x${r.status.toString(16)}`);
  const hostCrc = crc16(data);
  if (hostCrc !== r.value) {
    throw new Error(`CRC mismatch: host=0x${hostCrc.toString(16).toUpperCase()} pico=0x${r.value.toString(16).toUpperCase()}`);
  }
  return { status: r.status, crc: r.value };
}

async function doGsInfo() {
  await drainFlush();
  await sendCmd(CMD_N64_GS_INFO);
  const data = await recvReliable(18);
  const r = await recvRsp(5000);
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  return {
    present: data[0] !== 0,
    family: data[1],
    flashId: view.getUint16(2, true),
    baseAddr: view.getUint32(4, true),
    sizeBytes: view.getUint32(8, true),
    caps: view.getUint16(12, true),
    flags: view.getUint16(14, true),
    mfgId: view.getUint16(16, true),
    status: r.status,
  };
}

async function doGsExport(size, onProgress) {
  await drainFlush();
  await sendCmd(CMD_N64_GS_EXPORT);
  const data = await recvReliable(size, onProgress);
  const r = await recvRsp(10000);
  if (r.status !== 0) throw new Error(`GS export failed: status=0x${r.status.toString(16)}`);
  const hostCrc = crc16(data);
  if (hostCrc !== r.value) {
    throw new Error(`CRC mismatch: host=0x${hostCrc.toString(16).toUpperCase()} pico=0x${r.value.toString(16).toUpperCase()}`);
  }
  return data;
}

async function doGsImport(data, onProgress) {
  if ((data.length & 1) !== 0) throw new Error('GS flash data must be word-aligned (even size)');
  await drainFlush();
  await sendCmd(CMD_N64_GS_IMPORT);
  await sendReliable(data, onProgress);
  const r = await recvRsp(30000);
  if (r.status !== 0) throw new Error(`GS import failed: status=0x${r.status.toString(16)}`);
  const hostCrc = crc16(data);
  if (hostCrc !== r.value) {
    throw new Error(`CRC mismatch: host=0x${hostCrc.toString(16).toUpperCase()} pico=0x${r.value.toString(16).toUpperCase()}`);
  }
  return { status: r.status, crc: r.value };
}

async function doSetSaveCfg(saveTypeEnum, sizeBytes) {
  const sizeUnits64 = Math.ceil(sizeBytes / 64);
  const r = await cmdRsp(CMD_N64_SET_SAVE_CFG, saveTypeEnum, sizeUnits64 & 0xFFFF);
  if (r.status !== 0) throw new Error(`Set save config failed: status=0x${r.status.toString(16)}`);
  const confirmedType = (r.value >> 12) & 0x0F;
  const confirmedSize = (r.value & 0x0FFF) * 64;
  return { type: confirmedType, typeName: SAVE_TYPES[confirmedType] || 'unknown', size: confirmedSize };
}
