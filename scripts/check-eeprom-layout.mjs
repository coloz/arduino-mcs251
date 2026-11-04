#!/usr/bin/env node
// Exercise the generated board recipes with native SDCC, independently of the
// Arduino/Rust frontends. Build products remain under .tmp/eeprom-layout.
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {dirname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const HOME = 0xff0000;
const ADDRESS_END = 0x1000000;
const hex = value => `0x${value.toString(16)}`;
const argv = process.argv.slice(2);
if (argv.includes('--help')) {
  console.log('Usage: node scripts/check-eeprom-layout.mjs [--sdcc <native SDCC path>]');
  process.exit(0);
}
assert(argv.length === 0 || (argv.length === 2 && argv[0] === '--sdcc'), 'Unknown arguments; use --help');
const sdcc = argv.length ? resolve(argv[1]) : (process.platform === 'win32' ? 'sdcc.exe' : 'sdcc');
const work = resolve(root, '.tmp', 'eeprom-layout');
mkdirSync(work, {recursive: true});

// Arduino properties use quoted arguments such as "-Wl-b GSINIT0=0xfe0800".
// Pass those directly to spawnSync without invoking a shell.
function flags(value = '') {
  const result = [];
  for (const match of value.matchAll(/"([^"]*)"|'([^']*)'|([^\s"']+)/g)) {
    result.push(match[1] ?? match[2] ?? match[3]);
  }
  assert(!value.includes('{'), `Unresolved Arduino property: ${value}`);
  return result;
}

const properties = new Map();
for (const raw of readFileSync(resolve(root, 'boards.txt'), 'utf8').split(/\r?\n/)) {
  const line = raw.trim();
  if (!line || line.startsWith('#')) continue;
  const separator = line.indexOf('=');
  assert(separator > 0, `Invalid boards.txt line: ${raw}`);
  properties.set(line.slice(0, separator), line.slice(separator + 1));
}

function boardProperties(id, selection) {
  const result = {};
  const prefix = `${id}.`;
  for (const [key, value] of properties) {
    if (key.startsWith(prefix) && !key.slice(prefix.length).startsWith('menu.')) {
      result[key.slice(prefix.length)] = value;
    }
  }
  if (selection !== undefined) {
    const menuPrefix = `${id}.menu.eeprom.${selection}.`;
    assert(properties.has(`${id}.menu.eeprom.${selection}`), `${id}: missing EEPROM menu ${selection}`);
    for (const [key, value] of properties) {
      if (key.startsWith(menuPrefix)) result[key.slice(menuPrefix.length)] = value;
    }
  }
  return result;
}

function readHex(path) {
  const image = new Map();
  let base = 0;
  let eof = false;
  let linear = false;
  for (const [index, raw] of readFileSync(path, 'utf8').split(/\r?\n/).entries()) {
    const line = raw.trim();
    if (!line) continue;
    assert(!eof && /^:(?:[0-9a-f]{2})+$/i.test(line), `${path}:${index + 1}: malformed HEX`);
    const record = Buffer.from(line.slice(1), 'hex');
    assert(record.length >= 5 && record.length === record[0] + 5, 'Invalid HEX record length');
    assert.equal(record.reduce((sum, byte) => sum + byte, 0) & 255, 0, 'Invalid HEX checksum');
    const offset = record.readUInt16BE(1);
    const data = record.subarray(4, -1);
    switch (record[3]) {
      case 0:
        for (const [i, byte] of data.entries()) {
          const address = base + offset + i;
          assert(!image.has(address) || image.get(address) === byte, `Conflicting HEX at ${hex(address)}`);
          image.set(address, byte);
        }
        break;
      case 1:
        assert.equal(offset, 0);
        assert.equal(data.length, 0);
        eof = true;
        break;
      case 4:
        assert.equal(offset, 0);
        assert.equal(data.length, 2);
        base = data.readUInt16BE(0) * 0x10000;
        linear = true;
        break;
      default:
        assert.fail(`Unexpected HEX record type ${record[3]}`);
    }
  }
  assert(eof && linear && image.size, 'Expected nonempty type-04 MCS251 HEX');
  return image;
}

function run(args, cwd, expectFailure = false) {
  const result = spawnSync(sdcc, args, {cwd, encoding: 'utf8', windowsHide: true, maxBuffer: 4 * 1024 * 1024});
  if (result.error) throw result.error;
  const output = `${result.stdout ?? ''}${result.stderr ?? ''}`;
  if (expectFailure) {
    assert.notEqual(result.status, 0, `SDCC accepted an absolute object inside EEPROM:\n${args.join(' ')}`);
    assert(/code.window|outside.*window|out.of.*window/i.test(output), `Failure was unrelated to the code window:\n${output}`);
  } else {
    assert.equal(result.status, 0, `SDCC failed (${result.status}):\n${args.join(' ')}\n${output}`);
  }
  return output;
}

const fixture = `
#ifndef STC_EEPROM_SIZE
#error Missing board EEPROM definitions
#endif
#if STC_EEPROM_SIZE != EXPECT_EEPROM_SIZE || STC_EEPROM_FLASH_START != EXPECT_EEPROM_START || STC_EEPROM_SECTOR_SIZE != 512UL
#error Incorrect board EEPROM definitions
#endif
volatile __xdata unsigned char layout_initialized[4] = { 0x42, 0x19, 0x73, 0xa5 };
volatile __xdata unsigned char layout_sink;
const volatile __code unsigned char layout_constants[4] = { 0x31, 0x57, 0x9b, 0xdf };
const volatile __code __at (CHECK_PROGRAM_LAST) unsigned char layout_last_byte = 0x5a;
#ifdef CHECK_FORBIDDEN_ADDRESS
const volatile __code __at (CHECK_FORBIDDEN_ADDRESS) unsigned char layout_forbidden_byte = 0x6b;
#endif
void layout_timer_isr(void) __interrupt (1) { ++layout_sink; }
void main(void) {
  for (;;) layout_sink = layout_initialized[layout_sink & 3] ^ layout_constants[layout_sink & 3] ^ layout_last_byte;
}
`;

function symbolAddress(map, name) {
  const match = map.match(new RegExp(`^\\s*C:\\s+([0-9a-fA-F]{8})\\s+${name}(?:\\s|$)`, 'm'));
  assert(match, `Missing code symbol ${name}`);
  return Number.parseInt(match[1], 16);
}

function validateImage(profile, path) {
  const image = readHex(path);
  const map = readFileSync(path.replace(/\.ihx$/, '.map'), 'utf8');
  for (const address of image.keys()) {
    assert(address >= profile.programStart && address < profile.programEnd,
      `${profile.label}: HEX byte ${hex(address)} lies outside the program area`);
    if (profile.eepromSize) assert(address < profile.eepromStart || address >= profile.eepromStart + profile.eepromSize,
      `${profile.label}: HEX occupies EEPROM at ${hex(address)}`);
  }
  assert.equal(image.get(profile.programEnd - 1), 0x5a, `${profile.label}: missing last-byte sentinel`);
  assert.equal(image.get(HOME), 0x02, `${profile.label}: HOME/reset moved from FF:0000`);
  assert.equal(image.get(HOME + 0x0b), 0x8a, `${profile.label}: timer ISR vector moved`);
  const isrTarget = image.get(HOME + 0x0c) * 0x10000 + image.get(HOME + 0x0d) * 0x100 + image.get(HOME + 0x0e);
  assert.equal(isrTarget, symbolAddress(map, '_layout_timer_isr'), 'Timer vector does not target its ISR');
  const startup = symbolAddress(map, '__sdcc_gsinit_startup');
  assert(startup >= profile.programStart && startup < profile.programEnd, 'Startup outside program area');
  if (profile.allocation === 'prefix') assert.equal(startup, profile.programStart, 'GSINIT0 must follow reserved EEPROM');
  const trampoline = symbolAddress(map, '__sdcc_mcs251_reset_trampoline');
  assert.equal(HOME + image.get(HOME + 1) * 0x100 + image.get(HOME + 2), trampoline,
    'Reset vector does not target the HOME trampoline');
  assert.equal(image.get(trampoline), 0x8a, 'Reset trampoline is not an EJMP');
  assert.equal(image.get(trampoline + 1) * 0x10000 + image.get(trampoline + 2) * 0x100 + image.get(trampoline + 3), startup,
    'Reset trampoline does not target relocated GSINIT0');
  for (const symbol of ['_layout_timer_isr', '_layout_constants', '_main']) {
    const address = symbolAddress(map, symbol);
    assert(image.has(address), `${profile.label}: missing emitted ${symbol}`);
  }
  assert(/^XINIT\s+[0-9a-fA-F]{8}\s+0*[1-9a-fA-F][0-9a-fA-F]*\s+=/m.test(map), 'Fixture has no XINIT data');
  const window = map.match(/^Code Window:\s+0x([0-9a-f]+):0x([0-9a-f]+)\s*$/im);
  if (profile.linkFlags.some(flag => flag.includes('--code-window='))) {
    assert(window, 'Missing code-window ledger');
    assert.equal(Number.parseInt(window[1], 16), profile.programStart);
    assert.equal(Number.parseInt(window[2], 16), profile.programEnd);
  }
}

const devices = JSON.parse(readFileSync(resolve(root, 'tools', 'variants', 'devices.json'), 'utf8')).devices;
let checked = 0;
let rejected = 0;
for (const device of devices) {
  const layout = device.eeprom_layout;
  assert(layout && ['prefix', 'suffix', 'separate'].includes(layout.allocation), `${device.id}: missing EEPROM layout`);
  const physicalStart = device.linker.flash_loc ?? ADDRESS_END - device.flash_bytes;
  const physicalEnd = physicalStart + device.flash_bytes;
  const options = layout.allocation === 'separate' ? [undefined] : ['none', 'k1', 'k2', 'k4', 'k8', 'k16', 'k32'];
  const reserves = layout.allocation === 'separate' ? [layout.fixed_bytes] : layout.reserve_options_bytes;
  assert.equal(options.length, reserves.length, `${device.id}: unexpected EEPROM option count`);
  for (const [index, option] of options.entries()) {
    const props = boardProperties(device.id, option);
    const eepromSize = reserves[index];
    const programStart = physicalStart + (layout.allocation === 'prefix' ? eepromSize : 0);
    const programEnd = physicalEnd - (layout.allocation === 'suffix' ? eepromSize : 0);
    const eepromStart = layout.allocation === 'suffix' ? programEnd : (layout.flash_start ?? physicalStart);
    const label = `${device.id}${option === undefined ? '' : `:eeprom=${option}`}`;
    assert.equal(Number(props['upload.maximum_size']), programEnd - programStart, `${label}: wrong upload budget`);
    const linkFlags = flags(props['build.link_flags']);
    const profile = {allocation: layout.allocation, programStart, programEnd, eepromStart, eepromSize, label, linkFlags};
    const directory = resolve(work, device.id, option ?? 'fixed');
    mkdirSync(directory, {recursive: true});
    writeFileSync(resolve(directory, 'layout.c'), fixture);
    const args = [
      '-mmcs251', `--model-${props['build.memory_model']}`, '--stack-auto', '--std-sdcc11', '--opt-code-size',
      '--code-size', props['upload.maximum_size'], '--xram-size', '8192',
      ...linkFlags, ...flags(props['build.flash_flags']), ...flags(props['build.eeprom_flags']),
      `-DEXPECT_EEPROM_SIZE=${eepromSize}UL`, `-DEXPECT_EEPROM_START=${hex(eepromStart)}UL`,
      `-DCHECK_PROGRAM_LAST=${hex(programEnd - 1)}UL`,
    ];
    run([...args, 'layout.c', '-o', 'layout.ihx'], directory);
    validateImage(profile, resolve(directory, 'layout.ihx'));
    ++checked;
    if (option === 'k2') {
      run([...args, `-DCHECK_FORBIDDEN_ADDRESS=${hex(eepromStart)}UL`, 'layout.c', '-o', 'forbidden.ihx'], directory, true);
      ++rejected;
    }
    console.log(`PASS ${label}: program ${hex(programStart)}..${hex(programEnd)}; EEPROM ${hex(eepromStart)} + ${eepromSize}`);
  }
}
assert.equal(checked, 40, 'Expected all ten devices and every shared EEPROM menu option');
assert.equal(rejected, 5, 'Expected one absolute EEPROM-overlap rejection per shared-Flash device');
console.log(`EEPROM layout checks passed: ${checked} native SDCC images, ${rejected} rejected overlaps.`);
