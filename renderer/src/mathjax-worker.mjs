import * as std from 'qjs:std';
import { renderMath } from './mathjax-core.mjs';

function send(payload) {
  std.out.puts(JSON.stringify(payload) + '\n');
  std.out.flush();
}

send({ type: 'ready', renderer: 'mathjax', version: '3.2.2' });

while (true) {
  const line = std.in.getline();
  if (line === null) {
    break;
  }
  if (!line.trim()) {
    continue;
  }

  let request;
  try {
    request = JSON.parse(line);
  } catch (error) {
    send({ id: '', error: `invalid JSON: ${error && error.message ? error.message : String(error)}` });
    continue;
  }

  if (request.cmd === 'shutdown') {
    break;
  }

  const started = Date.now();
  try {
    const svg = renderMath(String(request.source ?? ''), request.display !== false);
    send({ id: request.id ?? '', svg, ms: Date.now() - started });
  } catch (error) {
    send({
      id: request.id ?? '',
      error: error && error.message ? error.message : String(error),
      ms: Date.now() - started,
    });
  }
}
