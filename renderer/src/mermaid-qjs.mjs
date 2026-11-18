import * as std from 'qjs:std';
import { createMermaid } from 'mermaid/slim';
import { flowchart } from '@mermaid-js/diagram-flowchart';
import { sequence } from '@mermaid-js/diagram-sequence';
import { state } from '@mermaid-js/diagram-state';

const mermaid = createMermaid({ diagrams: [flowchart, sequence, state] });
const theme = scriptArgs[2] === 'dark' ? 'dark' : 'default';
mermaid.initialize({
  startOnLoad: false,
  securityLevel: 'strict',
  theme,
  deterministicIds: true,
  deterministicIDSeed: 'locvas-spike',
});

async function main() {
  const inputPath = scriptArgs[1];
  if (!inputPath) {
    throw new Error('usage: qjs mermaid-qjs.mjs <input.mmd>');
  }
  const input = std.loadFile(inputPath);
  if (input === null) {
    throw new Error(`cannot read ${inputPath}`);
  }
  const { svg } = await mermaid.render('locvas-spike', input);
  std.out.puts(svg);
}

main().catch((error) => {
  std.err.puts(`mermaid error: ${error && error.message ? error.message : String(error)}\n`);
  std.exit(1);
});
