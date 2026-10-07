import { createServer } from 'node:http';
import { promises as fs, createReadStream, openSync, closeSync } from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { createHash, randomUUID } from 'node:crypto';
import { execFile, spawn } from 'node:child_process';
import { promisify } from 'node:util';
import { createMcpHandler, McpServer } from '@modelcontextprotocol/server';
import { localhostHostValidation, localhostOriginValidation, toNodeHandler } from '@modelcontextprotocol/node';
import * as z from 'zod/v4';

const execFileAsync = promisify(execFile);
const PORT = Number(process.env.PORT || 8765);
const ROOT = path.resolve(process.env.N100_MCP_ROOT || os.homedir());
const MAX_TEXT = 2_000_000;
const MAX_BINARY = 16 * 1024 * 1024;
const MAX_IMAGE = 12 * 1024 * 1024;
const MAX_EXEC_MS = 300_000;
const MAX_CHUNK = 8 * 1024 * 1024;
const MAX_SEARCH_FILE = 8 * 1024 * 1024;
const JOB_DIR_REL = '.n100-mcp/jobs';
const MEDIA_HELPER_REL = 'n100-mcp/helpers/media_archive.py';

function insideRoot(input = '.') {
  const resolved = path.resolve(ROOT, input);
  if (resolved !== ROOT && !resolved.startsWith(ROOT + path.sep)) {
    throw new Error('Path escapes allowed root');
  }
  return resolved;
}

function relFromRoot(abs) {
  const rel = path.relative(ROOT, abs);
  return rel || '.';
}

function out(value) {
  return { content: [{ type: 'text', text: typeof value === 'string' ? value : JSON.stringify(value, null, 2) }] };
}

function imageOut(data, mimeType, meta = {}) {
  return {
    content: [
      { type: 'image', data: data.toString('base64'), mimeType },
      { type: 'text', text: JSON.stringify(meta, null, 2) }
    ]
  };
}

function safeToken(value, name) {
  if (!value || value.startsWith('-') || /[\0\r\n]/.test(value)) throw new Error('Unsafe ' + name);
  return value;
}

function safeGitPath(value) {
  if (!value || value.startsWith('-') || path.isAbsolute(value)) throw new Error('Unsafe git path');
  const norm = path.normalize(value);
  if (norm === '..' || norm.startsWith('..' + path.sep)) throw new Error('Git path escapes repository');
  return value;
}

async function runGit(repo, args, timeout = 60_000) {
  const cwd = insideRoot(repo);
  const { stdout, stderr } = await execFileAsync('git', args, {
    cwd,
    timeout: Math.min(Math.max(timeout, 1000), MAX_EXEC_MS),
    maxBuffer: 16 * 1024 * 1024
  });
  return { stdout, stderr, cwd: relFromRoot(cwd), args };
}

async function hashFile(abs, algorithm = 'sha256') {
  return await new Promise((resolve, reject) => {
    const h = createHash(algorithm);
    const s = createReadStream(abs);
    s.on('data', chunk => h.update(chunk));
    s.on('error', reject);
    s.on('end', () => resolve(h.digest('hex')));
  });
}

async function processList(limit = 100) {
  const ids = (await fs.readdir('/proc'))
    .filter(x => /^\d+$/.test(x))
    .map(Number)
    .sort((a,b) => a-b);
  const uid = os.userInfo().uid;
  const rows = [];
  for (const pid of ids) {
    if (rows.length >= limit) break;
    try {
      const status = await fs.readFile('/proc/' + pid + '/status', 'utf8');
      const uidLine = status.split('\n').find(x => x.startsWith('Uid:'));
      const procUid = Number((uidLine || '').trim().split(/\s+/)[1]);
      if (procUid !== uid) continue;
      const cmd = (await fs.readFile('/proc/' + pid + '/cmdline'))
        .toString('utf8').split('\0').filter(Boolean).join(' ');
      const name = (status.split('\n').find(x => x.startsWith('Name:')) || '').slice(5).trim();
      rows.push({ pid, name, cmd });
    } catch {}
  }
  return rows;
}


function globRegex(pattern = '**/*') {
  let x = String(pattern).replace(/\\/g, '/');
  let out = '^';
  for (let i=0;i<x.length;i++) {
    const c=x[i], n=x[i+1];
    if (c==='*' && n==='*') { out += '.*'; i++; }
    else if (c==='*') out += '[^/]*';
    else if (c==='?') out += '[^/]';
    else out += c.replace(/[.*+?^$(){}|[\]\\]/g, '\\$&');
  }
  return new RegExp(out+'$');
}

async function walkFiles(base, limit=10000) {
  const root=insideRoot(base);
  const outFiles=[];
  const stack=[root];
  while(stack.length && outFiles.length<limit) {
    const dir=stack.pop();
    for(const e of await fs.readdir(dir,{withFileTypes:true})) {
      const p=path.join(dir,e.name);
      if(e.isDirectory()) stack.push(p);
      else if(e.isFile()) outFiles.push(p);
      if(outFiles.length>=limit) break;
    }
  }
  return outFiles;
}

async function runFixedPython(args, timeout=120000) {
  const helper=insideRoot(MEDIA_HELPER_REL);
  const {stdout,stderr}=await execFileAsync('python3',[helper,...args],{
    cwd:ROOT,timeout:Math.min(timeout,MAX_EXEC_MS),maxBuffer:16*1024*1024,
    env:{...process.env,PYTHONUNBUFFERED:'1'}
  });
  return {stdout,stderr};
}

async function readJsonFile(abs) {
  return JSON.parse(await fs.readFile(abs,'utf8'));
}
async function writeJsonFile(abs,obj) {
  await fs.mkdir(path.dirname(abs),{recursive:true});
  const tmp=abs+'.tmp-'+process.pid;
  await fs.writeFile(tmp,JSON.stringify(obj,null,2)+'\n','utf8');
  await fs.rename(tmp,abs);
}
function jobMetaPath(id) { return insideRoot(path.join(JOB_DIR_REL,id+'.json')); }
function jobLogPath(id, stream) { return insideRoot(path.join(JOB_DIR_REL,id+'.'+stream+'.log')); }

async function startFileJob(kind, script, args, cwd) {
  const abs=insideRoot(script);
  const ext=path.extname(abs).toLowerCase();
  if(kind==='python' && ext!=='.py') throw new Error('Python job requires .py');
  if(kind==='node' && !['.js','.mjs','.cjs'].includes(ext)) throw new Error('Node job requires .js/.mjs/.cjs');
  const st=await fs.stat(abs); if(!st.isFile()) throw new Error('Script is not a file');
  const work=insideRoot(cwd);
  const id=randomUUID();
  const dir=insideRoot(JOB_DIR_REL); await fs.mkdir(dir,{recursive:true});
  const stdoutPath=jobLogPath(id,'stdout'), stderrPath=jobLogPath(id,'stderr');
  const outFd=openSync(stdoutPath,'a'), errFd=openSync(stderrPath,'a');
  const exe=kind==='python'?'python3':process.execPath;
  const argv=[abs,...args];
  const child=spawn(exe,argv,{
    cwd:work,detached:true,stdio:['ignore',outFd,errFd],
    env:kind==='python'?{...process.env,PYTHONUNBUFFERED:'1'}:process.env
  });
  closeSync(outFd); closeSync(errFd);
  const meta={id,kind,script:relFromRoot(abs),cwd:relFromRoot(work),args,pid:child.pid,
    status:'running',started_at:new Date().toISOString(),ended_at:null,exit_code:null,signal:null};
  await writeJsonFile(jobMetaPath(id),meta);
  child.on('exit', async (code,signal)=>{
    try {
      const m=await readJsonFile(jobMetaPath(id));
      m.status=code===0?'completed':(signal?'terminated':'failed');
      m.exit_code=code; m.signal=signal; m.ended_at=new Date().toISOString();
      await writeJsonFile(jobMetaPath(id),m);
    } catch {}
  });
  child.unref();
  return meta;
}

async function pidMatchesJob(meta) {
  if(!meta.pid) return false;
  try {
    const cmd=(await fs.readFile('/proc/'+meta.pid+'/cmdline')).toString('utf8').split('\0').filter(Boolean);
    const script=insideRoot(meta.script);
    return cmd.includes(script);
  } catch { return false; }
}

async function downloadGithub(urlText, target, expectedSha256, maxBytes) {
  const allowed=new Set(['github.com','raw.githubusercontent.com','objects.githubusercontent.com','release-assets.githubusercontent.com']);
  const initial=new URL(urlText);
  if(initial.protocol!=='https:' || !allowed.has(initial.hostname)) throw new Error('Only approved public GitHub HTTPS hosts are allowed');
  const dst=insideRoot(target); await fs.mkdir(path.dirname(dst),{recursive:true});
  const tmp=dst+'.part-'+process.pid;
  const response=await fetch(initial,{redirect:'follow',headers:{'user-agent':'n100-mcp/0.3'}});
  if(!response.ok) throw new Error('HTTP '+response.status);
  const finalUrl=new URL(response.url);
  if(finalUrl.protocol!=='https:' || !allowed.has(finalUrl.hostname)) throw new Error('Redirected outside approved GitHub hosts');
  const fh=await fs.open(tmp,'w'); const h=createHash('sha256'); let size=0;
  try {
    for await (const chunk of response.body) {
      size += chunk.length;
      if(size>maxBytes) throw new Error('Download exceeds maxBytes');
      h.update(chunk); await fh.write(chunk);
    }
  } finally { await fh.close(); }
  const digest=h.digest('hex');
  if(expectedSha256 && digest.toLowerCase()!==expectedSha256.toLowerCase()) {
    await fs.rm(tmp,{force:true}); throw new Error('SHA256 mismatch');
  }
  await fs.rename(tmp,dst);
  return {path:relFromRoot(dst),size,sha256:digest,url:response.url,verified:Boolean(expectedSha256)};
}

function buildServer() {
  const server = new McpServer(
    { name: 'n100-home-server', version: '0.3.0' },
    { instructions: 'Authorized N100 home-server access restricted to the configured home directory. Structured file, resumable transfer, async jobs, Git/worktree, Python/Node, archive, DDS/image and process tools are available. GitHub-hosted execution remains preferred. No generic shell tool is exposed.' }
  );

  server.registerTool('server_info', {
    description: 'Show server identity, MCP version and filesystem scope'
  }, async () => out({
    hostname: os.hostname(),
    platform: os.platform(),
    root: ROOT,
    port: PORT,
    node: process.version,
    mcp_version: '0.3.0',
    capabilities: [
      'filesystem-read-write','resumable-large-file-transfer','image-read-write','archive',
      'file-search','git-read-write','git-clone-worktree','python-file-runner','node-file-runner',
      'async-jobs','dds-inspect','process-list','github-download','system-status'
    ]
  }));

  server.registerTool('list_dir', {
    description: 'List files and directories under the allowed root',
    inputSchema: z.object({ path: z.string().default('.') })
  }, async ({ path: input }) => {
    const entries = await fs.readdir(insideRoot(input), { withFileTypes: true });
    return out(entries.map(e => ({
      name: e.name,
      type: e.isDirectory() ? 'dir' : e.isFile() ? 'file' : 'other'
    })));
  });

  server.registerTool('file_info', {
    description: 'Stat a file or directory under the allowed root',
    inputSchema: z.object({ path: z.string() })
  }, async ({ path: input }) => {
    const target = insideRoot(input);
    const s = await fs.stat(target);
    return out({
      path: relFromRoot(target), type: s.isDirectory() ? 'dir' : s.isFile() ? 'file' : 'other',
      size: s.size, mode: (s.mode & 0o777).toString(8),
      mtime: s.mtime.toISOString(), ctime: s.ctime.toISOString()
    });
  });

  server.registerTool('hash_file', {
    description: 'Hash a file under the allowed root without transferring its bytes',
    inputSchema: z.object({
      path: z.string(),
      algorithm: z.enum(['sha256','sha1','md5']).default('sha256')
    })
  }, async ({ path: input, algorithm }) => {
    const target = insideRoot(input);
    const s = await fs.stat(target);
    if (!s.isFile()) throw new Error('Not a file');
    return out({ path: relFromRoot(target), size: s.size, algorithm, digest: await hashFile(target, algorithm) });
  });

  server.registerTool('read_text', {
    description: 'Read a UTF-8 text file under the allowed root',
    inputSchema: z.object({
      path: z.string(),
      maxChars: z.number().int().min(1).max(MAX_TEXT).default(100000)
    })
  }, async ({ path: input, maxChars }) => {
    const data = await fs.readFile(insideRoot(input), 'utf8');
    return out(data.slice(0, maxChars));
  });

  server.registerTool('write_text', {
    description: 'Write or append UTF-8 text under the allowed root',
    inputSchema: z.object({
      path: z.string(),
      content: z.string().max(MAX_TEXT),
      append: z.boolean().default(false)
    })
  }, async ({ path: input, content, append }) => {
    const target = insideRoot(input);
    await fs.mkdir(path.dirname(target), { recursive: true });
    if (append) await fs.appendFile(target, content, 'utf8');
    else await fs.writeFile(target, content, 'utf8');
    return out({ ok: true, path: relFromRoot(target), bytes: Buffer.byteLength(content) });
  });

  server.registerTool('read_binary_base64', {
    description: 'Read a small binary file as base64; use hash_file or local runners for large DDS files',
    inputSchema: z.object({
      path: z.string(),
      maxBytes: z.number().int().min(1).max(MAX_BINARY).default(4 * 1024 * 1024)
    })
  }, async ({ path: input, maxBytes }) => {
    const target = insideRoot(input);
    const s = await fs.stat(target);
    if (s.size > maxBytes) throw new Error('File exceeds requested maxBytes');
    const data = await fs.readFile(target);
    return out({ path: relFromRoot(target), size: data.length, base64: data.toString('base64') });
  });

  server.registerTool('write_binary_base64', {
    description: 'Write a small binary file from base64 under the allowed root',
    inputSchema: z.object({
      path: z.string(),
      base64: z.string(),
      overwrite: z.boolean().default(false)
    })
  }, async ({ path: input, base64, overwrite }) => {
    const target = insideRoot(input);
    const data = Buffer.from(base64, 'base64');
    if (data.length > MAX_BINARY) throw new Error('Binary payload exceeds 16 MiB');
    if (!overwrite) {
      try { await fs.access(target); throw new Error('Target exists; set overwrite=true'); }
      catch (e) { if (e.message?.includes('Target exists')) throw e; }
    }
    await fs.mkdir(path.dirname(target), { recursive: true });
    await fs.writeFile(target, data);
    return out({ ok: true, path: relFromRoot(target), bytes: data.length });
  });

  server.registerTool('read_image', {
    description: 'Return a PNG/JPEG/WebP image under the allowed root for visual inspection',
    inputSchema: z.object({ path: z.string() })
  }, async ({ path: input }) => {
    const target = insideRoot(input);
    const ext = path.extname(target).toLowerCase();
    const mime = ext === '.png' ? 'image/png' : (ext === '.jpg' || ext === '.jpeg') ? 'image/jpeg' : ext === '.webp' ? 'image/webp' : null;
    if (!mime) throw new Error('Supported image types: png, jpg, jpeg, webp');
    const s = await fs.stat(target);
    if (s.size > MAX_IMAGE) throw new Error('Image exceeds 12 MiB');
    const data = await fs.readFile(target);
    return imageOut(data, mime, { path: relFromRoot(target), size: data.length });
  });

  server.registerTool('make_dir', {
    description: 'Create a directory under the allowed root',
    inputSchema: z.object({ path: z.string(), recursive: z.boolean().default(true) })
  }, async ({ path: input, recursive }) => {
    const target = insideRoot(input);
    await fs.mkdir(target, { recursive });
    return out({ ok: true, path: relFromRoot(target) });
  });

  server.registerTool('copy_path', {
    description: 'Copy a file or directory within the allowed root',
    inputSchema: z.object({
      source: z.string(), destination: z.string(),
      recursive: z.boolean().default(false), overwrite: z.boolean().default(false)
    })
  }, async ({ source, destination, recursive, overwrite }) => {
    const src = insideRoot(source), dst = insideRoot(destination);
    await fs.mkdir(path.dirname(dst), { recursive: true });
    await fs.cp(src, dst, { recursive, force: overwrite, errorOnExist: !overwrite });
    return out({ ok: true, source: relFromRoot(src), destination: relFromRoot(dst) });
  });

  server.registerTool('move_path', {
    description: 'Move or rename a file/directory within the allowed root',
    inputSchema: z.object({ source: z.string(), destination: z.string(), overwrite: z.boolean().default(false) })
  }, async ({ source, destination, overwrite }) => {
    const src = insideRoot(source), dst = insideRoot(destination);
    if (!overwrite) {
      try { await fs.access(dst); throw new Error('Destination exists; set overwrite=true'); }
      catch (e) { if (e.message?.includes('Destination exists')) throw e; }
    } else {
      await fs.rm(dst, { recursive: true, force: true });
    }
    await fs.mkdir(path.dirname(dst), { recursive: true });
    await fs.rename(src, dst);
    return out({ ok: true, source: relFromRoot(src), destination: relFromRoot(dst) });
  });

  server.registerTool('remove_path', {
    description: 'Delete a file or directory under the allowed root. confirm=true is required.',
    inputSchema: z.object({
      path: z.string(), recursive: z.boolean().default(false), confirm: z.literal(true)
    })
  }, async ({ path: input, recursive }) => {
    const target = insideRoot(input);
    if (target === ROOT) throw new Error('Refusing to remove root');
    await fs.rm(target, { recursive, force: false });
    return out({ ok: true, removed: relFromRoot(target) });
  });

  server.registerTool('git_status', {
    description: 'Run git status --short --branch in a repository under the allowed root',
    inputSchema: z.object({ repo: z.string() })
  }, async ({ repo }) => out(await runGit(repo, ['status','--short','--branch'], 15000)));

  server.registerTool('git_diff', {
    description: 'Show current git diff in a repository under the allowed root',
    inputSchema: z.object({ repo: z.string(), staged: z.boolean().default(false) })
  }, async ({ repo, staged }) => out(await runGit(repo, staged ? ['diff','--cached'] : ['diff'], 30000)));

  server.registerTool('git_log', {
    description: 'Show recent commits',
    inputSchema: z.object({ repo: z.string(), limit: z.number().int().min(1).max(100).default(20) })
  }, async ({ repo, limit }) => out(await runGit(repo, ['log','--oneline','--decorate','-n',String(limit)], 15000)));

  server.registerTool('git_fetch', {
    description: 'Fetch a Git remote, optionally pruning deleted refs',
    inputSchema: z.object({
      repo: z.string(), remote: z.string().default('origin'), prune: z.boolean().default(true)
    })
  }, async ({ repo, remote, prune }) => {
    safeToken(remote,'remote');
    return out(await runGit(repo, ['fetch', ...(prune ? ['--prune'] : []), remote], 120000));
  });

  server.registerTool('git_pull_ff', {
    description: 'Fast-forward-only pull from a Git remote',
    inputSchema: z.object({
      repo: z.string(), remote: z.string().default('origin'), branch: z.string().optional()
    })
  }, async ({ repo, remote, branch }) => {
    safeToken(remote,'remote');
    const args=['pull','--ff-only',remote];
    if (branch) { safeToken(branch,'branch'); args.push(branch); }
    return out(await runGit(repo,args,120000));
  });

  server.registerTool('git_switch', {
    description: 'Switch to an existing local branch',
    inputSchema: z.object({ repo: z.string(), branch: z.string() })
  }, async ({ repo, branch }) => {
    safeToken(branch,'branch');
    return out(await runGit(repo,['switch',branch],30000));
  });

  server.registerTool('git_add', {
    description: 'Stage selected repository paths',
    inputSchema: z.object({
      repo: z.string(), paths: z.array(z.string()).min(1).max(200)
    })
  }, async ({ repo, paths }) => {
    const safe = paths.map(safeGitPath);
    return out(await runGit(repo,['add','--',...safe],30000));
  });

  server.registerTool('git_commit', {
    description: 'Create a Git commit from staged changes',
    inputSchema: z.object({ repo: z.string(), message: z.string().min(1).max(1000) })
  }, async ({ repo, message }) => out(await runGit(repo,['commit','-m',message],60000)));

  server.registerTool('git_push', {
    description: 'Push current HEAD or a named branch to a remote',
    inputSchema: z.object({
      repo: z.string(), remote: z.string().default('origin'), branch: z.string().optional()
    })
  }, async ({ repo, remote, branch }) => {
    safeToken(remote,'remote');
    const args=['push',remote];
    if (branch) { safeToken(branch,'branch'); args.push(branch); }
    return out(await runGit(repo,args,120000));
  });

  server.registerTool('run_python_file', {
    description: 'Run an existing Python .py file under the allowed root without invoking a shell',
    inputSchema: z.object({
      script: z.string(),
      args: z.array(z.string()).max(100).default([]),
      cwd: z.string().default('.'),
      timeoutMs: z.number().int().min(1000).max(MAX_EXEC_MS).default(120000)
    })
  }, async ({ script, args, cwd, timeoutMs }) => {
    const abs = insideRoot(script);
    if (path.extname(abs).toLowerCase() !== '.py') throw new Error('Python runner requires a .py file');
    const s = await fs.stat(abs); if (!s.isFile()) throw new Error('Script is not a file');
    const work = insideRoot(cwd);
    const { stdout, stderr } = await execFileAsync('python3',[abs,...args],{
      cwd:work,timeout:timeoutMs,maxBuffer:16*1024*1024,
      env:{...process.env,PYTHONUNBUFFERED:'1'}
    });
    return out({ script:relFromRoot(abs),cwd:relFromRoot(work),stdout,stderr });
  });

  server.registerTool('run_node_file', {
    description: 'Run an existing Node .js/.mjs file under the allowed root without invoking a shell',
    inputSchema: z.object({
      script: z.string(),
      args: z.array(z.string()).max(100).default([]),
      cwd: z.string().default('.'),
      timeoutMs: z.number().int().min(1000).max(MAX_EXEC_MS).default(120000)
    })
  }, async ({ script, args, cwd, timeoutMs }) => {
    const abs = insideRoot(script);
    if (!['.js','.mjs','.cjs'].includes(path.extname(abs).toLowerCase())) throw new Error('Node runner requires .js/.mjs/.cjs');
    const s = await fs.stat(abs); if (!s.isFile()) throw new Error('Script is not a file');
    const work = insideRoot(cwd);
    const { stdout, stderr } = await execFileAsync(process.execPath,[abs,...args],{
      cwd:work,timeout:timeoutMs,maxBuffer:16*1024*1024
    });
    return out({ script:relFromRoot(abs),cwd:relFromRoot(work),stdout,stderr });
  });

  server.registerTool('process_list', {
    description: 'List processes owned by the MCP server user',
    inputSchema: z.object({ limit: z.number().int().min(1).max(500).default(100) })
  }, async ({ limit }) => out(await processList(limit)));


  server.registerTool('prepare_large_file', {
    description: 'Create/truncate a file for resumable chunk upload. Existing files require overwrite=true.',
    inputSchema: z.object({
      path:z.string(), size:z.number().int().min(0).max(1024*1024*1024*1024).optional(),
      overwrite:z.boolean().default(false)
    })
  }, async ({path:input,size,overwrite}) => {
    const target=insideRoot(input); await fs.mkdir(path.dirname(target),{recursive:true});
    if(!overwrite) { try { await fs.access(target); throw new Error('Target exists; set overwrite=true'); } catch(e) { if(e.message?.includes('Target exists')) throw e; } }
    const fh=await fs.open(target,'w'); try { if(size!==undefined) await fh.truncate(size); } finally { await fh.close(); }
    return out({ok:true,path:relFromRoot(target),size:size??0});
  });

  server.registerTool('read_file_chunk', {
    description: 'Read one base64 chunk from any-size file. Repeat with offsets for resumable transfer.',
    inputSchema: z.object({
      path:z.string(), offset:z.number().int().min(0).default(0),
      length:z.number().int().min(1).max(MAX_CHUNK).default(1024*1024)
    })
  }, async ({path:input,offset,length}) => {
    const target=insideRoot(input), st=await fs.stat(target);
    const len=Math.min(length,Math.max(0,st.size-offset)); const buf=Buffer.alloc(len);
    const fh=await fs.open(target,'r'); let bytesRead=0;
    try { if(len) ({bytesRead}=await fh.read(buf,0,len,offset)); } finally { await fh.close(); }
    const data=buf.subarray(0,bytesRead);
    return out({path:relFromRoot(target),offset,bytes:bytesRead,size:st.size,eof:offset+bytesRead>=st.size,
      sha256:createHash('sha256').update(data).digest('hex'),base64:data.toString('base64')});
  });

  server.registerTool('write_file_chunk', {
    description: 'Write one base64 chunk at an offset. Optional chunk SHA-256 guards corruption.',
    inputSchema: z.object({
      path:z.string(), offset:z.number().int().min(0), base64:z.string(),
      expectedSha256:z.string().regex(/^[0-9a-fA-F]{64}$/).optional()
    })
  }, async ({path:input,offset,base64,expectedSha256}) => {
    const target=insideRoot(input), data=Buffer.from(base64,'base64');
    if(data.length>MAX_CHUNK) throw new Error('Chunk exceeds 8 MiB');
    const digest=createHash('sha256').update(data).digest('hex');
    if(expectedSha256 && digest.toLowerCase()!==expectedSha256.toLowerCase()) throw new Error('Chunk SHA256 mismatch');
    await fs.mkdir(path.dirname(target),{recursive:true});
    let fh;
    try { fh=await fs.open(target,'r+'); }
    catch(e) { if(e.code==='ENOENT') fh=await fs.open(target,'w+'); else throw e; }
    try { await fh.write(data,0,data.length,offset); } finally { await fh.close(); }
    const st=await fs.stat(target);
    return out({ok:true,path:relFromRoot(target),offset,bytes:data.length,size:st.size,sha256:digest});
  });

  server.registerTool('finalize_file', {
    description: 'Verify final size and SHA-256 after chunk transfer.',
    inputSchema: z.object({
      path:z.string(), expectedSize:z.number().int().min(0).optional(),
      expectedSha256:z.string().regex(/^[0-9a-fA-F]{64}$/).optional()
    })
  }, async ({path:input,expectedSize,expectedSha256}) => {
    const target=insideRoot(input), st=await fs.stat(target), digest=await hashFile(target,'sha256');
    const sizeOk=expectedSize===undefined || st.size===expectedSize;
    const hashOk=!expectedSha256 || digest.toLowerCase()===expectedSha256.toLowerCase();
    return out({path:relFromRoot(target),size:st.size,sha256:digest,size_ok:sizeOk,sha256_ok:hashOk,verified:sizeOk&&hashOk});
  });

  server.registerTool('write_text_atomic', {
    description: 'Atomically replace a UTF-8 file, optionally requiring its current SHA-256.',
    inputSchema: z.object({
      path:z.string(),content:z.string().max(MAX_TEXT),
      expectedCurrentSha256:z.string().regex(/^[0-9a-fA-F]{64}$/).optional()
    })
  }, async ({path:input,content,expectedCurrentSha256}) => {
    const target=insideRoot(input);
    if(expectedCurrentSha256) {
      const current=await hashFile(target,'sha256');
      if(current.toLowerCase()!==expectedCurrentSha256.toLowerCase()) throw new Error('Current SHA256 mismatch');
    }
    await fs.mkdir(path.dirname(target),{recursive:true});
    const tmp=target+'.tmp-'+process.pid+'-'+Date.now();
    await fs.writeFile(tmp,content,'utf8'); await fs.rename(tmp,target);
    return out({ok:true,path:relFromRoot(target),bytes:Buffer.byteLength(content),sha256:await hashFile(target,'sha256')});
  });

  server.registerTool('find_files', {
    description: 'Recursively find files by glob and optional size range.',
    inputSchema: z.object({
      path:z.string().default('.'), pattern:z.string().default('**/*'),
      minBytes:z.number().int().min(0).optional(), maxBytes:z.number().int().min(0).optional(),
      limit:z.number().int().min(1).max(50000).default(1000)
    })
  }, async ({path:base,pattern,minBytes,maxBytes,limit}) => {
    const root=insideRoot(base), rx=globRegex(pattern), files=await walkFiles(base,Math.min(50000,limit*10));
    const rows=[];
    for(const p of files) {
      const rel=path.relative(root,p).split(path.sep).join('/');
      if(!rx.test(rel)) continue;
      const st=await fs.stat(p);
      if(minBytes!==undefined && st.size<minBytes) continue;
      if(maxBytes!==undefined && st.size>maxBytes) continue;
      rows.push({path:relFromRoot(p),size:st.size,mtime:st.mtime.toISOString()});
      if(rows.length>=limit) break;
    }
    return out(rows);
  });

  server.registerTool('grep_text', {
    description: 'Search UTF-8-ish text files recursively with a plain string or regex.',
    inputSchema: z.object({
      path:z.string().default('.'), query:z.string().min(1),
      regex:z.boolean().default(false), caseSensitive:z.boolean().default(false),
      includeGlob:z.string().default('**/*'), maxFiles:z.number().int().min(1).max(20000).default(2000),
      maxMatches:z.number().int().min(1).max(20000).default(1000)
    })
  }, async ({path:base,query,regex,caseSensitive,includeGlob,maxFiles,maxMatches}) => {
    const root=insideRoot(base), rxGlob=globRegex(includeGlob), files=await walkFiles(base,maxFiles*2), matches=[];
    const rx=regex?new RegExp(query,caseSensitive?'g':'gi'):null;
    const q=caseSensitive?query:query.toLowerCase(); let scanned=0;
    for(const p of files) {
      const rel=path.relative(root,p).split(path.sep).join('/'); if(!rxGlob.test(rel)) continue;
      const st=await fs.stat(p); if(st.size>MAX_SEARCH_FILE) continue; scanned++;
      let textData; try { textData=await fs.readFile(p,'utf8'); } catch { continue; }
      const lines=textData.split(/\r?\n/);
      for(let i=0;i<lines.length;i++) {
        const line=lines[i], ok=rx?(rx.lastIndex=0,rx.test(line)):(caseSensitive?line:line.toLowerCase()).includes(q);
        if(ok) { matches.push({path:relFromRoot(p),line:i+1,text:line.slice(0,1000)}); if(matches.length>=maxMatches) break; }
      }
      if(matches.length>=maxMatches || scanned>=maxFiles) break;
    }
    return out({scanned_files:scanned,matches});
  });

  server.registerTool('tail_text', {
    description: 'Read the last characters of a text/log file.',
    inputSchema: z.object({path:z.string(),maxChars:z.number().int().min(1).max(1000000).default(20000)})
  }, async ({path:input,maxChars}) => {
    const target=insideRoot(input), st=await fs.stat(target), start=Math.max(0,st.size-maxChars*4);
    const fh=await fs.open(target,'r'), buf=Buffer.alloc(st.size-start);
    let n=0; try { ({bytesRead:n}=await fh.read(buf,0,buf.length,start)); } finally { await fh.close(); }
    return out(buf.subarray(0,n).toString('utf8').slice(-maxChars));
  });

  server.registerTool('github_download', {
    description: 'Download a large file directly from approved public GitHub HTTPS hosts, with optional SHA-256 verification.',
    inputSchema: z.object({
      url:z.string().url(), path:z.string(),
      expectedSha256:z.string().regex(/^[0-9a-fA-F]{64}$/).optional(),
      maxBytes:z.number().int().min(1).max(20*1024*1024*1024).default(2*1024*1024*1024)
    })
  }, async ({url,path:target,expectedSha256,maxBytes}) => out(await downloadGithub(url,target,expectedSha256,maxBytes)));

  server.registerTool('git_rev_parse', {
    description: 'Resolve a Git revision such as HEAD or origin/main.',
    inputSchema:z.object({repo:z.string(),ref:z.string().default('HEAD')})
  }, async ({repo,ref}) => { safeToken(ref,'ref'); return out(await runGit(repo,['rev-parse',ref],15000)); });

  server.registerTool('git_clone', {
    description: 'Clone a Git repository into a new directory under the allowed root.',
    inputSchema:z.object({
      url:z.string().min(1), destination:z.string(), branch:z.string().optional(),
      depth:z.number().int().min(1).max(100000).optional()
    })
  }, async ({url,destination,branch,depth}) => {
    if(/[\0\r\n]/.test(url)) throw new Error('Unsafe URL');
    const dst=insideRoot(destination); await fs.mkdir(path.dirname(dst),{recursive:true});
    const args=['clone']; if(depth) args.push('--depth',String(depth)); if(branch){safeToken(branch,'branch');args.push('--branch',branch);}
    args.push('--',url,dst);
    const {stdout,stderr}=await execFileAsync('git',args,{cwd:ROOT,timeout:MAX_EXEC_MS,maxBuffer:16*1024*1024});
    return out({stdout,stderr,destination:relFromRoot(dst)});
  });

  server.registerTool('git_worktree_add', {
    description: 'Create a Git worktree under the allowed root.',
    inputSchema:z.object({
      repo:z.string(),destination:z.string(),ref:z.string().default('HEAD'),
      newBranch:z.string().optional()
    })
  }, async ({repo,destination,ref,newBranch}) => {
    const dst=insideRoot(destination); safeToken(ref,'ref');
    const args=['worktree','add']; if(newBranch){safeToken(newBranch,'branch');args.push('-b',newBranch);} args.push(dst,ref);
    return out(await runGit(repo,args,120000));
  });

  server.registerTool('git_worktree_remove', {
    description: 'Remove a Git worktree. confirm=true is required.',
    inputSchema:z.object({repo:z.string(),destination:z.string(),force:z.boolean().default(false),confirm:z.literal(true)})
  }, async ({repo,destination,force}) => {
    const dst=insideRoot(destination), args=['worktree','remove']; if(force)args.push('--force'); args.push(dst);
    return out(await runGit(repo,args,120000));
  });

  server.registerTool('git_reset_hard', {
    description: 'Destructively reset a worktree to a ref. confirm=true is required.',
    inputSchema:z.object({repo:z.string(),ref:z.string().default('HEAD'),confirm:z.literal(true)})
  }, async ({repo,ref}) => { safeToken(ref,'ref'); return out(await runGit(repo,['reset','--hard',ref],60000)); });

  server.registerTool('git_clean', {
    description: 'Delete untracked files (and optionally directories). confirm=true is required.',
    inputSchema:z.object({repo:z.string(),directories:z.boolean().default(true),ignored:z.boolean().default(false),confirm:z.literal(true)})
  }, async ({repo,directories,ignored}) => {
    const args=['clean','-f']; if(directories)args.push('-d'); if(ignored)args.push('-x');
    return out(await runGit(repo,args,60000));
  });

  server.registerTool('start_python_job', {
    description: 'Start an existing Python file asynchronously; logs and metadata persist under .n100-mcp/jobs.',
    inputSchema:z.object({script:z.string(),args:z.array(z.string()).max(100).default([]),cwd:z.string().default('.')})
  }, async ({script,args,cwd}) => out(await startFileJob('python',script,args,cwd)));

  server.registerTool('start_node_job', {
    description: 'Start an existing Node file asynchronously; logs and metadata persist under .n100-mcp/jobs.',
    inputSchema:z.object({script:z.string(),args:z.array(z.string()).max(100).default([]),cwd:z.string().default('.')})
  }, async ({script,args,cwd}) => out(await startFileJob('node',script,args,cwd)));

  server.registerTool('job_status', {
    description: 'Inspect an async job started by this MCP.',
    inputSchema:z.object({jobId:z.string().uuid()})
  }, async ({jobId}) => {
    const m=await readJsonFile(jobMetaPath(jobId));
    if(m.status==='running' && !(await pidMatchesJob(m))) {
      m.status='unknown_or_exited'; m.ended_at=m.ended_at||new Date().toISOString(); await writeJsonFile(jobMetaPath(jobId),m);
    }
    return out(m);
  });

  server.registerTool('job_output', {
    description: 'Read the tail of stdout/stderr for an async job.',
    inputSchema:z.object({jobId:z.string().uuid(),stream:z.enum(['stdout','stderr']).default('stdout'),maxChars:z.number().int().min(1).max(1000000).default(50000)})
  }, async ({jobId,stream,maxChars}) => {
    await readJsonFile(jobMetaPath(jobId));
    const p=jobLogPath(jobId,stream);
    let data=''; try { data=await fs.readFile(p,'utf8'); } catch {}
    return out({jobId,stream,text:data.slice(-maxChars)});
  });

  server.registerTool('job_cancel', {
    description: 'Terminate an async job started by this MCP. confirm=true is required.',
    inputSchema:z.object({jobId:z.string().uuid(),confirm:z.literal(true)})
  }, async ({jobId}) => {
    const m=await readJsonFile(jobMetaPath(jobId));
    if(!(await pidMatchesJob(m))) return out({ok:false,reason:'process_not_running_or_mismatch',job:m});
    process.kill(m.pid,'SIGTERM'); m.status='cancel_requested'; await writeJsonFile(jobMetaPath(jobId),m);
    return out({ok:true,jobId,pid:m.pid});
  });

  server.registerTool('list_jobs', {
    description: 'List recent async jobs.',
    inputSchema:z.object({limit:z.number().int().min(1).max(500).default(50)})
  }, async ({limit}) => {
    const dir=insideRoot(JOB_DIR_REL); await fs.mkdir(dir,{recursive:true});
    const names=(await fs.readdir(dir)).filter(x=>x.endsWith('.json')).sort().reverse().slice(0,limit);
    const rows=[]; for(const n of names){try{rows.push(await readJsonFile(path.join(dir,n)));}catch{}}
    return out(rows);
  });

  server.registerTool('archive_create', {
    description: 'Create zip/tar/tar.gz/tar.xz archive from a file or directory.',
    inputSchema:z.object({source:z.string(),archive:z.string(),format:z.enum(['zip','tar','tar.gz','tgz','tar.xz']).default('zip')})
  }, async ({source,archive,format}) => {
    const r=await runFixedPython(['archive-create',format,insideRoot(source),insideRoot(archive)],MAX_EXEC_MS); return out({stdout:r.stdout,stderr:r.stderr});
  });

  server.registerTool('archive_list', {
    description: 'List zip/tar archive entries without extracting.',
    inputSchema:z.object({archive:z.string()})
  }, async ({archive}) => { const r=await runFixedPython(['archive-list',insideRoot(archive)]); return out({stdout:r.stdout,stderr:r.stderr}); });

  server.registerTool('archive_extract', {
    description: 'Safely extract zip/tar archive under the allowed root.',
    inputSchema:z.object({archive:z.string(),destination:z.string()})
  }, async ({archive,destination}) => {
    const r=await runFixedPython(['archive-extract',insideRoot(archive),insideRoot(destination)],MAX_EXEC_MS); return out({stdout:r.stdout,stderr:r.stderr});
  });

  server.registerTool('inspect_dds', {
    description: 'Read DDS dimensions, mip count, FourCC/DX10 header and SHA-256 without decoding pixels.',
    inputSchema:z.object({path:z.string()})
  }, async ({path:input}) => {
    const target=insideRoot(input), fh=await fs.open(target,'r'), buf=Buffer.alloc(148);
    let n=0; try { ({bytesRead:n}=await fh.read(buf,0,148,0)); } finally { await fh.close(); }
    if(n<128 || buf.toString('ascii',0,4)!=='DDS ') throw new Error('Not a DDS file');
    const height=buf.readUInt32LE(12),width=buf.readUInt32LE(16),pitch=buf.readUInt32LE(20),depth=buf.readUInt32LE(24),mips=buf.readUInt32LE(28)||1;
    const fourcc=buf.toString('ascii',84,88).replace(/\0/g,''),rgbBits=buf.readUInt32LE(88);
    const masks={r:buf.readUInt32LE(92),g:buf.readUInt32LE(96),b:buf.readUInt32LE(100),a:buf.readUInt32LE(104)};
    let dx10=null; if(fourcc==='DX10' && n>=148) dx10={dxgi_format:buf.readUInt32LE(128),resource_dimension:buf.readUInt32LE(132),misc_flag:buf.readUInt32LE(136),array_size:buf.readUInt32LE(140),misc_flags2:buf.readUInt32LE(144)};
    const st=await fs.stat(target);
    return out({path:relFromRoot(target),width,height,pitch_or_linear_size:pitch,depth,mipmaps:mips,pixel_format:fourcc||('RGB'+rgbBits),rgb_bit_count:rgbBits,masks,dx10,file_size:st.size,sha256:await hashFile(target,'sha256')});
  });

  server.registerTool('image_info', {
    description: 'Inspect image dimensions/mode using Pillow.',
    inputSchema:z.object({path:z.string()})
  }, async ({path:input}) => { const r=await runFixedPython(['image-info',insideRoot(input)]); return out({stdout:r.stdout,stderr:r.stderr}); });

  server.registerTool('image_diff', {
    description: 'Pixel-diff two same-size images and optionally save a diff image.',
    inputSchema:z.object({a:z.string(),b:z.string(),output:z.string().optional()})
  }, async ({a,b,output}) => {
    const args=['image-diff',insideRoot(a),insideRoot(b)]; if(output)args.push('--output',insideRoot(output));
    const r=await runFixedPython(args,MAX_EXEC_MS); return out({stdout:r.stdout,stderr:r.stderr});
  });

  server.registerTool('image_contact_sheet', {
    description: 'Build a labeled contact sheet from PNG/JPG/WebP images.',
    inputSchema:z.object({
      images:z.array(z.string()).min(1).max(100),output:z.string(),columns:z.number().int().min(1).max(10).default(3),
      width:z.number().int().min(64).max(2048).default(512)
    })
  }, async ({images,output,columns,width}) => {
    const args=['contact',insideRoot(output),String(columns),String(width),...images.map(insideRoot)];
    const r=await runFixedPython(args,MAX_EXEC_MS); return out({stdout:r.stdout,stderr:r.stderr});
  });

  server.registerTool('image_crop', {
    description: 'Crop an image to a rectangle and save it under the allowed root.',
    inputSchema:z.object({source:z.string(),output:z.string(),x:z.number().int().min(0),y:z.number().int().min(0),width:z.number().int().min(1),height:z.number().int().min(1)})
  }, async ({source,output,x,y,width,height}) => {
    const r=await runFixedPython(['crop',insideRoot(source),insideRoot(output),String(x),String(y),String(width),String(height)],MAX_EXEC_MS);
    return out({stdout:r.stdout,stderr:r.stderr});
  });

  server.registerTool('system_status', {
    description: 'Show CPU/load, memory, uptime and allowed-root filesystem space.'
  }, async () => {
    const st=await fs.statfs(ROOT);
    return out({hostname:os.hostname(),loadavg:os.loadavg(),cpu_count:os.cpus().length,uptime_seconds:os.uptime(),
      memory:{total:os.totalmem(),free:os.freemem()},
      filesystem:{root:ROOT,block_size:st.bsize,blocks:st.blocks,free_blocks:st.bfree,available_blocks:st.bavail,
        total_bytes:st.bsize*st.blocks,free_bytes:st.bsize*st.bavail}});
  });

  return server;
}

const handler = createMcpHandler(buildServer);
const nodeHandler = toNodeHandler(handler);
const validateHost = localhostHostValidation();
const validateOrigin = localhostOriginValidation();

const httpServer = createServer((req, res) => {
  if (!validateHost(req, res) || !validateOrigin(req, res)) return;

  const requestUrl = new URL(req.url || '/', 'http://127.0.0.1');
  if (requestUrl.pathname.startsWith('/.well-known/oauth-protected-resource')) {
    res.statusCode = 404;
    res.end('Not Found');
    return;
  }

  void nodeHandler(req, res);
});

httpServer.listen(PORT, '127.0.0.1', () => {
  console.error('[n100-mcp] v0.3.0 listening on http://127.0.0.1:' + PORT + '/mcp root=' + ROOT);
});

for (const signal of ['SIGINT', 'SIGTERM']) {
  process.on(signal, async () => {
    httpServer.close();
    await handler.close();
    process.exit(0);
  });
}