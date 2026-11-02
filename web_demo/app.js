'use strict';
const $=id=>document.getElementById(id);
const fmt=(v,d=2)=>Number.isFinite(Number(v))?Number(v).toFixed(d):'—';
const int=v=>Number.isFinite(Number(v))?String(Number(v)):'—';
const getJSON=async url=>{const r=await fetch(url);const d=await r.json();if(!r.ok)throw Error(d.error||'Request failed');return d};
const postJSON=async(url,body={})=>{const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const d=await r.json();if(!r.ok)throw Error(d.error||'Request failed');return d};
let busy=false;
function setBusy(v,action){busy=v;$('runButton').disabled=v;$('testButton').disabled=v;$('runButton').textContent=v?(action||'Running...'):'▶  Execute on Linux   ↗';$('testButton').textContent=v?'Running...':'◇ Run test suite'}
function text(id,value){$(id).textContent=value}
function node(tag,cls,content){const n=document.createElement(tag);if(cls)n.className=cls;if(content!==undefined)n.textContent=content;return n}
function loadRun(data){
  const s=data.run,m=data.meta||{};
  if(!s){text('runBadge','No summary');text('consoleText',m.output||'No CSV summary was generated');return}
  const failed=Number(s.failed)>0;
  text('kpiDone',int(s.done));text('kpiDoneSub',`${int(s.failed)} failed · ${int(s.jobs)} total accepted`);
  text('kpiThroughput',fmt(s.throughput));text('kpiP95',fmt(s.p95_ms));text('kpiRetries',int(s.retries));
  text('kpiRetrySub',`${int(s.crc_rejects)} CRC rejects · ${int(s.deadline_misses)} deadline misses`);
  text('stripDone',int(s.done));text('stripFailed',int(s.failed));text('stripUnknown',int(s.unknown));text('stripCrc',int(s.crc_rejects));
  text('runBadge',failed?'Fault outcome':'Completed');$('runBadge').style.color=failed?'#fac77c':'#9cdbce';
  text('runIdLabel',s.run_id);text('consoleText',`$ ./build/labline --jobs ${s.jobs} --workers ${s.workers} --fault ${s.fault}\n${m.output||'Run completed.'}\n\nExit code: ${m.exit_code??'?'}\nDuration: ${fmt(s.duration_ms)} ms\nSoft deadline misses: ${int(s.deadline_misses)}`);
  const audit=data.audit;$('auditDot').className='audit-dot'+(audit?(Number(audit.anomalies)===0?' good':' bad'):'');
  text('auditTitle',audit?(Number(audit.anomalies)===0?'Audit consistency verified':`${int(audit.anomalies)} audit anomaly/anomalies`):'Audit checker not available');
  text('auditDescription',audit?`${audit.accepted} accepted · ${audit.done} done · ${audit.failed} failed · ${audit.anomalies} audit anomalies`:'Audit evidence not generated; inspect terminal logs.');
  renderJobs(data.jobs||[]);renderEvents(data.events||[])
}
function renderJobs(jobs){
  const tbody=$('jobRows');tbody.replaceChildren();
  if(!jobs.length){tbody.append(node('tr','',''));tbody.firstChild.append(node('td','empty','No job records available.'));tbody.firstChild.firstChild.colSpan=4;return}
  jobs.forEach(j=>{const tr=node('tr');tr.append(node('td','',`#${String(j.job_id).padStart(3,'0')}`));
    const state=node('td'),pill=node('span',`pill ${j.state==='DONE'?'done':'failed'}`,j.state);state.append(pill);tr.append(state);
    tr.append(node('td','',`${fmt(j.queue_ms)} ms`));tr.append(node('td','',`${fmt(j.total_ms)} ms`));tbody.append(tr)});
  text('jobCount',`${jobs.length} jobs`);text('jobsNote',`Showing ${jobs.length} job records. Source: web_demo/runs/<run-id>/jobs.csv.`)
}
function renderEvents(events){const root=$('eventRows');root.replaceChildren();if(!events.length){root.textContent='No event records available';return}
  events.slice(-30).forEach(e=>{const l=node('div','event-line');l.append(node('b','',`${String(e.event||'').padEnd(12)} `));l.append(document.createTextNode(`job=${e.job_id||'?'}  state=${e.result||'-'}  ${e.reason||''}`));root.append(l)})
}
function renderBench(data){const root=$('benchRows');root.replaceChildren();if(!data.available){root.append(node('div','empty','Verified report not found in this repository.'));return}
  const sizes=[10,50,100];sizes.forEach(size=>{const group=data.rows.filter(r=>r.jobs===size);if(group.length!==2)return;
    const box=node('div','bench-group'),head=node('div','bench-label');head.append(node('strong','',`${size} jobs`));head.append(node('span','',`${group[0].runs} trials / mode`));box.append(head);
    group.sort((a,b)=>a.workers-b.workers).forEach(row=>{const line=node('div','bar-entry');line.append(node('span','',`${row.workers} worker${row.workers===4?'s':''}`));const p=node('progress',row.workers===4?'concurrent':'');p.max=85;p.value=row.throughput;p.setAttribute('aria-label',`${row.workers} workers: ${row.throughput} jobs per second`);line.append(p);line.append(node('strong','',`${fmt(row.throughput)} /s`));box.append(line)});root.append(box)});
  const pair=data.rows.filter(r=>r.jobs===100).sort((a,b)=>a.workers-b.workers);
  if(pair.length===2){const change=(pair[1].throughput/pair[0].throughput-1)*100;text('benchSummary',`100 jobs: +${fmt(change,1)}% throughput with 4 workers. Job p95 ${fmt(pair[0].p95_ms)} → ${fmt(pair[1].p95_ms)} ms. Synthetic, 3 trials per mode.`)}
}
async function loadInitial(){
  try{const h=await getJSON('/api/health');$('connectionStatus').className='status '+(h.binary_ready?'ok':'error');text('connectionStatus',h.binary_ready?'Engine ready · localhost':'C++ binary not found');}
  catch(e){$('connectionStatus').className='status error';text('connectionStatus','Engine connection failed')}
  try{renderBench(await getJSON('/api/benchmarks'))}catch(e){text('benchRows',`Unable to load report: ${e.message}`)}
  try{const recent=await getJSON('/api/runs');if(recent.runs?.length){loadRun(await getJSON(`/api/runs/${recent.runs[0].id}`))}}catch(e){console.warn(e)}
}
$('runForm').addEventListener('submit',async e=>{e.preventDefault();if(busy)return;const f=new FormData(e.currentTarget);
  const payload={jobs:Number(f.get('jobs')),workers:Number(f.get('workers')),fault:f.get('fault'),seed:42};
  setBusy(true,'Running C++ simulator...');text('runBadge','Running');text('consoleText',`$ ./build/labline --jobs ${payload.jobs} --workers ${payload.workers} --fault ${payload.fault}\n\nExecuting real Linux IPC workload...`);
  try{const data=await postJSON('/api/run',payload);loadRun(data)}catch(err){text('runBadge','Error');text('consoleText',`Error: ${err.message}`)}finally{setBusy(false)}
});
$('testButton').addEventListener('click',async()=>{if(busy)return;setBusy(true,'Running CTest...');text('testHeading','Running existing tests...');text('testLog','ctest --test-dir build --output-on-failure\n\nExecuting test suite on your local machine...');
  try{const result=await postJSON('/api/tests');text('testSymbol',result.pass?'✓':'!');text('testHeading',result.pass?'Automated tests passed':'Automated tests reported failures');text('testDescription',result.total!=null?`${result.passed} / ${result.total} passed · exit ${result.exit_code}`:`CTest exit code: ${result.exit_code}`);text('testLog',result.output||'No output captured')}
  catch(err){text('testHeading','Tests could not start');text('testDescription',err.message);text('testLog',`Error: ${err.message}`)}finally{setBusy(false)}
});
loadInitial();
