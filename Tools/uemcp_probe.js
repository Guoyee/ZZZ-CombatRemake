// uemcp_probe.js — 零依赖 UE MCP Bridge 连通性探测（只读 JSON-RPC 调用）
// 用法: node uemcp_probe.js <method> [paramsJson]
// 示例: node uemcp_probe.js get_viewport_info
//       node uemcp_probe.js list_assets '{"path":"/Game/ZZZ"}'
const net = require('net');
const crypto = require('crypto');

const HOST = '127.0.0.1';
const PORT = Number(process.env.UEMCP_PORT || 9877);
const METHOD = process.argv[2] || 'get_viewport_info';
let PARAMS = {};
const ParamsSource = process.env.UEMCP_PARAMS || process.argv[3] || '';
try { PARAMS = ParamsSource ? JSON.parse(ParamsSource) : {}; } catch (e) { console.error('Bad params JSON: ' + e.message); process.exit(1); }

const key = crypto.randomBytes(16).toString('base64');
const socket = net.connect(PORT, HOST);
let handshakeDone = false;
let buffer = Buffer.alloc(0);

const httpReq = [
  `GET / HTTP/1.1`,
  `Host: ${HOST}:${PORT}`,
  'Upgrade: websocket',
  'Connection: Upgrade',
  `Sec-WebSocket-Key: ${key}`,
  'Sec-WebSocket-Version: 13',
  '', ''
].join('\r\n');

const timer = setTimeout(() => { console.error('TIMEOUT'); process.exit(1); }, 20000);

socket.on('connect', () => socket.write(httpReq));

socket.on('data', (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  if (!handshakeDone) {
    const idx = buffer.indexOf('\r\n\r\n');
    if (idx === -1) return;
    const header = buffer.slice(0, idx).toString();
    if (!header.includes('101')) {
      console.error('HANDSHAKE FAILED:\n' + header);
      process.exit(1);
    }
    handshakeDone = true;
    buffer = buffer.slice(idx + 4);
    const payload = Buffer.from(JSON.stringify({ jsonrpc: '2.0', id: 1, method: METHOD, params: PARAMS }));
    const mask = crypto.randomBytes(4);
    let headerLen, len;
    if (payload.length < 126) { headerLen = 2; len = payload.length; }
    else if (payload.length < 65536) { headerLen = 4; len = 126; }
    else { headerLen = 10; len = 127; }
    const frame = Buffer.alloc(headerLen + 4 + payload.length);
    frame[0] = 0x81; // FIN + text
    frame[1] = len | 0x80; // masked
    let off = 2;
    if (len === 126) { frame.writeUInt16BE(payload.length, 2); off = 4; }
    else if (len === 127) { frame.writeBigUInt64BE(BigInt(payload.length), 2); off = 10; }
    mask.copy(frame, off); off += 4;
    for (let i = 0; i < payload.length; i++) frame[off + i] = payload[i] ^ mask[i % 4];
    socket.write(frame);
  } else {
    let pos = 0;
    while (buffer.length - pos >= 2) {
      const b0 = buffer[pos], b1 = buffer[pos + 1];
      const opcode = b0 & 0x0f;
      let len = b1 & 0x7f;
      let hdr = 2;
      if (len === 126) { if (buffer.length - pos < 4) break; len = buffer.readUInt16BE(pos + 2); hdr = 4; }
      else if (len === 127) { if (buffer.length - pos < 10) break; len = Number(buffer.readBigUInt64BE(pos + 2)); hdr = 10; }
      if (buffer.length - pos < hdr + len) break;
      const payload = buffer.slice(pos + hdr, pos + hdr + len);
      pos += hdr + len;
      if (opcode === 0x8) { console.error('SERVER CLOSED'); process.exit(1); }
      if (opcode === 0x1 || opcode === 0x0) {
        clearTimeout(timer);
        const text = payload.toString('utf8');
        try { console.log(JSON.stringify(JSON.parse(text), null, 2)); }
        catch { console.log(text); }
        process.exit(0);
      }
    }
    buffer = buffer.slice(pos);
  }
});

socket.on('error', (e) => { console.error('SOCKET ERROR: ' + e.message); process.exit(1); });
