// Run the production C drivers in WebAssembly with simulated peripheral registers.
// Usage: node tests/peripherals/run.mjs --clang /path/to/clang --ld /path/to/wasm-ld
import { readFileSync, mkdirSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
const args = process.argv.slice(2);
const option = (name, fallback) => args.includes(name) ? args[args.indexOf(name)+1] : fallback;
const clang = option('--clang', 'clang'), ld = option('--ld', 'wasm-ld');
const root = resolve(import.meta.dirname, '../..');
const out = resolve(root, '.tmp/peripheral-host-tests'); mkdirSync(out, {recursive:true});
const boards = readFileSync(resolve(root,'boards.txt'),'utf8');
const devices = JSON.parse(readFileSync(resolve(root,'tools/variants/devices.json'),'utf8')).devices;
function run(command, argv) {
  const result = spawnSync(command, argv, {cwd:root, encoding:'utf8'});
  if (result.error || result.status !== 0) throw new Error(`${command}: ${result.error ?? result.stderr ?? result.stdout}`);
}
const id='stc32g144k246';
const flags=boards.split('\n').find(l=>l.startsWith(id+'.build.core_flags=')).slice((id+'.build.core_flags=').length).trim().split(/\s+/);
const objects=[];
for(const [i,source] of ['tests/peripherals/wire.c','libraries/Wire/src/Wire.c','libraries/Wire/src/Wire1.c'].entries()) {
  const obj=resolve(out,'wire'+i+'.o'); objects.push(obj);
  run(clang,['--target=wasm32','-ffreestanding','-O2','-Wall','-Werror','-DSTC_WIRE_HOST_HARDWARE_HOOKS',
    '-DF_CPU=12000000UL',...flags,'-Itests/peripherals','-Icores/STC','-Ivariants/STC32G144K246','-c',source,'-o',obj]);
}
const wasm=resolve(out,'wire.wasm');run(ld,['--no-entry','--export=run_tests',...objects,'-o',wasm]);
const {instance}=await WebAssembly.instantiate(readFileSync(wasm));
const line=instance.exports.run_tests();if(line)throw new Error(`wire.c:${line}`);
console.log('PASS dual Wire: independent mux/registers/buffers, repeated START, NACK, timeout, slave callbacks and shutdown');
for (const id of ['stc32g12k64','stc32cl8k64','ai8051u_34k64','stc32g144k246']) {
  const device = devices.find(d=>d.id===id);
  const flags = boards.split('\n').find(l=>l.startsWith(id+'.build.core_flags=')).split('=');
  const coreFlags = flags.slice(1).join('=').trim().split(/\s+/);
  const obj = resolve(out,id+'.o'), wasm = resolve(out,id+'.wasm');
  run(clang,['--target=wasm32','-ffreestanding','-O2','-Wall','-Werror','-Wno-unused-variable',
    '-DF_CPU=12000000UL', ...coreFlags, '-Itests/peripherals','-Icores/STC','-Icores/STC/hal',
    '-Ivariants/'+device.model.replaceAll('-','_'), '-c','tests/peripherals/serial.c','-o',obj]);
  run(ld,['--no-entry','--export=run_tests',obj,'-o',wasm]);
  const {instance} = await WebAssembly.instantiate(readFileSync(wasm));
  const line = instance.exports.run_tests();
  if (line) throw new Error(`${id}: serial.c:${line}`);
console.log(`PASS ${id}: UART routing, timers, RX isolation/overflow, TX polling, shutdown`);
}

for (const id of ['stc32g12k64','stc32cl8k64','ai8051u_34k64','stc32g144k246']) {
  const device=devices.find(d=>d.id===id);
  const flags=boards.split('\n').find(l=>l.startsWith(id+'.build.core_flags=')).slice((id+'.build.core_flags=').length).trim().split(/\s+/);
  const common=['--target=wasm32','-ffreestanding','-O2','-Wall','-Werror','-DF_CPU=12000000UL',...flags,
    '-Itests/peripherals','-Icores/STC','-Ilibraries/SPI/src','-Ivariants/'+device.model.replaceAll('-','_')];
  const sources=['tests/peripherals/spi.c',...readdirSync(resolve(root,'libraries/SPI/src')).filter(f=>f.endsWith('.c')).map(f=>'libraries/SPI/src/'+f)];
  const objects=[];
  for (const [index,source] of sources.entries()) {
    const obj=resolve(out,`spi-${id}-${index}.o`); objects.push(obj);
    run(clang,[...common,'-DSTC_SPI_HOST_HARDWARE_HOOKS','-DSTC_SPI_HOST_INTERRUPT_HOOKS','-DLSBFIRST=0','-DMSBFIRST=1','-c',source,'-o',obj]);
  }
  const wasm=resolve(out,`spi-${id}.wasm`);
  run(ld,['--no-entry','--export=run_tests',...objects,'-o',wasm]);
  const {instance}=await WebAssembly.instantiate(readFileSync(wasm));
  const line=instance.exports.run_tests(); if(line)throw new Error(`${id}: spi.c:${line}`);
  console.log(`PASS ${id}: SPI hardware routes, modes, independent state, interrupt guards, fallback and errors`);
  const cppObjects=[];
  for (const [index,source] of ['tests/peripherals/spi_class.cpp','libraries/SPI/src/SPIClass.cpp'].entries()) {
    const obj=resolve(out,`spi-class-${id}-${index}.o`);cppObjects.push(obj);
    run(clang,[...common,'-std=c++11','-fno-exceptions','-fno-rtti','-DSTCXX_HOST_TEST=1',
      '-idirafter','cores/STC/runtime/include','-c',source,'-o',obj]);
  }
  const cppWasm=resolve(out,`spi-class-${id}.wasm`);
  run(ld,['--no-entry','--export=run_tests','--export=__wasm_call_ctors',...cppObjects,'-o',cppWasm]);
  const cpp=await WebAssembly.instantiate(readFileSync(cppWasm));cpp.instance.exports.__wasm_call_ctors();
  const cppLine=cpp.instance.exports.run_tests();if(cppLine)throw new Error(`${id}: spi_class.cpp:${cppLine}`);
  console.log(`PASS ${id}: SPI C++ bus dispatch, per-bus settings, transfer16 and invalid objects`);
}
