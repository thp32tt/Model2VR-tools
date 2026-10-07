import { Client, StreamableHTTPClientTransport } from '@modelcontextprotocol/client';
import { setTimeout as sleep } from 'node:timers/promises';

const c=new Client({name:'n100-mcp-ci',version:'0.3.0'});
await c.connect(new StreamableHTTPClientTransport(new URL(process.env.N100_MCP_URL || 'http://127.0.0.1:8877/mcp')));
async function call(name,args={}) {
  const r=await c.callTool({name,arguments:args});
  if(r.isError) throw new Error(name+': '+JSON.stringify(r.content));
  return r.content;
}
function textObj(content) {
  const t=content.find(x=>x.type==='text')?.text || '{}';
  return JSON.parse(t);
}
const names=(await c.listTools()).tools.map(x=>x.name);
for(const n of ['read_file_chunk','write_file_chunk','finalize_file','find_files','grep_text','archive_create',
'start_python_job','git_clone','git_worktree_add','inspect_dds','image_info','github_download','system_status'])
  if(!names.includes(n)) throw new Error('missing '+n);

await call('make_dir',{path:'.ci-n100',recursive:true});
await call('prepare_large_file',{path:'.ci-n100/chunk.bin',size:10,overwrite:true});
await call('write_file_chunk',{path:'.ci-n100/chunk.bin',offset:5,base64:Buffer.from('56789').toString('base64')});
await call('write_file_chunk',{path:'.ci-n100/chunk.bin',offset:0,base64:Buffer.from('01234').toString('base64')});
let j=textObj(await call('read_file_chunk',{path:'.ci-n100/chunk.bin',offset:0,length:10}));
if(Buffer.from(j.base64,'base64').toString()!=='0123456789') throw new Error('chunk offset failure');
j=textObj(await call('finalize_file',{path:'.ci-n100/chunk.bin',expectedSize:10}));
if(!j.verified) throw new Error('finalize failed');

await call('write_text',{path:'.ci-n100/search/a.txt',content:'alpha REWORK_REQUIRED omega\n'});
j=textObj(await call('grep_text',{path:'.ci-n100',query:'REWORK_REQUIRED',includeGlob:'**/*.txt'}));
if(j.matches.length!==1) throw new Error('grep failed');
await call('archive_create',{source:'.ci-n100/search',archive:'.ci-n100/search.zip',format:'zip'});
await call('archive_extract',{archive:'.ci-n100/search.zip',destination:'.ci-n100/extracted'});

const hdr=Buffer.alloc(128); hdr.write('DDS ',0,'ascii'); hdr.writeUInt32LE(124,4); hdr.writeUInt32LE(64,12); hdr.writeUInt32LE(128,16);
hdr.writeUInt32LE(512,20); hdr.writeUInt32LE(1,28); hdr.writeUInt32LE(32,76); hdr.writeUInt32LE(0x40,80); hdr.writeUInt32LE(32,88);
hdr.writeUInt32LE(0xff,92); hdr.writeUInt32LE(0xff00,96); hdr.writeUInt32LE(0xff0000,100); hdr.writeUInt32LE(0xff000000,104);
await call('write_binary_base64',{path:'.ci-n100/test.dds',base64:hdr.toString('base64'),overwrite:true});
j=textObj(await call('inspect_dds',{path:'.ci-n100/test.dds'}));
if(j.width!==128 || j.height!==64) throw new Error('DDS inspect failed');

await call('write_text',{path:'.ci-n100/make_images.py',content:'from PIL import Image\nImage.new("RGB",(32,32),(10,20,30)).save("a.png")\nImage.new("RGB",(32,32),(11,20,30)).save("b.png")\n'});
await call('run_python_file',{script:'.ci-n100/make_images.py',cwd:'.ci-n100'});
await call('image_info',{path:'.ci-n100/a.png'});
await call('image_diff',{a:'.ci-n100/a.png',b:'.ci-n100/b.png',output:'.ci-n100/diff.png'});
await call('image_contact_sheet',{images:['.ci-n100/a.png','.ci-n100/b.png'],output:'.ci-n100/contact.jpg',columns:2,width:128});

await call('write_text',{path:'.ci-n100/job.py',content:'import time\nprint("START",flush=True)\ntime.sleep(.2)\nprint("DONE",flush=True)\n'});
j=textObj(await call('start_python_job',{script:'.ci-n100/job.py',cwd:'.ci-n100'}));
await sleep(600);
const js=textObj(await call('job_status',{jobId:j.id}));
if(js.status!=='completed') throw new Error('async job failed '+JSON.stringify(js));

await call('remove_path',{path:'.ci-n100/clone',recursive:true,confirm:true}).catch(()=>{});
await call('remove_path',{path:'.ci-n100/wt',recursive:true,confirm:true}).catch(()=>{});
await call('git_clone',{url:'https://github.com/thp32tt/Model2VR-tools.git',destination:'.ci-n100/clone',branch:'main',depth:1});
await call('git_worktree_add',{repo:'.ci-n100/clone',destination:'.ci-n100/wt',ref:'main',newBranch:'ci-worktree'});
await call('git_worktree_remove',{repo:'.ci-n100/clone',destination:'.ci-n100/wt',force:true,confirm:true});

await call('github_download',{url:'https://raw.githubusercontent.com/thp32tt/Model2VR-tools/main/README.md',path:'.ci-n100/remote-readme.md',maxBytes:1048576});
await call('system_status',{});
console.log('N100_MCP_V03_CI_PASS tools='+names.length);
await c.close();
