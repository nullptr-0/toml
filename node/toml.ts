import { spawn } from 'child_process';
import { join } from 'path';

// Grab arguments to pass to the worker
const args = process.argv.slice(2);

// Spawn worker process and pass the args
const worker = spawn('node', [join(__dirname, 'worker.js'), ...args]);

let buffer = Buffer.alloc(0);

function parseMessages() {
  while (true) {
    const str = buffer.toString();
    const headerEnd = str.indexOf('\r\n\r\n');
    if (headerEnd === -1) break; // Not enough data yet

    const headers = str.slice(0, headerEnd);
    const match = headers.match(/Content-Length:\s*(\d+)/i);
    if (!match) {
      console.error('Invalid message: missing Content-Length');
      buffer = Buffer.alloc(0);
      break;
    }

    const bodyLength = parseInt(match[1], 10);
    const totalLength = headerEnd + 4 + bodyLength;

    if (buffer.length < totalLength) break; // Wait for full body

    const fullMessage = buffer.subarray(0, totalLength);
    const body = buffer.subarray(headerEnd + 4, totalLength);

    // Append \n only if not already present
    const needsNewline = body[body.length - 1] !== 0x0a; // 0x0a is '\n'
    const finalMessage = needsNewline
      ? Buffer.concat([fullMessage, Buffer.from('\n')])
      : fullMessage;

    // Send to worker
    worker.stdin.write(finalMessage);

    // Remove processed part
    buffer = buffer.subarray(totalLength);
  }
}

process.stdin.on('data', (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  parseMessages();
});

// Output from worker
worker.stdout.on('data', (data) => {
  process.stdout.write(`${data}`);
});

worker.stderr.on('data', (data) => {
  process.stderr.write(`${data}`);
});

worker.on('exit', (code) => {
  console.log(`Worker exited with code ${code}`);
  process.exit(0);
});
