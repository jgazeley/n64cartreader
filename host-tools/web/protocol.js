// PPK1 protocol: constants, CRC16, serial pump, framing, reliable transfer

// ── Constants ──
const CMD_MAGIC = new Uint8Array([0x50, 0x50, 0x4B, 0x31]); // PPK1
const RSP_MAGIC = new Uint8Array([0x52, 0x50, 0x4B, 0x31]); // RPK1
const SYNC_A = 0xAA;
const SYNC_B = 0x55;
const ACK = 0x06;
const NAK = 0x15;

const CMD_PING            = 0x01;
const CMD_GET_VERSION     = 0x03;
const CMD_N64_RESCAN      = 0x40;
const CMD_N64_STATUS      = 0x41;
const CMD_N64_EXPORT_SAVE = 0x42;
const CMD_N64_IMPORT_SAVE = 0x43;
const CMD_N64_ROM_INFO       = 0x44;
const CMD_N64_EXPORT_ROM     = 0x45;
const CMD_N64_EXPORT_HEADER  = 0x46;
const CMD_N64_EXPORT_MPK     = 0x49;
const CMD_N64_IMPORT_MPK     = 0x4A;
const CMD_N64_GS_INFO        = 0x4B;
const CMD_N64_GS_EXPORT      = 0x4C;
const CMD_N64_GS_IMPORT      = 0x4E;
const CMD_N64_SET_SAVE_CFG   = 0x4F;

const SAVE_TYPES = {0:'none',1:'sram',2:'flashram',3:'eeprom4k',4:'eeprom16k',5:'unknown'};
const SAVE_CONFIGS = {
  'none':      { type: 0, size: 0 },
  'sram':      { type: 1, size: 32768 },
  'sram96k':   { type: 1, size: 98304 },
  'flashram':  { type: 2, size: 131072 },
  'eeprom4k':  { type: 3, size: 512 },
  'eeprom16k': { type: 4, size: 2048 },
};

const CHUNK_SIZE = 512;

// ── CRC16 ──
function crc16(data) {
  let crc = 0;
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i] << 8;
    for (let j = 0; j < 8; j++) {
      crc = (crc & 0x8000) ? ((crc << 1) ^ 0x8005) & 0xFFFF : (crc << 1) & 0xFFFF;
    }
  }
  return crc;
}

// ── Serial transport ──
// Single read pump feeds rxBuf; higher-level code awaits data via waitForAvailable/waitForBytes.

let _reader = null;
let _writer = null;
let rxBuf = new Uint8Array(0);
let rxWaiters = [];
let pumpRunning = false;
let logFn = () => {};
let verboseFn = () => false;

function initTransport(opts) {
  _reader = opts.reader;
  _writer = opts.writer;
  logFn = opts.log || (() => {});
  verboseFn = opts.verbose || (() => false);
  rxBuf = new Uint8Array(0);
  rxWaiters = [];
}

function clearTransport() {
  _reader = null;
  _writer = null;
  rxBuf = new Uint8Array(0);
  rxWaiters = [];
}

function appendBuf(existing, newData) {
  const merged = new Uint8Array(existing.length + newData.length);
  merged.set(existing);
  merged.set(newData, existing.length);
  return merged;
}

async function startReadPump() {
  pumpRunning = true;
  try {
    while (pumpRunning && _reader) {
      const { value, done } = await _reader.read();
      if (done) break;
      if (value && value.length > 0) {
        if (verboseFn()) {
          const hex = Array.from(value).map(b => b.toString(16).padStart(2,'0')).join(' ');
          logFn(`RX [${value.length}]: ${hex}`);
        }
        rxBuf = appendBuf(rxBuf, value);
        const waiters = rxWaiters.splice(0);
        for (const w of waiters) w();
      }
    }
  } catch (e) {
    if (pumpRunning) logFn(`Read pump error: ${e.message}`, 'err');
  }
  pumpRunning = false;
}

function stopReadPump() {
  pumpRunning = false;
  const waiters = rxWaiters.splice(0);
  for (const w of waiters) w();
}

function consumeBuf(n) {
  const out = rxBuf.slice(0, n);
  rxBuf = rxBuf.slice(n);
  return out;
}

function waitForAvailable(n, timeout = 5000) {
  return new Promise((resolve, reject) => {
    if (rxBuf.length >= n) { resolve(); return; }
    const deadline = Date.now() + timeout;
    const timer = setTimeout(() => {
      rxWaiters = rxWaiters.filter(w => w !== check);
      reject(new Error(`Timeout waiting for ${n} available bytes (have ${rxBuf.length})`));
    }, timeout);
    function check() {
      if (rxBuf.length >= n) {
        clearTimeout(timer);
        resolve();
      } else if (Date.now() >= deadline) {
        clearTimeout(timer);
        reject(new Error(`Timeout waiting for ${n} available bytes (have ${rxBuf.length})`));
      } else {
        rxWaiters.push(check);
      }
    }
    rxWaiters.push(check);
  });
}

async function drainFlush() {
  let prevLen = -1;
  while (rxBuf.length !== prevLen) {
    prevLen = rxBuf.length;
    await new Promise(r => setTimeout(r, 50));
  }
  rxBuf = new Uint8Array(0);
}

// ── Command framing ──
async function sendCmd(cmd, arg0 = 0, arg1 = 0) {
  const frame = new Uint8Array(8);
  frame.set(CMD_MAGIC);
  frame[4] = cmd;
  frame[5] = arg0;
  frame[6] = (arg1 >> 8) & 0xFF;
  frame[7] = arg1 & 0xFF;
  if (verboseFn()) {
    const hex = Array.from(frame).map(b => b.toString(16).padStart(2,'0')).join(' ');
    logFn(`TX [${frame.length}]: ${hex}`);
  }
  await _writer.write(frame);
}

async function recvRsp(timeout = 5000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    try {
      await waitForAvailable(1, Math.max(100, deadline - Date.now()));
    } catch { break; }

    let found = -1;
    for (let i = 0; i <= rxBuf.length - 4; i++) {
      if (rxBuf[i] === 0x52 && rxBuf[i+1] === 0x50 && rxBuf[i+2] === 0x4B && rxBuf[i+3] === 0x31) {
        found = i;
        break;
      }
    }
    if (found < 0) {
      if (rxBuf.length > 3) rxBuf = rxBuf.slice(rxBuf.length - 3);
      continue;
    }
    if (found > 0) rxBuf = rxBuf.slice(found);
    if (rxBuf.length < 8) {
      try {
        await waitForAvailable(8, Math.max(100, deadline - Date.now()));
      } catch { break; }
      if (rxBuf.length < 8) continue;
    }
    const frame = consumeBuf(8);
    return {
      cmd: frame[4],
      status: frame[5],
      value: (frame[6] << 8) | frame[7]
    };
  }
  throw new Error('Timeout waiting for response');
}

async function cmdRsp(cmd, arg0 = 0, arg1 = 0, timeout = 10000) {
  await drainFlush();
  await sendCmd(cmd, arg0, arg1);
  return await recvRsp(timeout);
}

// ── Reliable receive ──
async function recvReliable(totalSize, onProgress) {
  const out = new Uint8Array(totalSize);
  let written = 0;
  let expectedSeq = 0;
  const totalChunks = Math.ceil(totalSize / CHUNK_SIZE);
  let chunks = 0;

  while (written < totalSize) {
    await waitForAvailable(1, 10000);

    while (rxBuf.length > 0) {
      if (rxBuf[0] === 0x52 && rxBuf.length >= 4 &&
          rxBuf[1] === 0x50 && rxBuf[2] === 0x4B && rxBuf[3] === 0x31) {
        if (rxBuf.length < 8) { await waitForAvailable(8, 5000); }
        const frame = consumeBuf(8);
        if (frame[5] !== 0) throw new Error(`Device error during transfer: status=0x${frame[5].toString(16)}`);
        continue;
      }
      if (rxBuf[0] === SYNC_A) {
        if (rxBuf.length < 2) await waitForAvailable(2, 5000);
        if (rxBuf[1] === SYNC_B) break;
      }
      consumeBuf(1);
    }

    await waitForAvailable(5, 5000);
    const hdr = consumeBuf(5);
    const seq = hdr[2];
    const payloadLen = (hdr[3] << 8) | hdr[4];

    await waitForAvailable(payloadLen + 2, 5000);
    const payload = consumeBuf(payloadLen);
    const crcBytes = consumeBuf(2);
    const rxCrc = (crcBytes[0] << 8) | crcBytes[1];

    if (rxCrc !== crc16(payload)) {
      await _writer.write(new Uint8Array([NAK]));
      continue;
    }

    if (seq === expectedSeq) {
      const remain = totalSize - written;
      const block = payload.slice(0, Math.min(payload.length, remain));
      out.set(block, written);
      written += block.length;
      await _writer.write(new Uint8Array([ACK]));
      expectedSeq = (expectedSeq + 1) & 0xFF;
      chunks++;
      if (onProgress) onProgress(chunks, totalChunks);
    } else if (seq === ((expectedSeq - 1 + 256) & 0xFF)) {
      await _writer.write(new Uint8Array([ACK]));
    } else {
      await _writer.write(new Uint8Array([NAK]));
    }
  }
  return out;
}

// ── Reliable send ──
const N64_MEMPAK_SIZE = 32768;

async function sendReliable(data, onProgress, chunkDelay = 0) {
  const totalChunks = Math.ceil(data.length / CHUNK_SIZE);
  let seq = 0;

  for (let idx = 0; idx < totalChunks; idx++) {
    const start = idx * CHUNK_SIZE;
    const end = Math.min(start + CHUNK_SIZE, data.length);
    const chunk = data.slice(start, end);
    const chunkCrc = crc16(chunk);

    const frame = new Uint8Array(5 + chunk.length + 2);
    frame[0] = SYNC_A;
    frame[1] = SYNC_B;
    frame[2] = seq;
    frame[3] = (chunk.length >> 8) & 0xFF;
    frame[4] = chunk.length & 0xFF;
    frame.set(chunk, 5);
    frame[5 + chunk.length] = (chunkCrc >> 8) & 0xFF;
    frame[5 + chunk.length + 1] = chunkCrc & 0xFF;

    let tries = 0;
    while (tries < 5) {
      await _writer.write(frame);
      try {
        await waitForAvailable(1, 3000);
      } catch { tries++; continue; }

      let gotAck = false;
      while (rxBuf.length > 0) {
        const b = consumeBuf(1)[0];
        if (b === ACK) { gotAck = true; break; }
        if (b === NAK) break;
      }
      if (gotAck) break;
      tries++;
      await new Promise(r => setTimeout(r, 40));
    }
    if (tries >= 5) throw new Error(`Send chunk ${idx+1}/${totalChunks} failed after retries`);

    seq = (seq + 1) & 0xFF;
    if (onProgress) onProgress(idx + 1, totalChunks);
    if (chunkDelay > 0 && idx < totalChunks - 1) {
      await new Promise(r => setTimeout(r, chunkDelay));
    }
  }
}
