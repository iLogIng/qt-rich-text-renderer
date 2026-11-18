import * as std from 'qjs:std';
import { renderMath } from './mathjax-core.mjs';

const inputPath = scriptArgs[1];
const displayMode = scriptArgs[2] !== 'inline';

try {
  if (!inputPath) {
    throw new Error('usage: qjs mathjax-qjs.mjs <input.tex> [display|inline]');
  }
  const input = std.loadFile(inputPath);
  if (input === null) {
    throw new Error(`cannot read ${inputPath}`);
  }
  const svg = renderMath(input, displayMode);
  std.out.puts(svg);
} catch (error) {
  std.err.puts(`mathjax error: ${error && error.message ? error.message : String(error)}\n`);
  std.exit(1);
}
