import {readFileSync, writeFileSync, mkdirSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const sdk = resolve(root, '../stcxx/sdk');
const check = process.argv.includes('--check');
const manifest = JSON.parse(readFileSync(resolve(sdk, 'runtime-files.json'), 'utf8'));
if (manifest.schema_version !== 2 || manifest.arduino_destination !== 'cores/STC/runtime') {
  throw new Error('Unsupported SDK runtime layout');
}
const copies = manifest.files.map(name => [`runtime/${name}`, `${manifest.arduino_destination}/${name}`]);
for (const host of ['windows-x86_64', 'macos-arm64']) {
  const name = `toolchain-lock.${host}.json`;
  copies.push([`locks/${name}`, `tools/stcxx-driver/${name}`]);
}
let changed = 0;
for (const [source, destination] of copies) {
  const bytes = readFileSync(resolve(sdk, source));
  const output = resolve(root, destination);
  let current;
  try { current = readFileSync(output); } catch {}
  if (current?.equals(bytes)) continue;
  if (check) throw new Error(`SDK copy differs: ${destination}; run node scripts/sync-stcxx-sdk.mjs`);
  mkdirSync(dirname(output), {recursive: true});
  writeFileSync(output, bytes);
  changed++;
}
// Arduino adds peripheral and pin metadata. Its compiler memory settings must
// continue to match the standalone SDK's chip profiles.
const targets = JSON.parse(readFileSync(resolve(sdk, 'targets.json'), 'utf8')).targets;
const devices = JSON.parse(readFileSync(resolve(root, 'tools/variants/devices.json'), 'utf8')).devices;
for (const t of targets) {
  const d = devices.find(d => d.id === t.id);
  if (!d) throw new Error(`Arduino is missing SDK chip ${t.id}`);
  for (const key of Object.keys(t)) {
    if (JSON.stringify(t[key]) !== JSON.stringify(d[key])) {
      throw new Error(`SDK/Arduino chip configuration differs: ${t.id}.${key}`);
    }
  }
}
if (devices.length !== targets.length) throw new Error('SDK/Arduino chip catalog differs');
console.log(`STCXX SDK ${check ? 'verified' : 'synchronized'} (${changed} files updated)`);
