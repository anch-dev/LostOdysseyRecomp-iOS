import { createZip, readSingleZip } from './zip.mjs';

export const MAX_INPUT_BYTES = 16 * 1024 * 1024;
const BLOCK = 4096;
const FANOUT = 170;
const END = 0xffffff;
const encoder = new TextEncoder();

function check(condition, message) {
  if (!condition) throw new Error(message);
}

const hex = bytes => Array.from(bytes, value => value.toString(16).padStart(2, '0')).join('');
const equal = (a, b) => a.length === b.length && a.every((value, index) => value === b[index]);
const digest = async (algorithm, data) => new Uint8Array(await globalThis.crypto.subtle.digest(algorithm, data));
const u24le = bytes => bytes[0] + bytes[1] * 256 + bytes[2] * 65536;

// Layout follows the repository's install::StfsPackage reader. Import verifies
// SHA-1 on the active hash tree as well as each referenced block. No RSA key or
// console/account identifier is needed or included in the output metadata.
async function extractConsoleSave(bytes, slot) {
  const data = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  function range(offset, size) {
    check(Number.isSafeInteger(offset) && offset >= 0 && size >= 0 && offset + size <= bytes.length,
      'The console save is truncated or contains an out-of-range block.');
    return bytes.subarray(offset, offset + size);
  }
  function u32(offset) { range(offset, 4); return data.getUint32(offset); }
  check(bytes.length >= 0xa000 && equal(bytes.subarray(0, 4), encoder.encode('CON ')),
    'Select an original Xbox 360 CON save, not an extracted save.bin or a PC save ZIP.');
  check(u32(0x344) === 1, 'This is not a saved-game package (DLC and profiles are not supported).');
  check(u32(0x360) === 0x4d5307fa, 'This save belongs to a different game. Select a Lost Odyssey save.');
  const headerSize = u32(0x340);
  check(headerSize >= 0x971a && headerSize <= 1024 * 1024, 'Unsupported console save header.');
  const base = Math.ceil(headerSize / BLOCK) * BLOCK;
  check(equal(await digest('SHA-1', range(0x344, base - 0x344)), range(0x32c, 20)),
    'The console save header failed its integrity check.');
  check(u32(0x3a9) === 0 && u32(0x39d) <= 1, 'Only single-file STFS saves are supported.');
  const volume = range(0x379, 0x24);
  check(volume[0] === 0x24 && volume[1] === 0, 'Unsupported STFS volume descriptor.');
  const copies = volume[2] & 1 ? 1 : 2;
  const rootCopy = copies === 2 && volume[2] & 2 ? 1 : 0;
  const total = u32(0x395);
  const directoryCount = volume[3] + volume[4] * 256;
  const directoryStart = u24le(volume.subarray(5, 8));
  check(total > 0 && total <= MAX_INPUT_BYTES / BLOCK && directoryCount > 0 && directoryCount <= 32,
    'The save has an unsupported or excessive block/directory count.');
  const topLevel = total > FANOUT * FANOUT ? 2 : total > FANOUT ? 1 : 0;

  function blockOffset(number) {
    check(Number.isInteger(number) && number >= 0 && number < total, 'Save block is outside the volume.');
    let overhead = 0;
    for (const span of [FANOUT, FANOUT ** 2, FANOUT ** 3]) {
      overhead += (Math.floor(number / span) + 1) * copies;
      if (number < span) break;
    }
    return base + (number + overhead) * BLOCK;
  }
  range(blockOffset(total - 1), BLOCK);
  function tableOffset(number, level) {
    const firstSpan = FANOUT + copies;
    const secondSpan = FANOUT ** 2 + (FANOUT + 1) * copies;
    let physical;
    if (level === 2) physical = secondSpan;
    else if (level === 1) physical = number < FANOUT ** 2 ? firstSpan : Math.floor(number / FANOUT ** 2) * secondSpan + copies;
    else if (number < FANOUT) physical = 0;
    else physical = Math.floor(number / FANOUT) * firstSpan + (Math.floor(number / FANOUT ** 2) + 1) * copies +
      (number >= FANOUT ** 2 ? copies : 0);
    return base + physical * BLOCK;
  }
  const tables = new Map();
  async function table(number, level) {
    const offset = tableOffset(number, level);
    const key = `${level}:${offset}`;
    if (tables.has(key)) return tables.get(key);
    let active = rootCopy;
    let expected = volume.subarray(8, 28);
    if (level !== topLevel) {
      const parent = await table(number, level + 1);
      const index = Math.floor(number / (level === 0 ? FANOUT : FANOUT ** 2)) % FANOUT;
      const record = parent.subarray(index * 24, index * 24 + 24);
      active = copies === 2 && record[20] & 0x40 ? 1 : 0;
      expected = record.subarray(0, 20);
    }
    const value = range(offset + active * BLOCK, BLOCK);
    check(equal(await digest('SHA-1', value), expected), 'A save hash table failed its integrity check.');
    tables.set(key, value);
    return value;
  }
  const claimed = new Set();
  async function chain(start, count) {
    check(count > 0 && count <= total, 'Invalid save block-chain length.');
    const output = new Uint8Array(count * BLOCK);
    let number = start;
    for (let index = 0; index < count; index++) {
      const offset = blockOffset(number);
      check(!claimed.has(number), 'Save contains a cyclic or overlapping block chain.');
      claimed.add(number);
      const hashTable = await table(number, 0);
      const record = hashTable.subarray((number % FANOUT) * 24, (number % FANOUT + 1) * 24);
      check(record[20] & 0x80, 'Save refers to an unallocated block.');
      const block = range(offset, BLOCK);
      check(equal(await digest('SHA-1', block), record.subarray(0, 20)), 'A save data block failed its integrity check.');
      output.set(block, index * BLOCK);
      number = record[21] * 65536 + record[22] * 256 + record[23];
    }
    check(number === END, 'Save block chain exceeds its declared length.');
    return output;
  }

  const directory = await chain(directoryStart, directoryCount);
  const entries = [];
  for (let offset = 0; offset < directory.length; offset += 64) {
    const record = directory.subarray(offset, offset + 64);
    if (record[0] === 0 && (record[40] & 63) === 0) continue;
    entries.push(record);
  }
  check(entries.length === 1, 'This save layout is not supported: expected one save.bin file.');
  const entry = entries[0];
  check((entry[40] & 63) === 8 && equal(entry.subarray(0, 8), encoder.encode('save.bin')) &&
    !(entry[40] & 128) && entry[50] === 255 && entry[51] === 255,
    'This save layout is not supported: expected save.bin in the package root.');
  const length = new DataView(entry.buffer, entry.byteOffset, entry.byteLength).getUint32(52);
  const valid = u24le(entry.subarray(41, 44));
  const allocated = u24le(entry.subarray(44, 47));
  check(length === 206000 && valid === Math.ceil(length / BLOCK) && allocated >= valid && allocated <= total,
    'This Lost Odyssey payload size or block count has not been verified.');
  const payload = (await chain(u24le(entry.subarray(47, 50)), allocated)).slice(0, length);
  check(equal(payload.subarray(0, 12), new Uint8Array([0x4c, 0x4f, 0x53, 0x56, 0x4d, 0x53, 7, 0xfa, 0, 0, 0, 3])),
    'This internal save format has not been verified. The save was not changed.');

  let display = new Uint8Array(256);
  for (let index = 0; index < 9; index++) {
    const candidate = range(0x411 + index * 256, 256);
    if (candidate[0] || candidate[1]) { display = candidate.slice(); break; }
  }
  const displayName = new TextDecoder('utf-16be', { fatal: true }).decode(display).split('\0')[0];
  check(displayName.length < 128, 'Save display name is not terminated.');
  // Only the local listing label is relabelled; game payload bytes are intact.
  const slotNumber = String(Number(slot.slice(4)) + 1).padStart(2, '0');
  if (/^\d{2} /.test(displayName)) { display[1] = slotNumber.charCodeAt(0); display[3] = slotNumber.charCodeAt(1); }
  const metadata = new Uint8Array(308);
  const m = new DataView(metadata.buffer);
  m.setUint32(0, 1);
  m.setUint32(4, 1);
  metadata.set(display, 8);
  metadata.set(encoder.encode(slot), 264);
  const thumbnailSize = u32(0x1712);
  check(thumbnailSize <= 0x4000 && 0x171a + thumbnailSize <= base, 'Invalid save thumbnail size.');
  const thumbnail = range(0x171a, thumbnailSize);
  check(!thumbnailSize || equal(thumbnail.subarray(0, 8), new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10])),
    'The save thumbnail is not a supported PNG.');
  const files = { [`save/${slot}/save.bin`]: payload, [`save/${slot}/.lo-content`]: metadata };
  if (thumbnailSize) files[`save/${slot}/.lo-thumbnail.png`] = thumbnail;
  return { files, payload, displayName: new TextDecoder('utf-16be').decode(display).split('\0')[0] };
}

export async function convertSave(input, filename, { slot = 'user00' } = {}) {
  check(input instanceof Uint8Array && input.length > 0 && input.length <= MAX_INPUT_BYTES, 'Select a nonempty file up to 16 MiB.');
  check(/^user(?:[01][0-9]|2[0-9])$/.test(slot), 'Choose a save slot from 01 to 30.');
  check(globalThis.crypto?.subtle, 'Secure browser APIs are unavailable. Open this page over HTTPS in a current browser.');
  let bytes = new Uint8Array(input);
  let sourceName = String(filename || 'console-save').split(/[\\/]/).at(-1).slice(0, 255);
  if (bytes[0] === 0x50 && bytes[1] === 0x4b) {
    const unpacked = await readSingleZip(bytes, MAX_INPUT_BYTES);
    bytes = unpacked.data;
    sourceName = unpacked.name;
  }
  const { files, payload, displayName } = await extractConsoleSave(bytes, slot);
  const summary = { sourceName, displayName, slot, payloadBytes: payload.length, payloadSha256: hex(await digest('SHA-256', payload)) };
  files['conversion.json'] = encoder.encode(JSON.stringify({
    converter: 'Lost Odyssey Recomp save converter', formatVersion: 1,
    ...summary, sourceSha256: hex(await digest('SHA-256', bytes)),
    titleId: '4D5307FA', payloadModified: false, rsaSignatureVerified: false,
  }, null, 2) + '\n');
  files['README.txt'] = encoder.encode(`Lost Odyssey Recomp — Xbox 360 save import\n\n` +
    `1. Close the game and back up your existing save folder.\n` +
    `2. Copy save/${slot} into your Recomp save folder. Choose an EMPTY slot; do not overwrite an existing folder.\n` +
    `3. Start the game and load slot ${Number(slot.slice(4)) + 1}.\n\n` +
    `If this slot is occupied, convert again with a different target slot.\n` +
    `Windows portable installs use the save folder beside the game executable.\n` +
    `Linux installations may use the app data directory; follow the project's installation guide.\n` +
    `The save.bin payload is unchanged. This tool imports Xbox 360 saves into Recomp; it does not export back to a console.\n` +
    `Only the submitted file was processed, locally in your browser. No save was uploaded.\n\n` +
    `中文说明\n关闭游戏并备份已有 save 文件夹，将 save/${slot} 放入 Recomp 的存档目录。请使用空槽位，不要覆盖已有文件夹。\n` +
    `如果目标槽位已占用，请在网页选择其他槽位重新转换。内部 save.bin 未修改；目前只支持导入 Recomp，不支持回写主机。\n` +
    `https://github.com/freefrank/LostOdysseyRecomp\n`);
  return { zip: createZip(files), downloadName: `lost-odyssey-${slot}.zip`, summary };
}
