// C++ API dispatch tests; --clang and --ld use the same tools as run.mjs.
import {readFileSync, mkdirSync} from 'node:fs';
import {resolve, dirname} from 'node:path';
import {spawnSync} from 'node:child_process';
const args = process.argv.slice(2);
const option = (name, fallback) => args.includes(name) ? args[args.indexOf(name) + 1] : fallback;
const clang = option('--clang', 'clang'), ld = option('--ld', 'wasm-ld');
const sdk = option('--sdk', resolve(dirname(clang), '../../share/stcxx/sdk/runtime/cpp'));
const root = resolve(import.meta.dirname, '../..');
const out = resolve(root, '.tmp/peripheral-host-tests'); mkdirSync(out, {recursive: true});
const boards = readFileSync(resolve(root, 'boards.txt'), 'utf8');
function run(command, argv) {
  const result = spawnSync(command, argv, {cwd: root, encoding: 'utf8'});
  if (result.error || result.status !== 0) throw new Error(`${command}: ${result.error ?? result.stderr ?? result.stdout}`);
}
for (const id of ['stc32g144k246', 'stc32cl8k64', 'ai8051u_34k16']) {
  const prefix = id + '.build.core_flags=';
  const flags = boards.split('\n').find(line => line.startsWith(prefix)).slice(prefix.length).trim().split(/\s+/);
  const obj = resolve(out, `api-${id}.o`), wasm = resolve(out, `api-${id}.wasm`);
  run(clang, ['--target=wasm32', '-ffreestanding', '-std=c++11', '-O2', '-fno-exceptions', '-fno-rtti',
    '-Wall', '-Werror', '-D__STC_MCS251__', '-D__STC_CLANG_IR_ONLY__', ...flags, '-Icores/STC', '-idirafter', sdk,
    '-c', 'tests/peripherals/api.cpp', '-o', obj]);
  run(ld, ['--no-entry', '--export=run_tests', obj, '-o', wasm]);
  const {instance} = await WebAssembly.instantiate(readFileSync(wasm));
  const line = instance.exports.run_tests(); if (line) throw new Error(`${id}: api.cpp:${line}`);
  console.log(`PASS ${id}: UART constructor validation, Wire selection and inert invalid buses`);
  const variant = boards.split('\n').find(line => line.startsWith(id + '.build.variant=')).split('=')[1].trim();
  const nativeObj = resolve(out, `uart1-${id}.o`), nativeWasm = resolve(out, `uart1-${id}.wasm`);
  run(clang, ['--target=wasm32', '-ffreestanding', '-O2', '-Wall', '-Werror',
    '-D__STC_MCS251__', '-DF_CPU=12000000UL', ...flags, '-Itests/peripherals', '-Icores/STC',
    '-Icores/STC/hal', '-Ivariants/' + variant, '-c', 'tests/peripherals/serial1.c', '-o', nativeObj]);
  run(ld, ['--no-entry', '--export=run_tests', nativeObj, '-o', nativeWasm]);
  const native = await WebAssembly.instantiate(readFileSync(nativeWasm));
  const nativeLine = native.instance.exports.run_tests(); if (nativeLine) throw new Error(`${id}: serial1.c:${nativeLine}`);
  console.log(`PASS ${id}: UART1 baud prescaler reset/restore, restart and interrupt/window preservation`);
}
