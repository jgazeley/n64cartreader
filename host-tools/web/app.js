// UI wiring and application state

// ── State ──
let port = null;
let reader = null;
let writer = null;
let connected = false;
let saveType = 0;
let saveSize = 0;
let romSize = 0;
let gsFlashSize = 0;
let cartMode = 'none'; // 'none', 'game', 'gameshark'
let verbose = false;

// ── Cart-swap safety state ──
let cartReady = false;   // true only after successful rescan + configure
let prevCartId = null;    // gameId of the previous cart (for swap detection)
let prevSaveType = 0;     // save type of the previous cart
let headerTitle = '';     // raw title from ROM header (used for download filenames)

// ── DOM helpers ──
const $ = id => document.getElementById(id);
const logEl = $('log');

function log(msg, level = 'info') {
  const ts = new Date().toLocaleTimeString();
  const color = level === 'err' ? '#ff4455' : level === 'ok' ? '#00d060' : '#8878a0';
  logEl.innerHTML += `<span style="color:${color}">[${ts}] ${msg}</span>\n`;
  logEl.scrollTop = logEl.scrollHeight;
}

function updateSaveButtons() {
  const ready = connected && cartReady;
  // Export/import gated on cartReady AND valid save config
  $('btn-export').disabled = !(ready && saveSize > 0);
  // ROM dump gated on cartReady
  $('btn-dump-rom').disabled = !ready;
  // GS export gated on cartReady
  $('btn-gs-export').disabled = !(ready && gsFlashSize > 0);
}

function setConnected(state) {
  connected = state;
  $('status-dot').className = `status-dot ${state ? 'on' : 'off'}`;
  $('btn-connect').disabled = state;
  $('btn-disconnect').disabled = !state;
  $('btn-rescan').disabled = !state;
  $('btn-set-save').disabled = !state;
  $('btn-export-mpk').disabled = !state;
  $('conn-info').style.display = state ? '' : 'none';
  if (!state) {
    cartReady = false;
    prevCartId = null;
    prevSaveType = 0;
    setCartMode('none');
  }
  updateSaveButtons();
}

function setCartMode(mode) {
  cartMode = mode;
  $('save-section').style.display = mode === 'game' ? '' : 'none';
  $('gs-section').style.display = mode === 'gameshark' ? '' : 'none';
  $('card-data').style.display = mode === 'none' ? 'none' : '';
}

function resetCartInfo() {
  $('cart-title').textContent = '—';
  $('cart-id').textContent = '—';
  $('cart-rom-size').textContent = '—';
  $('cart-save').textContent = '—';
  $('cart-size').textContent = '—';
  // Preserve previous cart info for swap detection before clearing
  prevSaveType = saveType;
  saveType = 0;
  saveSize = 0;
  romSize = 0;
  gsFlashSize = 0;
  headerTitle = '';
  cartReady = false;
  updateSaveButtons();
}

function formatBytes(bytes) {
  if (bytes <= 0) return '0 B';
  if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${bytes} B`;
}

function showProgress(label, done, total, detail = '') {
  const panel = $('progress-panel');
  const pct = total > 0 ? Math.round((done / total) * 100) : 0;
  panel.style.display = '';
  $('progress-label').textContent = label;
  $('progress-pct').textContent = `${Math.max(0, Math.min(100, pct))}%`;
  $('progress-fill').style.width = `${Math.max(0, Math.min(100, pct))}%`;
  $('progress-detail').textContent = detail;
}

function hideProgress() {
  $('progress-panel').style.display = 'none';
}

function beginProgress(label, totalBytes) {
  const totalChunks = Math.max(1, Math.ceil(totalBytes / CHUNK_SIZE));
  showProgress(label, 0, totalChunks, `0 / ${totalChunks} chunks • 0 B / ${formatBytes(totalBytes)}`);
}

function makeChunkProgress(label, totalBytes) {
  return (doneChunks, totalChunks) => {
    const doneBytes = totalChunks > 0 ? Math.round((doneChunks / totalChunks) * totalBytes) : 0;
    const detail = `${doneChunks} / ${totalChunks} chunks • ${formatBytes(doneBytes)} / ${formatBytes(totalBytes)}`;
    showProgress(label, doneChunks, totalChunks, detail);
  };
}

function filenameStem() {
  return (headerTitle || $('cart-title').textContent).trim().replace(/\s+/g, '_') || 'save';
}

function timestampStem(date = new Date()) {
  const pad = value => String(value).padStart(2, '0');
  return (
    `${date.getFullYear()}${pad(date.getMonth() + 1)}${pad(date.getDate())}_` +
    `${pad(date.getHours())}${pad(date.getMinutes())}${pad(date.getSeconds())}`
  );
}

function downloadBlob(data, filename) {
  const blob = new Blob([data], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

// ── Catalog loading ──
async function loadCatalog() {
  if (typeof window.N64_CATALOG_TEXT === 'string' && window.N64_CATALOG_TEXT.length > 0) {
    const count = setCatalog(parseN64Txt(window.N64_CATALOG_TEXT));
    log(`Bundled catalog loaded: ${count} entries`, 'ok');
    $('catalog-status').textContent = `${count} games loaded`;
    $('catalog-drop').style.display = 'none';
    return;
  }

  try {
    const resp = await fetch('n64.txt');
    if (resp.ok) {
      const text = await resp.text();
      const count = setCatalog(parseN64Txt(text));
      log(`Catalog loaded: ${count} entries from n64.txt`, 'ok');
      $('catalog-status').textContent = `${count} games loaded`;
      $('catalog-drop').style.display = 'none';
      return;
    }
  } catch (e) {
    if (window.location.protocol === 'file:') {
      log(
        'Browser blocked automatic n64.txt loading from file://. Drop n64.txt below to load it manually.',
        'info'
      );
      return;
    }
  }
  log('n64.txt not found — drop it below or select save type manually', 'info');
}

function loadCatalogFromFile(file) {
  const fr = new FileReader();
  fr.onload = () => {
    const count = setCatalog(parseN64Txt(fr.result));
    log(`Catalog loaded: ${count} entries from ${file.name}`, 'ok');
    $('catalog-status').textContent = `${count} games loaded`;
    $('catalog-drop').style.display = 'none';
  };
  fr.readAsText(file);
}

// ── Cart rescan + auto-configure ──
async function doRescanAndUpdate() {
  try {
    log('Rescanning cart...');
    resetCartInfo();

    // Safety: clear stale save config on firmware before rescan.
    // This ensures no leftover FlashRAM state survives across cart swaps.
    // If the previous cart was FlashRAM, the firmware may still have
    // FlashRAM bus state armed — clearing to NONE prevents that from
    // interacting with the new cart during rescan.
    try {
      await doSetSaveCfg(0, 0);
    } catch {
      // OK if this fails (e.g. first connect, no cart yet)
    }

    await doRescan();

    const hdr = await doReadHeader();

    // No cart detected
    if (isEmptyHeader(hdr)) {
      $('cart-title').textContent = 'No cartridge detected';
      setCartMode('none');
      prevCartId = null;
      log('No cartridge detected', 'err');
      return;
    }

    // GameShark detected
    if (isGameSharkHeader(hdr)) {
      $('cart-title').textContent = 'GameShark / Action Replay';
      $('cart-id').textContent = '—';
      setCartMode('gameshark');

      // Detect cart swap
      if (prevCartId !== null && prevCartId !== '__gameshark__') {
        log(`Cart changed: ${prevCartId} -> GameShark`, 'info');
      }
      prevCartId = '__gameshark__';

      try {
        const info = await doGsInfo();
        if (info.present && info.sizeBytes > 0) {
          gsFlashSize = info.sizeBytes;
          $('gs-flash-size').textContent = `${(gsFlashSize / 1024).toFixed(0)} KB`;
          log(`GameShark detected — flash: ${(gsFlashSize / 1024).toFixed(0)} KB, mfg=0x${info.mfgId.toString(16).toUpperCase()}`, 'ok');
          cartReady = true;
          updateSaveButtons();
        } else {
          log('GameShark detected but flash probe failed', 'err');
        }
      } catch (e) {
        log(`GameShark probe error: ${e.message}`, 'err');
      }
      return;
    }

    // Normal game cart
    const info = parseHeader(hdr);
    const match = lookupCatalog(hdr);
    const displayTitle = (match && match.title) || info.title || '(empty)';
    headerTitle = info.title || '';
    $('cart-title').textContent = displayTitle;
    $('cart-id').textContent = info.gameId || '—';
    setCartMode('game');

    // Detect cart swap and warn about dangerous transitions
    const newCartId = info.gameId || null;
    if (prevCartId !== null && prevCartId !== newCartId) {
      log(`Cart changed: ${prevCartId} -> ${newCartId}`, 'info');
      if (prevSaveType === 2) {
        // Previous cart was FlashRAM — this is the dangerous transition
        log('WARNING: Previous cart used FlashRAM. Verify save data integrity after this swap.', 'err');
      }
    }
    prevCartId = newCartId;

    if (match && match.save && SAVE_CONFIGS[match.save]) {
      const cfg = SAVE_CONFIGS[match.save];
      const confirmed = await doSetSaveCfg(cfg.type, cfg.size);
      saveType = confirmed.type;
      saveSize = confirmed.size;
      log(`Auto-configured: ${match.save} (matched: ${match.title})`, 'ok');
    } else {
      const st = await doStatus();
      saveType = st.type;
      saveSize = st.size;
      if (saveSize === 0) {
        const reason = isCatalogLoaded() ? 'not in catalog' : 'no catalog loaded';
        log(`Unknown cart [${info.gameId}] — ${reason}, select save type manually`, 'err');
      }
    }

    try {
      romSize = await doRomInfo();
      $('cart-rom-size').textContent = romSize > 0 ? `${(romSize / 1048576).toFixed(0)} MB` : '—';
    } catch { romSize = 0; }

    $('cart-save').textContent = SAVE_TYPES[saveType] || 'unknown';
    $('cart-size').textContent = saveSize > 0 ? `${saveSize} bytes` : '—';

    // Mark cart as ready for save operations
    cartReady = true;
    updateSaveButtons();

    log(`Cart: ${displayTitle} [${info.gameId}] — ${SAVE_TYPES[saveType] || '?'} (${saveSize}B)`, 'ok');
  } catch (e) {
    log(`Rescan error: ${e.message}`, 'err');
  }
}

// ── Event handlers ──

$('btn-connect').addEventListener('click', async () => {
  try {
    port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: 0x2E8A }]
    });
    await port.open({ baudRate: 115200 });
    reader = port.readable.getReader();
    writer = port.writable.getWriter();

    initTransport({ reader, writer, log, verbose: () => verbose });
    startReadPump();

    await new Promise(r => setTimeout(r, 500));

    log('Connected to Pico');
    $('port-name').textContent = 'Web Serial';

    const pong = await doPing();
    log(`PONG: 0x${pong.toString(16).toUpperCase()}`, 'ok');

    const ver = await doGetVersion();
    $('fw-version').textContent = ver || '(not available)';
    if (ver) log(`Firmware: ${ver}`, 'ok');

    setConnected(true);
    await doRescanAndUpdate();
  } catch (e) {
    log(`Connect error: ${e.message}`, 'err');
  }
});

$('btn-disconnect').addEventListener('click', async () => {
  try {
    stopReadPump();
    if (reader) { await reader.cancel(); reader.releaseLock(); reader = null; }
    if (writer) { writer.releaseLock(); writer = null; }
    if (port) { await port.close(); port = null; }
  } catch {}
  clearTransport();
  setConnected(false);
  resetCartInfo();
  log('Disconnected');
});

$('btn-rescan').addEventListener('click', doRescanAndUpdate);

$('btn-set-save').addEventListener('click', async () => {
  const hint = $('save-type-select').value;
  if (!hint || !SAVE_CONFIGS[hint]) { log('Select a save type first', 'err'); return; }
  try {
    const cfg = SAVE_CONFIGS[hint];
    const confirmed = await doSetSaveCfg(cfg.type, cfg.size);
    saveType = confirmed.type;
    saveSize = confirmed.size;
    $('cart-save').textContent = confirmed.typeName;
    $('cart-size').textContent = confirmed.size > 0 ? `${confirmed.size} bytes` : '—';
    cartReady = true;
    updateSaveButtons();
    log(`Save configured: ${hint} (${confirmed.size}B)`, 'ok');
  } catch (e) {
    log(`Set save error: ${e.message}`, 'err');
  }
});


$('btn-export').addEventListener('click', async () => {
  try {
    if (!cartReady) { log('Rescan the cart first before exporting', 'err'); return; }
    if (saveSize === 0) { log('No save to export (size=0)', 'err'); return; }
    log(`Exporting ${saveSize} bytes...`);
    beginProgress('Exporting Save', saveSize);
    const data = await doExportSave(saveSize, makeChunkProgress('Exporting Save', saveSize));
    hideProgress();
    const crc = crc16(data);
    downloadBlob(data, `${filenameStem()}.sav`);
    log(`Exported ${data.length} bytes, CRC16=0x${crc.toString(16).toUpperCase()}`, 'ok');
  } catch (e) {
    hideProgress();
    log(`Export error: ${e.message}`, 'err');
  }
});

$('btn-dump-rom').addEventListener('click', async () => {
  try {
    if (romSize === 0) { log('No ROM detected (size=0)', 'err'); return; }
    const mb = (romSize / 1048576).toFixed(0);
    log(`Dumping ROM (${mb} MB)... this will take a while`);
    beginProgress('Dumping ROM', romSize);
    const data = await doExportRom(romSize, makeChunkProgress('Dumping ROM', romSize));
    hideProgress();
    const crc = crc16(data);
    downloadBlob(data, `${filenameStem()}.z64`);
    log(`ROM dumped ${data.length} bytes, CRC16=0x${crc.toString(16).toUpperCase()}`, 'ok');
  } catch (e) {
    hideProgress();
    log(`ROM dump error: ${e.message}`, 'err');
  }
});

$('btn-export-mpk').addEventListener('click', async () => {
  try {
    log('Exporting Memory Pak (32 KB)...');
    beginProgress('Exporting Memory Pak', 32768);
    const data = await doExportMpk(makeChunkProgress('Exporting Memory Pak', 32768));
    hideProgress();
    const crc = crc16(data);
    downloadBlob(data, `controller_pak_${timestampStem()}.mpk`);
    log(`MPK exported ${data.length} bytes, CRC16=0x${crc.toString(16).toUpperCase()}`, 'ok');
  } catch (e) {
    hideProgress();
    log(`MPK export error: ${e.message}`, 'err');
  }
});

// GameShark export
$('btn-gs-export').addEventListener('click', async () => {
  try {
    if (!cartReady) { log('Rescan the cart first', 'err'); return; }
    if (gsFlashSize === 0) { log('No GameShark flash detected', 'err'); return; }
    log(`Exporting GameShark flash (${(gsFlashSize / 1024).toFixed(0)} KB)...`);
    beginProgress('Exporting GameShark Flash', gsFlashSize);
    const data = await doGsExport(gsFlashSize, makeChunkProgress('Exporting GameShark Flash', gsFlashSize));
    hideProgress();
    const crc = crc16(data);
    downloadBlob(data, 'gameshark.bin');
    log(`GS flash exported ${data.length} bytes, CRC16=0x${crc.toString(16).toUpperCase()}`, 'ok');
  } catch (e) {
    hideProgress();
    log(`GS export error: ${e.message}`, 'err');
  }
});

// GameShark import drop zone
const gsDropZone = $('gs-drop-zone');
const gsFileInput = $('gs-file-input');

gsDropZone.addEventListener('click', () => gsFileInput.click());
gsDropZone.addEventListener('dragover', e => { e.preventDefault(); gsDropZone.classList.add('dragover'); });
gsDropZone.addEventListener('dragleave', () => gsDropZone.classList.remove('dragover'));
gsDropZone.addEventListener('drop', e => {
  e.preventDefault();
  gsDropZone.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) handleGsImport(e.dataTransfer.files[0]);
});
gsFileInput.addEventListener('change', () => {
  if (gsFileInput.files.length > 0) handleGsImport(gsFileInput.files[0]);
  gsFileInput.value = '';
});

async function handleGsImport(file) {
  if (!connected) { log('Not connected', 'err'); return; }
  if (!cartReady) { log('Rescan the cart first', 'err'); return; }
  if (gsFlashSize === 0) { log('No GameShark flash detected', 'err'); return; }
  try {
    let buf = new Uint8Array(await file.arrayBuffer());
    if (buf.length > gsFlashSize) {
      log(`File too large (${buf.length} > ${gsFlashSize})`, 'err');
      return;
    }
    if ((buf.length & 1) !== 0) {
      log('File must be word-aligned (even size)', 'err');
      return;
    }
    // Pad to full flash size with 0xFF
    if (buf.length < gsFlashSize) {
      const padded = new Uint8Array(gsFlashSize);
      padded.fill(0xFF);
      padded.set(buf);
      buf = padded;
    }
    log(`Flashing GameShark (${(buf.length / 1024).toFixed(0)} KB)...`);
    beginProgress('Importing GameShark Flash', buf.length);
    const r = await doGsImport(buf, makeChunkProgress('Importing GameShark Flash', buf.length));
    hideProgress();
    log(`GS flash imported OK, CRC16=0x${r.crc.toString(16).toUpperCase()}`, 'ok');
  } catch (e) {
    hideProgress();
    log(`GS import error: ${e.message}`, 'err');
  }
}

// MPK import drop zone
const mpkDropZone = $('mpk-drop-zone');
const mpkFileInput = $('mpk-file-input');

mpkDropZone.addEventListener('click', () => mpkFileInput.click());
mpkDropZone.addEventListener('dragover', e => { e.preventDefault(); mpkDropZone.classList.add('dragover'); });
mpkDropZone.addEventListener('dragleave', () => mpkDropZone.classList.remove('dragover'));
mpkDropZone.addEventListener('drop', e => {
  e.preventDefault();
  mpkDropZone.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) handleImportMpk(e.dataTransfer.files[0]);
});
mpkFileInput.addEventListener('change', () => {
  if (mpkFileInput.files.length > 0) handleImportMpk(mpkFileInput.files[0]);
  mpkFileInput.value = '';
});

async function handleImportMpk(file) {
  if (!connected) { log('Not connected', 'err'); return; }
  try {
    const buf = new Uint8Array(await file.arrayBuffer());
    log(`Importing MPK ${file.name} (${buf.length} bytes)...`);
    beginProgress('Importing Memory Pak', buf.length);
    const r = await doImportMpk(buf, makeChunkProgress('Importing Memory Pak', buf.length));
    hideProgress();
    log(`MPK imported OK, CRC16=0x${r.crc.toString(16).toUpperCase()}`, 'ok');
  } catch (e) {
    hideProgress();
    log(`MPK import error: ${e.message}`, 'err');
  }
}

// Save import drop zone
const dropZone = $('drop-zone');
const fileInput = $('file-input');

dropZone.addEventListener('click', () => fileInput.click());
dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', e => {
  e.preventDefault();
  dropZone.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) handleImportFile(e.dataTransfer.files[0]);
});
fileInput.addEventListener('change', () => {
  if (fileInput.files.length > 0) handleImportFile(fileInput.files[0]);
  fileInput.value = '';
});

async function handleImportFile(file) {
  if (!connected) { log('Not connected', 'err'); return; }
  if (!cartReady) { log('Rescan the cart first before importing', 'err'); return; }
  if (saveSize === 0) { log('No save type configured — set save type first', 'err'); return; }
  try {
    const buf = new Uint8Array(await file.arrayBuffer());
    log(`Importing ${file.name} (${buf.length} bytes)...`);
    beginProgress('Importing Save', buf.length);
    const r = await doImportSave(buf, makeChunkProgress('Importing Save', buf.length));
    hideProgress();
    log(`Imported OK, CRC16=0x${r.crc.toString(16).toUpperCase()}`, 'ok');
  } catch (e) {
    hideProgress();
    log(`Import error: ${e.message}`, 'err');
  }
}

// Verbose toggle
$('chk-verbose').addEventListener('change', e => { verbose = e.target.checked; });

// Catalog drop zone
const catalogDrop = $('catalog-drop');
const catalogInput = $('catalog-input');
catalogDrop.addEventListener('click', () => catalogInput.click());
catalogDrop.addEventListener('dragover', e => { e.preventDefault(); catalogDrop.classList.add('dragover'); });
catalogDrop.addEventListener('dragleave', () => catalogDrop.classList.remove('dragover'));
catalogDrop.addEventListener('drop', e => {
  e.preventDefault();
  catalogDrop.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) loadCatalogFromFile(e.dataTransfer.files[0]);
});
catalogInput.addEventListener('change', () => {
  if (catalogInput.files.length > 0) loadCatalogFromFile(catalogInput.files[0]);
  catalogInput.value = '';
});

// ── Init ──
if (!('serial' in navigator)) {
  $('app').style.display = 'none';
  $('no-serial').style.display = '';
} else {
  log('Ready — click Connect to begin');
  loadCatalog();
}
