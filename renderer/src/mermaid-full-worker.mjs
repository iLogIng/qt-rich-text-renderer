import * as std from 'qjs:std';
import mermaid from 'mermaid';

function send(payload) {
  std.out.puts(JSON.stringify(payload) + '\n');
  std.out.flush();
}

const allowedThemes = new Set(['default', 'dark', 'forest', 'neutral', 'base']);
const allowedVariables = new Set([
  'primaryColor',
  'lineColor',
  'textColor',
  'mainBkg',
  'nodeBorder',
  'nodeTextColor',
  'arrowheadColor',
  'edgeLabelBackground',
]);

function themeConfig(request) {
  const name = allowedThemes.has(request.theme) ? request.theme : 'default';
  const variables = {};
  if (request.themeVariables && typeof request.themeVariables === 'object') {
    for (const key of allowedVariables) {
      const value = request.themeVariables[key];
      if (typeof value === 'string') {
        variables[key] = value;
      }
    }
  }
  return { name, variables };
}

function initialize(config) {
  mermaid.initialize({
    startOnLoad: false,
    securityLevel: 'strict',
    theme: config.name,
    themeVariables: config.variables,
    deterministicIds: true,
    deterministicIDSeed: 'rich-text-renderer',
  });
}

initialize({ name: 'default', variables: {} });
send({ type: 'ready', renderer: 'mermaid-full', version: '13.0.0-alpha.3' });

let renderCounter = 0;
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
    initialize(themeConfig(request));
    const { svg } = await mermaid.render(`rich-text-renderer-${++renderCounter}`, String(request.source ?? ''));
    send({ id: request.id ?? '', svg, ms: Date.now() - started });
  } catch (error) {
    send({
      id: request.id ?? '',
      error: error && error.message ? error.message : String(error),
      ms: Date.now() - started,
    });
  }
}
