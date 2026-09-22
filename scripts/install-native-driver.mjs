// Install already validated native payloads. The old installation is retained
// outside Arduino's package directory; no shell or interpreter is launched.
import {cpSync,existsSync,mkdirSync,readFileSync,readdirSync,renameSync,writeFileSync} from 'node:fs';
import {resolve,dirname,join,relative,isAbsolute,sep} from 'node:path';
import {randomUUID} from 'node:crypto';
const [platformInput,toolchainInput,dataInput]=process.argv.slice(2);
if(!platformInput||!toolchainInput)throw new Error('Usage: node scripts/install-native-driver.mjs <native-platform-directory> <native-toolchain-directory> [Arduino15-directory]');
const platform=resolve(platformInput),toolchain=resolve(toolchainInput);
const data=resolve(dataInput||join(process.env.LOCALAPPDATA,'Arduino15'));
const version=readFileSync(join(platform,'platform.txt'),'utf8').match(/^version=([\d.]+)$/m)?.[1];
if(!version||!existsSync(join(platform,'tools/stcxx-driver/stcxx.exe')))throw new Error('Missing Windows native platform');
const metadata=JSON.parse(readFileSync(join(toolchain,'toolchain.json'),'utf8'));
if(metadata.execution!=='native'||!/^\d+\.\d+\.\d+$/.test(metadata.version))throw new Error('Toolchain is not a versioned native payload');
if(!existsSync(join(toolchain,'bin/stcxx.exe')))throw new Error('Missing unified Windows STCXX driver');
function checkPayload(root){
  for(const entry of readdirSync(root,{withFileTypes:true})){
    const path=join(root,entry.name);
    if(entry.isSymbolicLink())throw new Error(`Unexpected link: ${path}`);
    if(/python|\.(py|pyc|pyd|ps1|sh)$/i.test(entry.name))throw new Error(`Interpreter payload: ${path}`);
    if(entry.isDirectory())checkPayload(path);
  }
}
checkPayload(platform);checkPayload(toolchain);
const id=randomUUID();
const backup=resolve(dirname(data),'Arduino15-native-backups',id);
const destinations=[join(data,'packages/stc/hardware/mcs251',version),join(data,'packages/stc/tools/stcxx-toolchain',metadata.version)];
const sources=[platform,toolchain];
// Every directory rename is constrained to the explicitly selected Arduino
// data directory or this unique sibling backup directory.
function within(root,path){const rel=relative(root,resolve(path));if(!rel||rel.startsWith('..'+sep)||rel==='..'||isAbsolute(rel))throw new Error(`Unsafe installation path: ${path}`);}
const operations=destinations.map((destination,i)=>{
  within(data,destination);
  if(i===0&&!existsSync(destination))throw new Error(`Install the published platform first: ${destination}`);
  const saved=join(backup,i===0?'platform':'toolchain');within(backup,saved);
  return {source:sources[i],destination,staged:destination+'.native-'+id,saved,existed:existsSync(destination)};
});
mkdirSync(backup,{recursive:true});
for(const op of operations){within(data,op.staged);cpSync(op.source,op.staged,{recursive:true,errorOnExist:true,force:false});}
// Keep Boards Manager metadata and user overrides, but pin this local build to
// the installed unified toolchain even when the published index names an older one.
const platformOp=operations[0];
const installed=join(platformOp.destination,'installed.json');
if(existsSync(installed))cpSync(installed,join(platformOp.staged,'installed.json'));
const local=join(platformOp.destination,'platform.local.txt');
const previous=existsSync(local)?readFileSync(local,'utf8').replace(/\n?# BEGIN STCXX LOCAL TOOLCHAIN[\s\S]*?# END STCXX LOCAL TOOLCHAIN\r?\n?/g,''):'';
const toolPath=destinations[1].replaceAll('\\','/');
writeFileSync(join(platformOp.staged,'platform.local.txt'),previous.trimEnd()+'\n'+[
  '# BEGIN STCXX LOCAL TOOLCHAIN',
  `compiler.stcxx.path=${toolPath}`,
  'compiler.backend.path={compiler.stcxx.path}/sdcc/bin',
  'compiler.systemincludes="-I{compiler.stcxx.path}/sdcc/include" "-I{compiler.stcxx.path}/sdcc/include/mcs51"',
  '# END STCXX LOCAL TOOLCHAIN',''
].join('\n'));
const done=[];
try{
  for(const op of operations){
    if(op.existed)renameSync(op.destination,op.saved);
    try{renameSync(op.staged,op.destination);}catch(error){if(op.existed)renameSync(op.saved,op.destination);throw error;}
    done.push(op);
  }
}catch(error){
  for(const op of done.reverse()){renameSync(op.destination,op.staged);if(op.existed)renameSync(op.saved,op.destination);}
  throw error;
}
writeFileSync(join(backup,'installation.json'),JSON.stringify({date:new Date().toISOString(),operations},null,2)+'\n');
console.log(JSON.stringify({platform:destinations[0],toolchain:destinations[1],backup},null,2));
