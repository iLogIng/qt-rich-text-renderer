import { mathjax } from 'mathjax-full/js/mathjax.js';
import { TeX } from 'mathjax-full/js/input/tex.js';
import { SVG } from 'mathjax-full/js/output/svg.js';
import { liteAdaptor } from 'mathjax-full/js/adaptors/liteAdaptor.js';
import { RegisterHTMLHandler } from 'mathjax-full/js/handlers/html.js';
import { AllPackages } from 'mathjax-full/js/input/tex/AllPackages.js';

const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);

const tex = new TeX({ packages: AllPackages });
const svg = new SVG({ fontCache: 'local' });
const document = mathjax.document('', { InputJax: tex, OutputJax: svg });

export function renderMath(texString, display = true, options = {}) {
  const node = document.convert(texString, {
    display,
    em: options.em ?? 16,
    ex: options.ex ?? 8,
    containerWidth: options.containerWidth ?? 800,
    scale: options.scale ?? 1,
  });
  return adaptor.innerHTML(node);
}
