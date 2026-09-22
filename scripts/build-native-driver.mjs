import {spawnSync} from 'node:child_process';
import {copyFileSync,readdirSync,mkdirSync,writeFileSync,rmSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import './sync-stcxx-sdk.mjs';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..','tools','stcxx-driver');
const result=spawnSync('cargo',['build','--release','--locked'],{cwd:root,stdio:'inherit',windowsHide:true});
if(result.status!==0)process.exit(result.status??1);
const name=process.platform==='win32'?'stcxx.exe':'stcxx';
copyFileSync(resolve(root,'target/release',name),resolve(root,name));
// Ship the licenses of every dependency reachable on this host.
const host=spawnSync('rustc',['-vV'],{encoding:'utf8',windowsHide:true}).stdout.match(/^host: (.+)$/m)?.[1];
if(!host)throw new Error('Cannot determine Rust build host');
const metadata=spawnSync('cargo',['metadata','--format-version','1','--locked','--offline','--filter-platform',host],{cwd:root,encoding:'utf8',maxBuffer:16*1024*1024,windowsHide:true});
if(metadata.status!==0)throw new Error(metadata.stderr);
const graph=JSON.parse(metadata.stdout);
const included=new Set(graph.resolve.nodes.map(n=>n.id));
const notices=[];
for(const pkg of graph.packages.filter(p=>included.has(p.id)&&p.name!=='stcxx').sort((a,b)=>a.id.localeCompare(b.id))){
  const source=dirname(pkg.manifest_path);
  const files=readdirSync(source,{withFileTypes:true}).filter(f=>f.isFile()&&/^(LICENSE|COPYING|NOTICE)([-.]|$)/i.test(f.name));
  if(!files.length)throw new Error(`Missing dependency license: ${pkg.id}`);
  const destination=resolve(root,'LICENSES',`${pkg.name}-${pkg.version}`);
  mkdirSync(destination,{recursive:true});
  for(const file of files)copyFileSync(resolve(source,file.name),resolve(destination,file.name));
  notices.push({name:pkg.name,version:pkg.version,license:pkg.license,repository:pkg.repository,files:files.map(f=>f.name)});
}
writeFileSync(resolve(root,'LICENSES',`dependencies-${host}.json`),JSON.stringify(notices,null,2)+'\n');
rmSync(resolve(root,'LICENSES','dependencies.json'),{force:true});
console.log(resolve(root,name));
