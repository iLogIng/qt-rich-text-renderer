import * as std from 'qjs:std';
import { createMermaid } from 'mermaid/slim';
import { flowchart } from '@mermaid-js/diagram-flowchart';
import { sequence } from '@mermaid-js/diagram-sequence';
import { state } from '@mermaid-js/diagram-state';

function send(payload) {
  std.out.puts(JSON.stringify(payload) + '\n');
  std.out.flush();
}

const mermaid = createMermaid({ diagrams: [flowchart, sequence, state] });
let renderCounter = 0;

function initialize(theme) {
  mermaid.initialize({
    startOnLoad: false,
    securityLevel: 'strict',
    theme: theme === 'dark' ? 'dark' : 'default',
    deterministicIds: true,
    deterministicIDSeed: 'locvas-spike',
  });
}

initialize('default');
send({ type: 'ready', renderer: 'mermaid', version: '13.0.0-alpha.3' });

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
    initialize(request.theme);
    const id = `locvas-${++renderCounter}`;
    const { svg } = await mermaid.render(id, String(request.source ?? ''));
    send({ id: request.id ?? '', svg, ms: Date.now() - started });
  } catch (error) {
    send({
      id: request.id ?? '',
      error: error && error.message ? error.message : String(error),
      ms: Date.now() - started,
    });
  }
}
