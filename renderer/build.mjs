import { build } from 'esbuild';
import { mkdir, rm } from 'node:fs/promises';

await rm('dist', { recursive: true, force: true });
await mkdir('dist', { recursive: true });

const common = {
  bundle: true,
  format: 'esm',
  platform: 'neutral',
  target: 'es2022',
  minify: true,
  legalComments: 'inline',
  external: ['qjs:std'],
  logLevel: 'info',
};

await build({
  ...common,
  entryPoints: ['src/mathjax-qjs.mjs'],
  outfile: 'dist/mathjax-qjs.mjs',
});

await build({
  ...common,
  entryPoints: ['src/mermaid-qjs.mjs'],
  outfile: 'dist/mermaid-qjs.mjs',
});

await build({
  ...common,
  entryPoints: ['src/mathjax-worker.mjs'],
  outfile: 'dist/mathjax-worker.mjs',
});

await build({
  ...common,
  entryPoints: ['src/mermaid-full-worker.mjs'],
  outfile: 'dist/mermaid-full-worker.mjs',
});
