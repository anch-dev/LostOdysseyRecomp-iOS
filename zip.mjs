// Small ZIP boundary: stored/deflate input, stored output. No filesystem paths
// from an uploaded archive are ever used as output paths.
const encoder = new TextEncoder();
const crcTable = Uint32Array.from({ length: 256 }, (_, n) => {
  for (let bit = 0; bit < 8; bit++) n = (n >>> 1) ^ ((n & 1) ? 0xedb88320 : 0);
  return n >>> 0;
});

export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) crc = (crc >>> 8) ^ crcTable[(crc ^ byte) & 255];
  return (crc ^ 0xffffffff) >>> 0;
}

function check(condition, message) {
  if (!condition) throw new Error(message);
}

function view(bytes) {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

async function inflateBounded(bytes, expected, limit) {
  let stream;
  try {
    stream = new DecompressionStream('deflate-raw');
  } catch {
    throw new Error('This browser cannot unzip this file. Extract the ZIP first and select the original console save.');
  }
  const reader = new Blob([bytes]).stream().pipeThrough(stream).getReader();
  const output = new Uint8Array(expected);
  let size = 0;
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      check(size + value.length <= expected && size + value.length <= limit,
        'ZIP expands beyond its declared size or the conversion limit.');
      output.set(value, size);
      size += value.length;
    }
    check(size === expected, 'ZIP entry has an incorrect uncompressed size.');
    return output;
  } catch (error) {
    await reader.cancel().catch(() => {});
    throw new Error(`Could not decompress ZIP: ${error.message}`);
  } finally {
    reader.releaseLock();
  }
}

export async function readSingleZip(bytes, limit) {
  check(bytes.length >= 22 && bytes.length <= limit, 'ZIP is truncated or too large.');
  const data = view(bytes);
  let end = -1;
  for (let p = bytes.length - 22; p >= Math.max(0, bytes.length - 65557); p--) {
    if (data.getUint32(p, true) === 0x06054b50 && p + 22 + data.getUint16(p + 20, true) === bytes.length) {
      end = p;
      break;
    }
  }
  check(end >= 0, 'ZIP end record is missing.');
  const entries = data.getUint16(end + 10, true);
  const directorySize = data.getUint32(end + 12, true);
  const directoryStart = data.getUint32(end + 16, true);
  check(data.getUint16(end + 4, true) === 0 && data.getUint16(end + 6, true) === 0 &&
    data.getUint16(end + 8, true) === entries, 'Split ZIP archives are not supported.');
  check(entries > 0 && entries <= 128 && directoryStart + directorySize === end,
    'ZIP64, empty or oversized ZIP directories are not supported.');
  let cursor = directoryStart;
  const candidates = [];
  const names = new Set();
  for (let index = 0; index < entries; index++) {
    check(cursor + 46 <= end && data.getUint32(cursor, true) === 0x02014b50, 'Invalid ZIP directory entry.');
    const flags = data.getUint16(cursor + 8, true);
    const method = data.getUint16(cursor + 10, true);
    const checksum = data.getUint32(cursor + 16, true);
    const compressed = data.getUint32(cursor + 20, true);
    const uncompressed = data.getUint32(cursor + 24, true);
    const nameLength = data.getUint16(cursor + 28, true);
    const extraLength = data.getUint16(cursor + 30, true);
    const commentLength = data.getUint16(cursor + 32, true);
    const local = data.getUint32(cursor + 42, true);
    const next = cursor + 46 + nameLength + extraLength + commentLength;
    check(next <= end && nameLength > 0 && nameLength <= 1024, 'Invalid ZIP filename length.');
    check(!(flags & (1 | 64)) && data.getUint16(cursor + 34, true) === 0,
      'Encrypted or split ZIP archives are not supported.');
    const nameBytes = bytes.subarray(cursor + 46, cursor + 46 + nameLength);
    const name = new TextDecoder('utf-8', { fatal: !!(flags & 2048) }).decode(nameBytes);
    const parts = name.split('/');
    check(!name.includes('\\') && !name.includes('\0') && !name.includes(':') &&
      !name.startsWith('/') && !parts.some(part => part === '..' || part === '.'), 'Unsafe ZIP filename.');
    check(!names.has(name.toLowerCase()), 'ZIP contains duplicate filenames.');
    names.add(name.toLowerCase());
    // Folder entries and macOS archive metadata are not save candidates.
    if (!name.endsWith('/') && parts[0] !== '__MACOSX' && parts.at(-1) !== '.DS_Store') {
      check(uncompressed > 0 && uncompressed <= limit && compressed <= limit,
        'ZIP entry exceeds the conversion size limit.');
      check(method === 0 || method === 8, 'ZIP compression is unsupported. Extract it first and select the console save.');
      candidates.push({ name, nameBytes, local, flags, method, checksum, compressed, uncompressed });
    }
    cursor = next;
  }
  check(cursor === end, 'ZIP directory length is inconsistent.');
  check(candidates.length === 1, 'ZIP must contain exactly one console save. Extract it and select one save at a time.');
  const entry = candidates[0];
  const local = entry.local;
  check(local + 30 <= directoryStart && data.getUint32(local, true) === 0x04034b50, 'Invalid ZIP local header.');
  const nameLength = data.getUint16(local + 26, true);
  const start = local + 30 + nameLength + data.getUint16(local + 28, true);
  check(data.getUint16(local + 6, true) === entry.flags && data.getUint16(local + 8, true) === entry.method &&
    nameLength === entry.nameBytes.length && start + entry.compressed <= directoryStart,
    'ZIP local header disagrees with its directory.');
  check(entry.nameBytes.every((byte, i) => bytes[local + 30 + i] === byte), 'ZIP filenames disagree.');
  if (!(entry.flags & 8)) {
    check(data.getUint32(local + 14, true) === entry.checksum &&
      data.getUint32(local + 18, true) === entry.compressed && data.getUint32(local + 22, true) === entry.uncompressed,
      'ZIP local sizes or checksum disagree.');
  }
  const packed = bytes.subarray(start, start + entry.compressed);
  const output = entry.method === 0 ? packed.slice() : await inflateBounded(packed, entry.uncompressed, limit);
  check(output.length === entry.uncompressed && crc32(output) === entry.checksum, 'ZIP size or CRC-32 check failed.');
  return { name: entry.name.split('/').at(-1), data: output };
}

export function createZip(files) {
  const parts = [];
  const directory = [];
  let offset = 0;
  for (const [name, bytes] of Object.entries(files)) {
    const filename = encoder.encode(name);
    const crc = crc32(bytes);
    const header = new Uint8Array(30 + filename.length);
    const h = view(header);
    h.setUint32(0, 0x04034b50, true);
    h.setUint16(4, 20, true);
    h.setUint16(6, 2048, true);
    h.setUint16(12, 33, true); // 1980-01-01; deterministic output, no local timestamps.
    h.setUint32(14, crc, true);
    h.setUint32(18, bytes.length, true);
    h.setUint32(22, bytes.length, true);
    h.setUint16(26, filename.length, true);
    header.set(filename, 30);
    parts.push(header, bytes);
    const central = new Uint8Array(46 + filename.length);
    const c = view(central);
    c.setUint32(0, 0x02014b50, true);
    c.setUint16(4, 20, true);
    c.setUint16(6, 20, true);
    c.setUint16(8, 2048, true);
    c.setUint16(14, 33, true);
    c.setUint32(16, crc, true);
    c.setUint32(20, bytes.length, true);
    c.setUint32(24, bytes.length, true);
    c.setUint16(28, filename.length, true);
    c.setUint32(42, offset, true);
    central.set(filename, 46);
    directory.push(central);
    offset += header.length + bytes.length;
  }
  const directoryLength = directory.reduce((total, item) => total + item.length, 0);
  const end = new Uint8Array(22);
  const e = view(end);
  e.setUint32(0, 0x06054b50, true);
  e.setUint16(8, directory.length, true);
  e.setUint16(10, directory.length, true);
  e.setUint32(12, directoryLength, true);
  e.setUint32(16, offset, true);
  const output = new Uint8Array(offset + directoryLength + end.length);
  let position = 0;
  for (const part of [...parts, ...directory, end]) {
    output.set(part, position);
    position += part.length;
  }
  return output;
}
