# Third-party notices

This spike uses:

- **Qt 6** — LGPLv3 / commercial. This project links dynamically against the system Qt installation for the spike; a distributed build must comply with Qt's LGPL terms.
- **QuickJS-ng** — MIT. See `third_party/quickjs-ng/LICENSE`.
- **MathJax** (`mathjax-full` 3.2.2) — Apache-2.0.
- **Mermaid** 13.0.0-alpha.3 — MIT.
- **esbuild** — MIT (build-time only).
- **Node.js** — Node.js license (build-time only; not bundled into the AppImage).
- **linuxdeploy / appimagetool** — build-time packaging tools; see their respective licenses.

The generated AppImage includes Qt libraries and Qt plugins; the distribution must include the corresponding license texts and allow relinking/replacement as required by Qt's LGPL terms.
