# rich-text-render spike

一个独立的 Qt6 富文本渲染 spike，用来验证：

- Qt Widgets + `QTextDocument` 渲染 Markdown；
- QuickJS-ng 运行 MathJax，将 TeX 渲染成 SVG；
- QuickJS-ng 运行 Mermaid v13 alpha，将图渲染成 SVG；
- `QSvgRenderer` 通过自定义 `QTextObjectInterface` 直接绘制 SVG；
- 两个懒启动 qjs worker + 内容寻址缓存 + 异步渲染；
- 本地文件链接在窗口内切换，带浏览器式 back/forward 历史；
- 深色 / 浅色主题切换；
- AppImage 打包与体积测量。

> 这是验证项目，不是生产实现。它证明路线可行，但还没有达到可直接发布的产品形态。

## 0. 当前结论

**可行，而且效果已经达到“可以日常试用”的级别。**

- 数学公式：可用；
- Mermaid：可用（基于 alpha 版本）；
- Markdown 表格、中文、emoji、代码块：可用；
- 数学：支持 `$...$`、`\(...\)`、`$$...$$`、`\[...\]`；
- 所有 fenced code 都按代码处理，只有 `mermaid` 例外；`latex`/`math` fence 不渲染成公式；
- 代码高亮：C++ / Python / JS / JSON / Bash / LaTeX；
- 深色模式：可用，公式和 Mermaid 都会随主题重新渲染；
- 缓存：可用，热缓存基本无感；
- renderer service：两个 qjs worker，进程数上限 2；
- Mermaid：full worker，常用图类型 24/24 smoke test 通过；
- 本地链接：非二进制、非目录文件在当前窗口内打开，不再拉起系统编辑器；
- 历史：双向链表记录最多 64 个本地文件，工具栏右侧提供 back/forward 和下拉列表；
- 内存：30 blocks 总 PSS 约 62.2MB，400 blocks 约 72.0MB；
- AppImage：可用，约 **35.3MB / 34MiB**，无需 WebEngine；
- 不需要 QtWebEngine，不需要 Node 运行时。

当前最大缺口是**复制/选区**、**安全清洗**和**可见区域优先队列**；代码块语法高亮、本地链接与历史导航已完成。

## 1. 效果评估

### 已经不错的部分

- MathJax SVG 在 `QSvgRenderer` 下渲染清晰，分式、根号、求和、积分、上下标都正常；
- Mermaid v13 alpha 的 DOM-free 渲染在 QuickJS 中确实能跑，流程图、边、边标签、中文都正常；
- 深色模式下数学公式使用系统前景色，不再融入背景；
- Mermaid 深色主题会输出深色节点与浅色文字；
- 30 个 block 冷启动约 250–320ms，热缓存约 13–50ms；
- 400 个 block 冷启动约 1.6s，总 PSS 约 72.0MB；
- Mermaid full bundle：常用图类型 24/24 smoke test 通过；
- 两个 qjs worker 常驻上限，空闲 60s 自动退出；
- AppImage 没有 WebEngine 的体积负担。

### 还不够好的部分

- 没有每条消息复制 / 代码块复制 / 跨消息选区；
- Markdown 只覆盖常用子集，复杂 GFM 未验证；
- Mermaid 仍是 alpha，升级需要回归；
- 缓存是文件缓存，不是 SQLite/Blob；
- SVG sanitizer 是 QtSvg 兼容层，不是完整安全清洗器；
- Mermaid 版本是 alpha，升级需要回归测试。

## 2. 架构

```mermaid
flowchart LR
  subgraph UI["Qt Widgets"]
    Editor["Markdown editor"]
    Preview["QTextBrowser preview"]
    Theme["Theme / palette"]
    Nav["Back / Forward<br/>history dropdown"]
  end

  subgraph Core["C++ core"]
    MD["MarkdownRenderer<br/>scan + placeholders"]
    RS["RendererService<br/>cache + queue + single-flight"]
    San["SvgSanitizer<br/>QtSvg normalization"]
    Obj["SvgObjectHandler<br/>QTextObjectInterface"]
    Doc["QTextDocument"]
    Link["MarkdownLinkHandler<br/>local / remote / anchor"]
    Files["LocalFileLoader<br/>text validation"]
    History["NavigationHistory<br/>max 64"]
  end

  subgraph JS["QuickJS-ng workers (lazy, max 2)"]
    MW["MathJax worker"]
    DW["Mermaid worker"]
  end

  Editor --> MD
  MD --> Preview
  MD --> RS
  RS -->|"cache hit"| San
  RS -->|"cache miss"| MW
  RS -->|"cache miss"| DW
  MW --> SVGOut["SVG output"]
  DW --> SVGOut
  SVGOut --> San
  San --> Obj
  Obj --> Doc
  Doc --> Preview
  Preview --> Link
  Link -->|"local file"| Files
  Link -->|"remote"| External["System browser / mail client"]
  Files --> History
  Nav --> History
  History -->|"restore file"| Files
  Theme --> RS
  Theme --> San
```

### 渲染链路

````text
Markdown
  → 扫描 $$...$$ / ```mermaid
  → 替换成唯一 token
  → QTextDocument::setMarkdown()
  → RendererService 检查缓存 / single-flight
  → cache miss 进入对应 worker
      → MathJax 或 Mermaid 输出 SVG
      → 写入内容寻址缓存
  → SvgSanitizer 归一化
  → token 被替换为自定义对象字符
  → SvgObjectHandler 在绘制时用 QSvgRenderer 直接画 SVG
````

### 缓存

```text
cache key = SHA256(
  renderer version |
  kind (math/mermaid) |
  source |
  display/options |
  theme (mermaid)
)
```

当前缓存目录：

```text
~/.cache/locvas-spike/rich-text-render-spike/rich-text-render-spike/
```

缓存的是**原始 SVG**；颜色/主题在嵌入时由 `SvgSanitizer` 应用。Mermaid 主题不同会生成不同 SVG，因此主题进入 cache key。

## 3. 模块划分

| 模块             | 路径                                  | 职责                                                 |
| ---------------- | ------------------------------------- | ---------------------------------------------------- |
| 应用入口         | `src/main.cpp`                        | 命令行参数、主题初始值、截图模式                     |
| 主窗口           | `src/MainWindow.*`                    | 编辑器/预览 splitter、工具栏、主题切换、异步渲染编排 |
| Markdown 处理    | `src/MarkdownRenderer.*`              | 扫描 math/mermaid、生成 token、插入 SVG 对象         |
| 渲染服务         | `src/RendererService.*`               | 缓存、single-flight、队列、generation 取消           |
| Worker 进程      | `src/RendererWorker.*`                | 懒启动 qjs、NDJSON、超时、idle、内存/CPU 限制        |
| 链接处理         | `src/MarkdownLinkHandler.*`           | base URL、标题锚点、本地/远程/邮件策略               |
| 本地文件加载     | `src/LocalFileLoader.*`               | 存在性、目录、大小、二进制检测、UTF-8/UTF-16 解码    |
| 本地历史         | `src/NavigationHistory.*`             | 双向链表、最多 64 条、back/forward、最新优先列表     |
| 代码高亮         | `src/CodeHighlighter.*`               | `BlockCodeLanguage` + `QSyntaxHighlighter`           |
| SVG 对象绘制     | `src/SvgObjectHandler.*`              | `QTextObjectInterface` 直接绘制 SVG，不常驻 QImage   |
| 旧 one-shot 客户端 | `src/RendererClient.*`              | 源码保留；未参与当前构建，仅作历史参考                |
| SVG 归一化       | `src/SvgSanitizer.*`                  | CSS 变量、颜色、尺寸、深色主题、基础安全清理         |
| 主题             | `src/Theme.*`                         | 浅色/深色 QPalette                                   |
| MathJax 核心     | `renderer/src/mathjax-core.mjs`       | `liteAdaptor` + TeX + SVG                            |
| MathJax worker   | `renderer/src/mathjax-worker.mjs`     | NDJSON worker、常驻渲染 MathJax                       |
| Mermaid worker   | `renderer/src/mermaid-worker.mjs` / `mermaid-full-worker.mjs` | NDJSON worker；full 版本启用完整图集 |
| 旧 qjs 入口      | `renderer/src/mathjax-qjs.mjs` 等     | 一次性脚本，用于对照测试                             |
| JS 构建          | `renderer/build.mjs`                  | esbuild 打包为 QuickJS 可执行 bundle                 |
| SVG 检查工具     | `tools/svgcheck.cpp`                  | SVG → PNG，验证 QtSvg 兼容性                         |
| 构建脚本         | `scripts/`                            | setup/build/test/bench/package                       |
| 结果文档         | `docs/RESULTS.md`                     | 实测数据、限制、结论                                 |
| 决策记录         | `docs/ADR-0001-rendering-pipeline.md` | 为什么采用这条渲染路线                               |

## 4. 构建

### 脚本方式

```bash
scripts/setup-quickjs.sh
scripts/setup-renderer.sh
scripts/build.sh
scripts/test.sh
scripts/test-markdown.sh
scripts/test-mermaid-full.py
scripts/test-process-bound.sh
scripts/test-idle.sh
scripts/bench.sh
scripts/mem-bench.sh
```

`scripts/test.sh` 也会运行三个 C++ 单元测试：

- `build/link-resolution-test`：base URL、锚点、远程/危险 scheme、本地链接信号；
- `build/navigation-history-test`：back/forward、forward 截断、64 条上限；
- `build/local-file-loader-test`：文本/二进制/目录/缺失文件/UTF-16/大小限制。

`tests/sources/link-source.md` 可用于手动验证本地文件、锚点、远程链接和危险 scheme。

三个 C++ 测试也注册到了 CTest：

```bash
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

### 手动方式

QuickJS-ng：

```bash
git submodule update --init --recursive third_party/quickjs-ng

cmake -S third_party/quickjs-ng -B third_party/quickjs-ng/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DQJS_BUILD_LIBC=ON
cmake --build third_party/quickjs-ng/build --target qjs_exe -j"$(nproc)"
```

JS bundles：

```bash
cd renderer
npm install
npm run build
cd ..
```

Qt 应用：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
```

## 5. 使用方法

### GUI

```bash
./build/rich-text-render
```

窗口包含：

- 左侧 Markdown 编辑器；
- 右侧渲染预览；
- `Load sample`；
- `Render (Ctrl+Enter)`；
- **`Dark mode` / `Light mode` 切换按钮**；
- 右上角 `Back` / `Forward` 和本地文件历史下拉框。

链接行为：

- 相对链接以当前 Markdown 文件所在目录为 base；
- `#anchor` 在文档内跳转；没有显式 anchor 的 Markdown 标题按 GitHub 风格 slug 匹配；
- 本机非二进制、非目录文件直接在窗口内加载并重新渲染，不调用系统文本编辑器；
- 目录、二进制文件、超过 32MiB 的文件会提示并保留当前文档；
- `http/https` 和 `mailto:` 打开前必须确认；
- `javascript:`、`data:` 等 scheme 直接拒绝。

历史行为：

- 使用 `std::list` 双向链表，最多保存 64 条；
- 新导航发生在历史中间时，forward 历史被丢弃；
- `Back` / `Forward` 按浏览器语义移动；
- 下拉列表最新记录在上、最远记录在下；
- 列表只显示文件名，tooltip 显示完整路径；
- 选中历史项会直接切换文件，不新增重复记录。

> 本地历史目前只保存文件路径，不保存滚动位置、锚点或编辑器未保存内容。
> 通过链接、Back/Forward 或下拉列表切换文件时，编辑器中的未保存修改会被替换。

切换主题会重新渲染当前文档，MathJax 使用新的前景色，Mermaid 使用新的主题。

浅色模式使用显式的 `#ffffff` 背景和 `#111111` 文字，不依赖系统标准调色板；深色模式使用显式的 `#1e1e1e` 背景和 `#e6e6e6` 文字。

### 命令行

```bash
# 渲染指定 Markdown
./build/rich-text-render --file tests/sources/bench.md

# 以深色模式启动
./build/rich-text-render --dark

# 启动 1.5 秒后自动切换一次主题（用于截图/测试）
./build/rich-text-render --toggle-theme --screenshot out/toggle.png

# 无头截图（8 秒后保存）
QT_QPA_PLATFORM=offscreen ./build/rich-text-render --screenshot out/app.png

# 深色截图
QT_QPA_PLATFORM=offscreen ./build/rich-text-render --dark --screenshot out/app-dark.png
```

### 测试与基准

```bash
scripts/test.sh
scripts/test-markdown.sh
scripts/test-mermaid-full.py
scripts/test-process-bound.sh
scripts/test-idle.sh
scripts/bench.sh
scripts/mem-bench.sh tests/sources/bench400.md bench400
```

`mem-bench.sh` 会采样主进程与 qjs 进程的 PSS，并检查：

- qjs worker 数 ≤2；
- 总应用 PSS ≤128MB。

`test-idle.sh` 使用 `RICH_TEXT_RENDER_IDLE_MS=2000` 验证 worker 空闲退出。

`test-markdown.sh` 验证数学分隔符/代码块 token 数量；`test-mermaid-full.py` 验证完整 Mermaid worker 的常用图类型。

## 6. 打包与发布流水线

### 当前脚本

```bash
scripts/package-appimage.sh
```

脚本流程：

1. 用 `linuxdeploy --plugin qt` 把 Qt 依赖/插件部署到 `AppDir`；
2. 复制 `qjs` 到 `AppDir/usr/bin/qjs`；
3. 复制 `renderer/dist/*.mjs` 到 `AppDir/usr/share/rich-text-render/renderer/dist/`；
4. 复制 sample；
5. 写入 `AppRun`、desktop、icon；
6. 用 `appimagetool` 生成 `dist/rich-text-render-x86_64.AppImage`。

脚本默认使用：

```text
/tmp/opencode/linuxdeploy-x86_64.AppImage
/tmp/opencode/appimagetool-x86_64.AppImage
```

也可以通过环境变量覆盖：

```bash
LINUXDEPLOY=/path/to/linuxdeploy \
APPIMAGETOOL=/path/to/appimagetool \
scripts/package-appimage.sh
```

### 当前产物

```text
dist/rich-text-render-x86_64.AppImage
```

> 当前 `dist/` 已包含 renderer service 和 worker 脚本；改动代码后需重新执行
> `scripts/package-appimage.sh`。

验证：

```bash
QT_QPA_PLATFORM=offscreen ./dist/rich-text-render-x86_64.AppImage \
  --appimage-extract-and-run --dark --screenshot out/appimage-dark.png
```

### 体积

| 项                         |       大小 |
| -------------------------- | ---------: |
| qjs                        |      1.5MB |
| MathJax bundle             |      1.8MB |
| Mermaid bundle             |      664KB |
| AppImage                   | **35.3MB / 34MiB** |
| AppDir（未压缩）           |      约 85MB |
| 渲染链（qjs + JS bundles） |      约 7MB |

### CI 建议

正式发布前建议：

1. 固定 Qt、QuickJS、Node、linuxdeploy、appimagetool 版本；
2. 在干净的 Fedora/Ubuntu 虚拟机跑 AppImage smoke test；
3. 用真实 AI 输出语料做 Markdown/MathJax/Mermaid 回归；
4. 校验 AppImage 内 `qjs`、JS bundles、sample 的路径；
5. 把 `scripts/package-appimage.sh` 拆成 CI job，产物上传到 release；
6. 处理 `linuxdeploy` 在 Fedora `.relr.dyn` 上的 strip 报错（固定打包基础镜像或关闭 strip）。

## 7. 当前限制

### 渲染

- Qt Markdown 只覆盖常用子集；复杂嵌套列表、脚注、部分 GFM 未验证；
- 数学分隔符支持 `\(...\)`、`$...$`、`$$...$$`、`\[...\]`；
- 代码块已支持 C++/Python/JS/JSON/Bash/LaTeX 高亮；
- 当前高亮器是项目内 `CodeHighlighter`（正则近似），后续可评估 KSyntaxHighlighting；
- 代码块背景、边框、行距仍可继续打磨；
- Mermaid 不是生产版本，固定为 `13.0.0-alpha.3`；
- Mermaid full worker 已启用；常用图 24/24 smoke test 通过，但 alpha 版本的全部 33 种图仍需完整回归；
- Mermaid SVG 需要 `SvgSanitizer` 做 QtSvg 兼容处理；
- `QSvgRenderer` 不是完整浏览器 SVG，复杂滤镜/字体特性未验证；
- 未做高 DPI、字体回退、RTL 的全量测试。

### 交互

- 没有每条消息复制；
- 没有代码块复制；
- 没有跨消息任意拖选；
- 没有流式 token 增量渲染（当前是文档级重渲染）；
- 取消/队列已实现，但可见区域优先尚未实现；
- 本地历史只记录文件路径，不记录滚动位置或锚点；
- 本地文件加载没有 workspace allowlist。

### 进程与性能

- 已改为两个懒启动 qjs worker（MathJax / Mermaid），进程数上限 2；
- 30 blocks：总 PSS 约 62.2MB，冷启动约 250–320ms；
- 400 blocks：总 PSS 约 72.0MB，冷启动约 1.5–1.7s；
- 空闲 60s 自动退出（测试可缩短到 2s）；
- worker 有内存/CPU rlimit、请求超时、崩溃重启；
- 当前是 FIFO 队列，可见区域优先尚未实现；
- one-shot `RendererClient` 源码保留，但未参与当前构建。

> 早期预留的 qjs 进程组 PSS 子目标（slim 40MB / full 50MB）已不再作为硬性验收口径。
> full Mermaid 当前约 41–46MB，总应用约 62–72MB，满足使用需求。
> 后续只为“总应用 PSS ≤128MB”负责，不再为压低 qjs 单项数字牺牲完整图集。

### 缓存与数据

- 缓存是文件缓存，不是 SQLite/Blob；
- cache key 已包含渲染器版本、kind、source、options、Mermaid 主题；
- 未包含 DPI/字体/主题色（MathJax 的颜色在嵌入时处理，但未来加主题色时需要扩展）；
- 没有缓存清理/容量控制。

### 安全

- `SvgSanitizer` 只做 QtSvg 兼容和基础清理，不是完整安全清洗器；
- 没有对 MathJax/Mermaid 输入做严格的资源/复杂度限制；
- qjs 运行受信任的 bundle；未来若允许第三方 JS，需要沙箱；
- 没有对 SVG 外部资源、脚本、事件属性做完整白名单；
- 本地链接可在窗口内读取任意非二进制文件（单文件上限 32MiB），尚未做 workspace allowlist；
- 文本/二进制判断基于 NUL、MIME 和 UTF-8 有效性，是启发式检查，不是内容审计；
- 没有安全审计。

### 打包

- AppImage 只在当前 Fedora 环境验证；
- `linuxdeploy` strip 在 Fedora `.relr.dyn` 上会报错，但 AppDir 仍可打包；
- 没有在干净虚拟机/不同发行版验证；
- 没有 Windows/macOS；
- Qt LGPL 合规材料尚未系统化。

## 8. 下一步

完整待办与建议打磨顺序见 [TODO.md](TODO.md) 的 **Polish 顺序** 小节。

1. **每条消息复制 + 代码块复制**：实际使用差距最大；
2. **renderer service 收尾**：可见区域优先、崩溃恢复测试；
3. **SVG sanitizer hardening**：脚本/事件/外链白名单；
4. **SQLite/Blob 缓存**；
5. **真实 AI 输出语料 + golden 测试**；
6. **AppImage CI** 与干净环境验证；
7. 再评估是否需要跨消息选区。

## 9. 目录

```text
src/                    Qt Widgets 应用与渲染/缓存逻辑
renderer/               MathJax/Mermaid 的 QuickJS bundle
third_party/quickjs-ng  QuickJS-ng submodule
samples/                demo Markdown（sample.md）
tests/sources/          测试输入：公式 / Mermaid / 基准 Markdown
tools/svgcheck.cpp      SVG -> PNG 的 QtSvg 检查工具
docs/                   spike 结果、流程与 ADR
docs/assets/            最终截图
packaging/              desktop/icon
scripts/                构建、测试、基准、打包
dist/                   生成的 AppImage（gitignored）
```

**编写任务聚焦于**

- `src/`
- `renderer/src/`
- `tools/`
- `scripts/`

## 10. 第三方与许可证

见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

主要依赖：

- Qt 6 — LGPLv3 / commercial；
- QuickJS-ng — MIT；
- MathJax 3.2.2 — Apache-2.0；
- Mermaid 13.0.0-alpha.3 — MIT；
- esbuild — MIT（仅构建时）。
