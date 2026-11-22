# locvas 本地 Markdown 渲染契约

## 1. 目的与范围

`rich-text-renderer` 是 locvas 本地 Markdown 富文本渲染能力的参考实现与验证载体。本文档规定 locvas 期望的渲染行为、交互边界、资源约束和验收条件。

覆盖范围：

- 本地 Markdown / 文本文件的阅读；
- Qt Widgets 与 `QTextDocument` 的文档渲染；
- MathJax、Mermaid 的 QuickJS 渲染与 SVG 嵌入；
- 链接、导航历史、目录、只读 Source、媒体预览和设置。

不包含：

- locvas 的产品界面、数据存储、网络协议和业务逻辑；
- 文档编辑、保存和未保存状态；
- 远程图片下载、QtWebEngine、Node.js 运行时。

实现流程见 [docs/FLOW.md](docs/FLOW.md)，测量数据见 [docs/RESULTS.md](docs/RESULTS.md)，渲染路线取舍见 [docs/ADR-0001-rendering-pipeline.md](docs/ADR-0001-rendering-pipeline.md)。

当前 UI 不采用 QML，配置方案采用 Widgets + QPalette + QSS + 主题 token。Widgets/QSS 与 QML 的对比、UI 自由度边界和未来重新评估条件见 [FOR_LOCVAS_IF_QML.md](FOR_LOCVAS_IF_QML.md)。本文契约中的 Markdown 渲染管线约束保持有效。

## 2. Markdown 与文档契约

### 2.1 输入

- 只接受本机普通文件，不加载远程文档。
- 目录、二进制文件、超过 32MiB 的文件必须被拒绝，且不得替换当前文档。
- 文本解码必须覆盖 UTF-8（含 BOM）、带 BOM 的 UTF-16 LE/BE 和 Latin-1。
- 无 BOM 的 UTF-16 与 UTF-32 必须按二进制处理。
- 文件大小和 mtime 可用于加载审计。

### 2.2 渲染

- Markdown 必须使用 Qt 原生 importer；不得引入 WebEngine、QML/MarkQml 或自写 Markdown parser。
- 必须覆盖标题、段落、强调、链接、图片、表格、行内代码和围栏代码等常用子集。
- 复杂嵌套列表、脚注和部分 GFM 扩展不在当前保证范围内。
- 所有围栏代码必须按代码处理，只有 `mermaid` 例外；`latex` 和 `math` 围栏不得自动渲染为公式。
- 代码高亮必须使用 Qt `QSyntaxHighlighter`，覆盖 C++、Python、JavaScript、JSON、Bash 和 LaTeX。
- Source 必须是只读视图，不提供编辑、保存或 dirty 状态。
- 标题目录必须从 `QTextDocument` 的标题层级生成；点击条目把目标 block 对齐到 Preview 顶部。
- Outline 固定在主窗口左侧，只能隐藏，不能浮动或取下。

### 2.3 加载与布局

- `Load` 与 `--file` 必须共用同一条加载入口；错误呈现方式可以不同。
- 加载完成后必须完成整篇文档布局，再执行滚动复位或 anchor 定位。
- 只查询 `documentSize()` 不足以完成布局；加载收尾必须使文档重新布局，避免长文档只得到部分 block 高度。
- 普通加载必须复位到文档顶部；有效 fragment 必须定位到对应标题；无效 fragment 必须回到顶部。
- 异步数学/Mermaid block 嵌入完成后，必须重新完成布局并再次校正 anchor。
- 加载收尾必须刷新完整 Preview viewport。

## 3. 数学、Mermaid 与 SVG

### 3.1 数学

- 必须支持 `$...$`、`\(...\)`、`$$...$$`、`\[...\]`。
- TeX 必须通过 QuickJS-ng + MathJax 3.2.2 渲染为 SVG，不依赖服务端或 Node 运行时。
- `currentColor` 必须替换为当前主题的前景色。
- `ex`、`em`、百分比等根尺寸必须归一化为像素，保证 `QSvgRenderer` 可预测地布局。
- 数学渲染失败时，必须在文档中保留可读的错误占位，不得中断其余内容。

### 3.2 Mermaid

- Mermaid 必须通过 QuickJS-ng 的全量 worker 渲染为 SVG。
- 版本固定为 `13.0.0-alpha.3`；不得在运行时下载或切换版本。
- 常用图类型的 smoke test 必须保持 24/24 通过；全部 33 种图不构成当前保证。
- 输出必须使用确定性 ID。
- CSS 变量、8 位十六进制颜色和 `width="100%"` 必须在嵌入前归一化，保证 QtSvg 兼容。

### 3.3 SVG 嵌入

- SVG 必须通过 `QTextObjectInterface` 保存，并在绘制时由 `QSvgRenderer` 直接渲染。
- 不得为每个 block 常驻 `QImage`。
- SVG 缓存必须保存原始 SVG；颜色和主题在嵌入阶段应用。
- `SvgSanitizer` 负责 QtSvg 兼容和基础清理，但不是完整安全清洗器。

## 4. 链接、历史与媒体

### 4.1 链接

- 相对链接必须以当前 Markdown 文件所在目录为 base。
- `#anchor` 必须在文档内跳转；没有显式 anchor 的标题按 GitHub 风格 slug 匹配。
- 本机非二进制、非目录文件必须直接在窗口内加载并重新渲染，不得调用系统文本编辑器。
- 本地图片文件必须进入独立媒体预览，不进入文本渲染链路。
- `http/https` 和 `mailto:` 必须经用户确认后交给系统处理。
- `javascript:`、`data:` 等 scheme 必须直接拒绝。
- 单文件读取上限为 32MiB；当前没有 workspace allowlist。

### 4.2 导航历史

- 历史必须使用显式双向链表，最多保存 64 条。
- 新导航发生在历史中间时，forward 分支必须被丢弃。
- `Back` / `Forward` 必须遵循浏览器语义。
- 下拉列表必须最新在上，只显示文件名，tooltip 显示完整路径。
- 历史只保存文件路径，不保存滚动位置或锚点。

### 4.3 媒体预览

- 支持栅格图片和 SVG 的独立非模态预览。
- 缩放范围必须是 5%–800%，支持 Fit、1:1、`Ctrl`+滚轮和拖拽平移。
- SVG 必须保持矢量渲染。
- `Save As` 必须支持：
  - 同格式栅格图片直接复制原始文件字节；
  - 其他栅格格式通过 `QImageWriter` 重编码；
  - SVG 保存原始 SVG，或导出 PNG。
- 远程图片不得下载。

## 5. 设置与数据

- `config/settings.ini` 是只读默认模板；用户覆盖写入 `QStandardPaths::AppConfigLocation/settings.ini`。
- 用户覆盖不得重写模板，避免丢失模板注释。
- 设置项至少覆盖：
  - `appearance.theme`、应用/预览字号、Source 字号、Source 高亮；
  - `view.outlineVisible`、`view.sourceVisible`；
  - `preview.maxSvgWidth`；
  - MathJax/Mermaid 请求超时和 worker 空闲超时；
  - 最近目录和窗口几何。
- Settings 对话框必须提供 Appearance、View、Preview、Advanced 四组设置和 Restore Defaults。
- `--dark` 只影响当前进程，不得自动写回用户配置。
- SVG 缓存必须是内容寻址的文件缓存，key 至少包含：
  - 渲染器版本；
  - kind（math/mermaid）；
  - 源内容；
  - display/options；
  - Mermaid 主题。
- 当前缓存没有容量上限或自动清理。

## 6. 进程与资源约束

- 必须使用两个懒启动 qjs worker：MathJax 与 Mermaid。
- qjs worker 数量上限为 2。
- worker 空闲 60 秒后退出；测试可通过环境变量缩短。
- worker 必须有请求超时、内存/CPU rlimit 和崩溃重启路径。
- 总应用 PSS 验收目标为 ≤128MiB。
- 参考测量和当前基线见 [docs/RESULTS.md](docs/RESULTS.md)。
- 运行时不得依赖 Node.js 或 QtWebEngine。
- 当前渲染队列为 FIFO，不保证可见区域优先。

## 7. 打包

- 必须使用 Release 构建生成 `dist/rich-text-renderer-x86_64.AppImage`；打包脚本不得接受 `RelWithDebInfo` 或其他构建类型。
- AppImage 必须包含：
  - `qjs`；
  - `mathjax-worker.mjs`；
  - `mermaid-full-worker.mjs`；
  - `config/settings.ini` 默认模板。
- AppImage 不得包含测试用 one-shot bundle 或 WebEngine。
- 当前 AppImage 约 34.3MB / 32.7MiB。
- 打包脚本使用 `linuxdeploy --plugin qt` 和 `appimagetool`；`--appimage-extract-and-run` 路径不要求宿主安装 FUSE。
- CI 必须固定 linuxdeploy、appimagetool、AppImage runtime 和打包基础镜像版本。
- 当前只在 Fedora Linux x86_64 环境验证；干净虚拟机和其他发行版验证仍待完成。

## 8. 安全边界

- 本地链接可在窗口内读取任意非二进制文件，单文件上限 32MiB；尚无 workspace allowlist。
- 文本/二进制判断基于 NUL、MIME 和 UTF-8 有效性，是启发式检查，不是内容审计。
- `SvgSanitizer` 不是完整安全清洗器；MathJax/Mermaid 输入没有严格的资源或复杂度限制。
- qjs 只运行仓库内的受信任 bundle；第三方 JavaScript 不在当前范围内。
- 远程图片不下载。
- 尚未进行独立安全审计。

## 9. 当前版本不包含

- 文档编辑、保存和未保存状态；
- 每条消息复制、代码块复制、跨消息选区；
- 流式 token 增量渲染；
- 可见区域优先队列；
- SQLite/Blob 缓存和缓存容量控制；
- KSyntaxHighlighting；
- 远程图片下载；
- Windows/macOS 打包；
- 完整 GFM、复杂 SVG 滤镜和浏览器级 SVG 特性。

## 10. 验收

### 10.1 构建与测试

```bash
scripts/build.sh
scripts/test.sh
scripts/test-markdown.sh
scripts/test-mermaid-full.py
scripts/test-process-bound.sh
scripts/test-idle.sh
```

`scripts/test.sh` 运行 13 个 CTest 目标，并执行渲染器 smoke test。

### 10.2 契约映射

| 契约 | 主要实现 | 验证 |
|---|---|---|
| Markdown、数学、Mermaid | `MarkdownPreprocessor`、`RendererService`、qjs workers | `markdown-preprocessor-test`、`document-rendering-test`、`scripts/test-markdown.sh`、`scripts/test-mermaid-full.py` |
| SVG 嵌入与归一化 | `SvgSanitizer`、`MarkdownDocumentBuilder`、`SvgObjectHandler` | `svg-sanitizer-test`、`document-rendering-test` |
| 缓存 | `SvgCache` | `svg-cache-test` |
| 链接与本地文件 | `LinkResolver`、`MarkdownLinkHandler`、`LocalFileLoader` | `link-resolver-test`、`link-handler-test`、`local-file-loader-test` |
| 导航历史 | `NavigationHistory` | `navigation-history-test` |
| 设置 | `AppSettings`、`SettingsDialog` | `app-settings-test`、`settings-dialog-test` |
| 目录与启动 | `MarkdownOutline`、`MainWindow` | `markdown-outline-test`、`main-window-startup-test` |
| 加载与布局收尾 | `MainWindow::loadFile()`、`openLocalFile()`、`finishPreviewLoad()` | `main-window-startup-test` |
| 媒体预览 | `MediaPreviewWindow` | `media-preview-test` |
| 进程与资源 | `RendererService`、`RendererWorker` | `scripts/test-process-bound.sh`、`scripts/test-idle.sh` |

### 10.3 AppImage

```bash
QT_QPA_PLATFORM=offscreen ./dist/rich-text-renderer-x86_64.AppImage \
  --appimage-extract-and-run --file samples/sample.md --dark \
  --screenshot out/appimage-dark.png
```

AppImage 必须能够启动、加载本地文档、渲染数学/Mermaid，并正常退出。
