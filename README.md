# rich-text-renderer

面向本地 Markdown 文件的 Qt6 桌面阅读器。应用使用 `QTextDocument` 排版文档，通过 QuickJS-ng worker 运行 MathJax 与 Mermaid，将公式和图表渲染为 SVG 后嵌入预览；运行时不依赖 QtWebEngine，也不需要 Node.js。

本仓库同时是 locvas 本地 Markdown 富文本渲染能力的参考实现。locvas 相关的行为契约、资源约束和验收条件见 [FOR_LOCVAS.md](FOR_LOCVAS.md)。

![rich-text-renderer 深色模式界面](docs/assets/app-dark.png)

## 功能

- 常用 Markdown 子集：标题、段落、强调、链接、图片、表格、行内代码和围栏代码。
- 行内与块级 TeX：支持 `$...$`、`\(...\)`、`$$...$$`、`\[...\]`。
- Mermaid 图表：通过全量 Mermaid worker 渲染，固定版本 `13.0.0-alpha.3`。
- 代码高亮：C++、Python、JavaScript、JSON、Bash、LaTeX，基于 Qt `QSyntaxHighlighter`。
- 本地文件导航：相对链接、标题锚点、窗口内文件切换和最多 64 条浏览器式历史。
- 目录面板：从文档标题生成，点击条目把目标位置对齐到 Preview 顶部。
- 只读 Source 视图：提供 Markdown 语法高亮，不提供编辑和保存。
- 媒体预览：图片和渲染后的 SVG 支持缩放、平移、Fit、1:1 和另存。
- 浅色/深色主题、可配置字体、视图状态和渲染器超时。
- 内容寻址 SVG 缓存，以及 MathJax / Mermaid 两个懒启动 qjs worker。
- AppImage 打包，不包含 WebEngine 或测试用一次性渲染脚本。

## 环境要求

| 依赖 | 要求 |
|---|---|
| 操作系统 | Linux x86_64；当前在 Fedora 上验证 |
| Qt | 6.8 或更高，需 Widgets、Svg、SvgWidgets |
| CMake | 3.21 或更高 |
| 编译器 | 支持 C++20 |
| QuickJS-ng | `third_party/quickjs-ng` 子模块，以静态库构建 |
| Node.js / npm | 仅构建 renderer bundle 时需要 |
| linuxdeploy / appimagetool | 可选，仅生成 AppImage 时需要 |

开发环境使用 Qt 6.11.2、CMake 4.3.0、GCC 16.2.1、QuickJS-ng 0.18.0 和 Node.js 22。

## 构建

### 脚本方式

```bash
git submodule update --init --recursive third_party/quickjs-ng

scripts/setup-quickjs.sh
scripts/setup-renderer.sh
scripts/build.sh
```

### 手动方式

构建 QuickJS-ng：

```bash
git submodule update --init --recursive third_party/quickjs-ng

cmake -S third_party/quickjs-ng -B third_party/quickjs-ng/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DQJS_BUILD_LIBC=ON
cmake --build third_party/quickjs-ng/build --target qjs_exe -j"$(nproc)"
```

构建 MathJax / Mermaid bundle：

```bash
cd renderer
npm ci
npm run build
cd ..
```

构建 Qt 应用：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
```

产物为 `build/rich-text-renderer`。

## 运行

```bash
./build/rich-text-renderer
```

应用默认不加载文档。使用工具栏的 `Load` 选择本地 Markdown 或文本文件，或通过命令行指定文件：

```bash
./build/rich-text-renderer --file samples/sample.md
```

### 命令行参数

| 参数 | 说明 |
|---|---|
| `--file <path>` | 启动时加载指定本地文件 |
| `--dark` | 以深色主题启动，仅覆盖当前进程 |
| `--toggle-theme` | 启动 1.5 秒后切换一次主题，主要用于截图测试 |
| `--screenshot <path>` | 启动 8 秒后保存窗口截图 |

### 调试输出

| 环境变量 | 作用 |
|---|---|
| `RICH_TEXT_RENDER_DEBUG_LOAD=1` | 输出加载来源、路径、大小、mtime 和 SHA-256 审计信息 |
| `RICH_TEXT_RENDER_DEBUG=1` | 输出渲染摘要和缓存命中信息 |
| `RICH_TEXT_RENDER_IDLE_MS=<ms>` | 覆盖 worker 空闲退出时间 |

## 配置

默认模板位于 `config/settings.ini`，包含全部可配置项和说明注释。用户修改通过 QSettings 写入独立文件：

```text
QStandardPaths::AppConfigLocation/settings.ini
```

Linux 上通常为：

```text
~/.config/rich-text-renderer/rich-text-renderer/settings.ini
```

Settings 对话框提供 Appearance、View、Preview、Advanced 四组设置，并支持 Restore Defaults。主要配置项包括：

- 主题、应用/预览字号、Source 字号和 Source 高亮；
- Outline 与 Source 可见性；
- 预览 SVG 最大宽度；
- MathJax / Mermaid 请求超时与 worker 空闲超时；
- 最近目录和窗口几何。

## 支持的 Markdown

| 能力 | 说明 |
|---|---|
| 基础 Markdown | 标题、段落、强调、链接、图片、表格、行内代码、围栏代码 |
| 数学 | 行内与块级 TeX，渲染为 SVG |
| Mermaid | `mermaid` 围栏，使用全量图集 worker |
| 代码围栏 | 除 `mermaid` 外均按代码处理；`latex`、`math` 围栏不渲染为公式 |
| 代码高亮 | C++、Python、JavaScript、JSON、Bash、LaTeX |
| 文件链接 | 窗口内打开本地非二进制文件，并记录导航历史 |
| 本地图片 | 点击图片或图片链接打开独立预览窗口 |

复杂嵌套列表、脚注、部分 GFM 扩展和浏览器级 SVG 特性尚未完整覆盖。

## 交互

- `Load` 使用 Qt 文件对话框；`Render` 或 `Ctrl+Enter` 重新渲染当前文档。
- `Source` 为只读视图，支持 Markdown 语法高亮。
- 左侧 `Outline` 固定停靠，可通过标签栏开关或 Settings 隐藏。
- `Back` / `Forward` 和下拉列表按浏览器语义导航本地文件。
- 本地文件链接在当前窗口打开；`http/https` 和 `mailto:` 打开前需要确认；`javascript:`、`data:` 等 scheme 直接拒绝。
- 图片和渲染 SVG 在独立窗口预览，支持 5%–800% 缩放、Fit、1:1、`Ctrl`+滚轮、拖拽平移和 `Save As`。
- 切换主题会重新渲染当前文档，MathJax 使用新的前景色，Mermaid 使用新的主题参数。

## 架构

渲染管线：

```text
Markdown
  -> MarkdownPreprocessor（提取 math / mermaid，生成占位 token）
  -> QTextDocument::setMarkdown
  -> RendererService（single-flight、缓存、队列）
  -> MathJax / Mermaid qjs worker
  -> SvgCache（内容寻址缓存）
  -> SvgSanitizer（QtSvg 兼容与基础清理）
  -> MarkdownDocumentBuilder（替换 token 为自定义对象）
  -> SvgObjectHandler（绘制时由 QSvgRenderer 渲染）
```

构建目标按依赖方向拆分：

| CMake 目标 | 内容 | 依赖 |
|---|---|---|
| `rich-text-renderer-core` | Markdown 预处理、设置、链接、本地文件、历史、SVG 缓存、渲染服务与 worker | `Qt6::Core` |
| `rich-text-renderer-document` | 文档构建、目录、Source 高亮、SVG 归一化与对象绘制、代码高亮 | `Qt6::Gui`、`Qt6::Svg` |
| `rich-text-renderer-ui` | 主窗口、目录面板、设置对话框、链接与媒体交互、主题 | 前两个库、`Qt6::Widgets`、`Qt6::SvgWidgets` |
| `rich-text-renderer` | 应用入口 | `rich-text-renderer-ui` |

更完整的模块说明见 [docs/FLOW.md](docs/FLOW.md) 和 [docs/ADR-0001-rendering-pipeline.md](docs/ADR-0001-rendering-pipeline.md)。

## 测试

```bash
scripts/test.sh
scripts/test-markdown.sh
scripts/test-mermaid-full.py
scripts/test-process-bound.sh
scripts/test-idle.sh
```

`scripts/test.sh` 运行渲染器 smoke test 和 13 个 CTest 目标，覆盖：

- 链接解析、链接交互和本地文件加载；
- 导航历史；
- 设置模板、用户覆盖和设置对话框；
- Markdown 预处理、目录提取和文档渲染；
- SVG 缓存、SVG 归一化和对象嵌入；
- 启动、Outline 和设置应用；
- 媒体预览的缩放与另存。

其他检查：

- `scripts/test-markdown.sh`：数学分隔符和代码块 token 数量；
- `scripts/test-mermaid-full.py`：常用 Mermaid 图类型 smoke test；
- `scripts/test-process-bound.sh`：worker 数量上限和总 PSS；
- `scripts/test-idle.sh`：worker 空闲退出。

测量数据与已知边界见 [docs/RESULTS.md](docs/RESULTS.md)。

## 打包

```bash
scripts/package-appimage.sh
```

脚本使用 `linuxdeploy --plugin qt` 部署 Qt 依赖，复制 `qjs`、运行时 worker 和默认设置模板，再由 `appimagetool` 生成：

```text
dist/rich-text-renderer-x86_64.AppImage
```

`linuxdeploy` 和 `appimagetool` 默认从 `PATH` 查找，也可以通过环境变量指定：

```bash
LINUXDEPLOY=/path/to/linuxdeploy \
APPIMAGETOOL=/path/to/appimagetool \
scripts/package-appimage.sh
```

脚本使用 `--appimage-extract-and-run`，不要求宿主安装 FUSE。当前 AppImage 约 34.3MB / 32.7MiB。

验证：

```bash
QT_QPA_PLATFORM=offscreen ./dist/rich-text-renderer-x86_64.AppImage \
  --appimage-extract-and-run --file samples/sample.md --dark \
  --screenshot out/appimage-dark.png
```

## 项目结构

```text
src/                    Qt Widgets 应用与渲染、缓存、交互逻辑
renderer/               MathJax / Mermaid 的 QuickJS bundle 与构建脚本
third_party/quickjs-ng  QuickJS-ng 子模块
config/                 settings.ini 默认模板
samples/                示例 Markdown
tests/                  C++ 模块测试和测试输入
tools/svgcheck.cpp      SVG 到 PNG 的 QtSvg 检查工具
docs/                   流程、结果、ADR 和截图
packaging/              desktop 文件与图标
scripts/                构建、测试、基准和打包脚本
dist/                   生成的 AppImage（gitignored）
```

## 文档

- [FOR_LOCVAS.md](FOR_LOCVAS.md)：渲染契约、资源约束与验收条件
- [docs/FLOW.md](docs/FLOW.md)：启动、渲染、缓存、链接和预览流程
- [docs/RESULTS.md](docs/RESULTS.md)：渲染结果、性能数据和已知限制
- [docs/ADR-0001-rendering-pipeline.md](docs/ADR-0001-rendering-pipeline.md)：渲染路线说明
- [TODO.md](TODO.md)：后续工作
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)：第三方组件与许可证

## 已知限制

- Markdown 覆盖常用子集，复杂 GFM 和极端嵌套结构未验证。
- Mermaid 固定为 alpha 版本，升级需要完整回归。
- 不支持文档编辑、保存、代码块复制或跨块任意选区。
- 渲染是文档级重渲染，尚未实现流式增量和可见区域优先队列。
- 缓存是文件缓存，没有容量上限或自动清理。
- `SvgSanitizer` 是 QtSvg 兼容层，不是完整安全清洗器。
- 本地文件链接没有 workspace allowlist。
- AppImage 当前只在 Fedora Linux x86_64 环境验证。

## 第三方与许可证

第三方组件、版本和许可证说明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

主要依赖：

- Qt 6 — LGPLv3 / commercial
- QuickJS-ng — MIT
- MathJax 3.2.2 — Apache-2.0
- Mermaid 13.0.0-alpha.3 — MIT
- esbuild — MIT（仅构建时）
