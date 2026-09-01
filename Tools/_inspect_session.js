// Diagnostic: inspect a DSH session jsonl (uncompressed) for tool catalog + messages.
const fs = require('fs');
const file = process.argv[2];
const lines = fs.readFileSync(file, 'utf8').split('\n').filter(Boolean);
for (const line of lines) {
  let ev;
  try { ev = JSON.parse(line); } catch { continue; }
  if (ev.type === 'request/context') {
    console.log('=== request/context ===');
    const d = ev.data || {};
    console.log('top keys:', Object.keys(d).join(', '));
    // tools may be d.tools or d.sections[].tools — walk shallowly
    const found = [];
    const walk = (o, depth) => {
      if (!o || depth > 4) return;
      if (Array.isArray(o)) { o.forEach(x => walk(x, depth + 1)); return; }
      if (typeof o === 'object') {
        if (Array.isArray(o.tools)) { found.push(...o.tools.map(t => (typeof t === 'string' ? t : (t && (t.name || t.title)))).filter(Boolean)); }
        for (const k of Object.keys(o)) walk(o[k], depth + 1);
      }
    };
    walk(d, 0);
    const uniq = [...new Set(found)];
    console.log('tool count:', uniq.length);
    console.log('mcp tools:', uniq.filter(t => String(t).startsWith('mcp__')).join(', ') || '(none)');
    console.log('sample tools:', uniq.slice(0, 30).join(', '));
  } else if (ev.type === 'user/message') {
    const d = ev.data || ev.message || ev;
    const text = JSON.stringify(d).slice(0, 300);
    console.log('\n=== user/message ===', text);
  } else if (ev.type === 'assistant/message') {
    const d = ev.data || ev.message || ev;
    const text = JSON.stringify(d).slice(0, 300);
    console.log('\n=== assistant/message ===', text);
  } else if (ev.type === 'tool/call') {
    const d = ev.data || ev;
    console.log('\n=== tool/call ===', (d.name || '?'), JSON.stringify(d.arguments || d.args || {}).slice(0, 150));
  } else if (ev.type === 'tool/result' && (JSON.stringify(ev).includes('error') || JSON.stringify(ev).includes('mcp'))) {
    console.log('\n=== tool/result (err/mcp) ===', JSON.stringify(ev).slice(0, 400));
  }
}
