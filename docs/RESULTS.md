# 验证结果：QuickJS + MathJax + Mermaid → SVG → QtSvg

记录日期：2026-11-18（链接与历史更新：2026-11-20；主题与 `.ui` 更新：2026-11-23）

环境：

- Fedora Linux，x86_64；
- Qt 6.11.2（Widgets、Svg、SvgWidgets、UiTools）；
- CMake 4.3.0，GCC 16.2.1；
- QuickJS-ng 0.18.0；
- Node.js 22.23.1，npm 11.18.0；
- MathJax `mathjax-full` 3.2.2；
- Mermaid `13.0.0-alpha.3`；
- `linuxdeploy` + `appimagetool`（continuous）。

> 结论：**可行，但 Mermaid v13 alpha 是主要不确定性，生产化需要 renderer service、SVG 清洗和语料测试。**

## 1. 验证矩阵

| 项 | 结果 | 证据 |
|---|---|---|
| QuickJS-ng 运行 MathJax | ✅ | `qjs --std mathjax-qjs.mjs formula.tex display` 输出 SVG |
| QuickJS-ng 运行 Mermaid v13 alpha | ✅ | `qjs --std mermaid-qjs.mjs diagram2.mmd` 输出 SVG |
| MathJax SVG → QSvgRenderer | ✅ | `svgcheck` 输出 PNG，公式正确 |
| Mermaid SVG → QSvgRenderer | ⚠️ 需清洗 | 原始 SVG 出现黑块；归一化 CSS 变量/颜色后正常 |
| `QTextDocument` 嵌入 SVG | ✅ | `SvgObjectHandler` + `QTextObjectInterface`，绘制时直接渲染 SVG |
| 文件缓存 | ✅ | 二次渲染 30 blocks 约 30–50ms |
| AppImage | ✅ | 34.9MB，含 Qt + qjs + MathJax + Mermaid full，无 WebEngine |

## 2. 体积

| 组件 | 大小 |
|---|---:|
| `qjs` 可执行文件 | 1.5MB |
| MathJax QuickJS bundle | 1.8MB |
| Mermaid QuickJS bundle (slim + flowchart/sequence/state) | 664KB |
| Mermaid full worker bundle | 2.1MB |
| Qt 应用二进制 | 6.7MB |
| AppDir（含 Qt 依赖/插件） | 约 85MB |
| AppImage（完整） | **34.9MB / 33.3MiB** |
| qjs + 4 个 JS bundle（含测试入口） | 约 7.9MB |

对比：QtWebEngine 通常会带来 130–180MB 的额外体积。这个方案避开了它。

## 3. 渲染性能

5 blocks 样例（4 math + 1 mermaid）：

- 冷缓存：**109ms / 128ms**（两次运行）；
- 热缓存：**16ms**；`cacheHits=5`。

30 blocks 基准（20 inline math + 10 mermaid）：

- 冷缓存：**404–433ms**，`cacheHits=0`；
- 热缓存：**29–47ms**，`cacheHits=30`。

冷缓存下每个 block 的耗时：

- math：约 60–110ms（5 blocks 场景）；
- mermaid：约 65–110ms；
- 30 blocks 时由于同时启动 30 个 `qjs` 进程，单 block 涨到约 140–420ms，总耗时约 404–433ms。

这证明：

- one-shot 进程可以工作；
- 但 30 个 block 会形成进程峰值。生产版应使用 **懒启动 renderer service/pool**，空闲退出，避免进程风暴。

### 3.1 内存基线与 stale-render bug

30 blocks 冷缓存场景曾出现：

- 峰值 qjs 进程数：**34–35**；
- qjs 进程组 PSS 峰值：**280.8MB**；
- qjs 进程组 RSS 峰值：**409.4MB**；
- 主进程 PSS：39.5MB；
- 主进程 RSS：78.2MB。

多出来的 4–5 个进程来自启动时 sample 渲染未被取消：旧回调被 generation 忽略，但 qjs 进程仍在运行。

已修复：

- 旧 one-shot 客户端维护 active process 集合（该客户端后来已移除）；
- `cancelAll()` 在文件/主题/重新渲染时 kill 旧进程，并递增 generation；
- 被取消的进程不再写缓存、不再回调。

修复后回归测试 `scripts/test-process-bound.sh`：30-block 场景峰值 qjs 进程数为 **30**，测试通过。

这仍然只是 bug fix；one-shot 模型的无界并发问题在当时仍在。

### 3.2 Renderer service + 自定义 SVG 对象

后续优化已实施：

- 两个懒启动 qjs worker：MathJax / Mermaid；
- NDJSON 协议、single-flight、generation 取消、idle 60s；
- worker 端 `RLIMIT_AS` / `RLIMIT_CPU`；
- 不再把每个 block 栅格化成 QImage，而是用 `QTextObjectInterface` 在绘制时直接渲染 SVG；
- `SvgObjectHandler` 缓存少量 `QSvgRenderer`，不常驻 400 张大图。

实测（PSS）：

| 场景 | qjs 进程 | qjs PSS | 主进程 PSS | 总 PSS | 耗时 |
|---|---:|---:|---:|---:|---:|
| 30 blocks 冷（slim 3 图） | 2 | 23.7MB | 21.1MB | 44.8MB | 203ms |
| 400 blocks 冷（slim 3 图） | 2 | 29.9MB | 25.8MB | 55.7MB | 1.6s |
| 30 blocks 冷（full 图集） | 2 | 41.0MB | 21.2MB | **62.2MB** | 322ms |
| 400 blocks 冷（full 图集） | 2 | 45.7MB | 26.3MB | **72.0MB** | 1.6s |
| 400 blocks 热（full 图集） | 2 | 23.0MB | 25.4MB | **48.4MB** | 236ms |

结论：**128MB 总应用 PSS 目标已达成**，而且 400 blocks 的 qjs 进程数稳定为 2。

Full Mermaid 图集比 slim 多约 18MB qjs PSS，但总应用仍有约 56MB 余量。

> 早期曾为 qjs 设置 slim 40MB / full 50MB 的子目标，那是 full worker 完成前的预算。
> 当前 full 图集约 41–46MB，30/400 blocks 总应用 62.2/72.0MB，已达到满意程度。
> 后续保留 **总应用 PSS ≤128MB** 作为验收目标，不再严格限制 qjs 单项 PSS。

## 4. MathJax 结果

MathJax SVG 的一些性质：

- SVG 根节点 `width`/`height` 使用 `ex` 单位；`QSvgRenderer` 会按 `viewBox` 得到过大的默认尺寸，需要归一化为 px；
- 包含 `<use xlink:href="#...">`；QSvgRenderer 能正常渲染；
- 使用 `currentColor`；需要替换为具体颜色（QtSvg 能处理 currentColor，但为确保可预测，本项目替换为 `#111111`）；
- `fontCache: 'local'` 输出可被 QtSvg 正确渲染；
- 典型的二次公式 SVG 约 6KB，积分/求和约 5.7KB。

结论：**MathJax + QuickJS 是三项里最确定的部分。**

## 5. Mermaid 结果

Mermaid v13 alpha 的关键性质：

- 官方声明无 DOM、确定性输出、Node/Deno/web worker 可运行；
- `mermaid/slim` + flowchart/sequence/state 可以运行；完整 `mermaid` worker 也已验证；
- 完整 worker 的常用图类型 smoke test 24/24 通过，并增加 1 个主题变量检查，总计 25/25 通过；
- 输出 SVG 使用大量 CSS 变量（`--_p`、`--_tx`、`--_el` 等）；
- QtSvg 不解析 CSS 变量和 `color(from ...)`，原始 SVG 会出现黑块/缺失文字；
- 8 位 hex 颜色（`#e8e8e8cc`）在 QtSvg 中会变成黑色，需要转成 6 位；
- 根节点 `width="100%"` 需要归一化为 viewBox 尺寸；
- 处理后节点、边、边标签、中文、箭头均正常。

结论：**Mermaid v13 alpha 能跑通，但它是 alpha，且需要一层面向 QtSvg 的输出归一化。** 如果 Mermaid 升级，这层要跟着测。

## 6. Qt Markdown 表现

`QTextDocument::setMarkdown` 在本项目中的表现：

- 标题、段落、粗体、链接：正常；
- 表格：正常；
- 行内代码/代码块：能被识别；代码块语言由 `QTextFormat::BlockCodeLanguage` 标记，已接入 `QSyntaxHighlighter`；
- 中文、emoji：正常；
- 复杂嵌套列表/脚注：未验证；
- 需要占位符替换才能插入自定义 SVG 对象；
- Qt 的 Markdown importer 不会自动保留自定义对象；本项目用 token + `QTextObjectInterface`。

实现细节：

- 先扫描 Markdown，把 `$$...$$` 和 ` ```mermaid ` 替换为唯一 token；
- `QTextDocument::setMarkdown()` 渲染剩余 Markdown；
- 异步渲染完成后，用 `QTextDocument::find(token)` 找到 token，替换为 `QChar::ObjectReplacementCharacter` + 自定义 `QTextCharFormat`；
- 格式中保存归一化后的 SVG、宽度、高度；
- `SvgObjectHandler` 在 `drawObject()` 时用 `QSvgRenderer` 直接绘制，只绘制可见对象；
- 不再为每个 block 常驻 QImage；400 blocks 的主进程 PSS 从 420MB 降到约 26MB。

## 6.1 深色模式

测试发现原始 MathJax SVG 使用 `currentColor`，如果固定替换成黑色，在深色主题下公式会融入背景。

修复方式：

- `MarkdownDocumentBuilder::embedSvg` 接收 `QPalette::Text`；
- `SvgSanitizer` 把 `currentColor` 替换为该前景色；
- 新增 `--dark` 运行参数与工具栏 `Dark mode / Light mode` 切换；
- 浅色模式改为显式 `#ffffff` 背景 + `#111111` 文字，不依赖系统 `standardPalette()`；
- Mermaid 渲染器接收 `dark`/`default` 主题参数，主题进入 cache key；
- `SvgSanitizer` 为 Mermaid 深色主题提供深色节点、浅色文字、浅色边；
- 已验证：浅色截图 [docs/assets/app-light.png](assets/app-light.png)，深色截图 [docs/assets/app-dark.png](assets/app-dark.png)，AppImage 深色截图 [docs/assets/appimage-dark.png](assets/appimage-dark.png)。

## 6.2 本地链接

Markdown 相对链接的根目录改为 **当前 Markdown 文件所在目录**：

- `QTextDocument::setBaseUrl()` 使用文件目录；
- `QTextBrowser` 关闭自动打开，统一由 `MarkdownLinkHandler` 处理；
- `#anchor` 在文档内跳转；Qt Markdown importer 不生成标题 anchor，因此额外按 GitHub 风格 slug 匹配标题；
- 本机非二进制、非目录文件通过 `localFileRequested` 交给 `MainWindow`，在当前窗口内重新加载，**不再调用系统文本编辑器**；
- `LocalFileLoader` 校验存在性、目录、大小（32MiB）、NUL/二进制内容，并支持 UTF-8（含 BOM）、带 BOM 的 UTF-16 LE/BE 和 Latin-1；
- 无 BOM 的 UTF-16 与 UTF-32 文本不在支持范围，会按二进制拒绝；
- 远程链接、`mailto:` 仍需要确认；
- `javascript:`、`data:` 等 scheme 拒绝；
- `tests/link_resolution_test.cpp` 覆盖解析策略、本地链接信号和标题锚点滚动；
- `tests/local_file_loader_test.cpp` 覆盖文本、二进制、目录、缺失文件和 UTF-16。

## 6.3 本地文件历史

新增 `NavigationHistory`：

- `std::list` 双向链表，最多 64 条；
- `visit()` 在当前节点之后插入，自动丢弃 forward 分支；
- 超过上限时删除最旧记录；
- `back()` / `forward()` 移动链表游标；
- `entriesNewestFirst()` 供下拉框显示，最新在上、最远在下；
- 下拉框只显示文件名，tooltip 显示完整路径；
- 打开失败时回滚历史游标；
- `tests/navigation_history_test.cpp` 覆盖 back/forward、forward 截断、选择、64 条上限和清空。

## 6.4 文档视图、目录与设置

当前窗口采用左侧目录 dock 和中央 Preview / Source 标签页：

- Source 只读，使用 Qt `QSyntaxHighlighter` 做 Markdown 语法着色；
- Source 高亮识别 fenced code，避免把代码中的 `#` 误判为标题；
- `MarkdownOutline` 从 Qt Markdown 文档提取标题层级、标题文字和 block 编号；
- 标题中的数学/Mermaid token 会替换成可读文字；
- 点击目录条目不依赖 anchor，直接切换到 Preview 并滚动到 block；
- 目录 dock 固定在左侧，只能通过标签栏 `Outline` 开关或 Settings 隐藏，不能浮动或取下。

配置系统：

- `config/settings.ini` 是完整只读默认模板，随 AppImage 打包；
- 用户修改通过 QSettings 写入独立设置文件；
- Settings 对话框支持 Appearance / View / Preview / Advanced；
- 字体大小分为应用/预览字号和只读 Source 字号；
- 主题、字号、目录/Source 可见性、预览宽度和渲染超时都可以运行时应用；
- `--dark` 仅覆盖当前进程，不自动写回配置；
- 窗口几何、最近目录、主题和可见性会持久化。

相关测试：

- `markdown-outline-test`；
- `app-settings-test`；
- `settings-dialog-test`；
- `document-rendering-test` 中的 Source 高亮和 fenced code 状态；
- `main-window-startup-test` 中的设置应用和目录生成。

## 6.5 图片与 SVG 预览

`PreviewBrowser` 在普通链接处理之前识别媒体点击：

- Markdown 内嵌图片：读取 `QTextImageFormat`，按文档 base URL 解析；
- 已渲染 MathJax/Mermaid：读取 `SvgObjectHandler::SvgProperty`；
- 本地图片链接或 Load 图片：在文本加载前识别图片并打开预览；
- 图片外层链接不会阻止图片预览。

`MediaPreviewWindow` 使用独立非模态窗口：

- 本地图片通过 `QImageReader` 解码，原始 `QImage` 保留给缩放和另存；
- SVG 通过 `QGraphicsSvgItem` + `QSvgRenderer` 矢量渲染；
- 预览窗口支持 5%–800% 缩放、Fit、1:1、`Ctrl`+滚轮和拖拽平移；
- `Save As` 对同格式栅格图片直接复制原文件，其他格式用 `QImageWriter` 重编码；SVG 可保存原始 SVG 或导出 PNG；
- 远程图片不下载，只提示仅支持本地图片。

图片点击、SVG 点击、图片链接预览、缩放控制和另存路径均已验证可用。

## 6.6 Load 与 --file 加载、布局与审计

`Load` 和 `--file` 统一经过 `MainWindow::loadFile()`，再调用 `openLocalFile()`；加载完成后由 `schedulePreviewRefresh()` 在 0ms 收尾中执行 `finishPreviewLoad()`。普通加载复位到文档顶部，anchor 加载在异步 block 完成后再次校正位置。

长文档的 `QTextDocument` 布局可能是惰性的。仅查询 `documentLayout()->documentSize()` 会返回部分布局，导致后续 block 高度为 0。`finishPreviewLoad()` 必须先执行 `forcePreviewLayout()`：

```text
document()->setTextWidth(viewport()->width());
documentLayout()->documentSize();
```

该操作使文档重新布局后再进行滚动复位、anchor 定位和 viewport 刷新。README 加载后的文档高度从约 3159px 恢复为 5407px，最后一个标题“第三方与许可证”的布局高度从 0 恢复为 29px。

加载诊断使用 `RICH_TEXT_RENDER_DEBUG_LOAD=1` 输出：

```text
[load] path=... size=... mtime=...
```

`Load` 使用 Qt 自带文件对话框（`DontUseNativeDialog`），避免 KDE 原生文件选择器的 KIO/KDirModel 路径和模型差异。`Load` 与 `--file` 两条路径得到相同的路径、文件大小和 mtime。

加载后 Preview 会切回 Preview tab，完成整篇文档布局，再回到顶部并刷新视口。Outline 跳转也会把目标标题 block 对齐到视口顶部，而不是只做最小可见滚动。

## 6.7 主题与静态 `.ui`

- 内置 `light` / `dark` 使用 QPalette，不启用 QSS，不提供 `.ui`，默认中央布局保持原视觉；
- 深色主题补齐 Active/Inactive/Disabled 组以及 `Light`、`Midlight`、`Dark`、`Mid`、`Shadow` 等 bevel 角色，避免 Outline 切换按钮和边框残留浅色调色板；Outline dock、面板和树的 palette 在主题切换时显式同步，避免继承链中断导致局部浅色；
- 用户主题支持 JSON 颜色、字体族、布局间距、Preview 文档边距与阅读列、语法高亮色板、Mermaid 主题、PNG/JPEG 资源、固定插槽样式、QSS 模板 token 和可选 `layout.ui`；
- 字号仍由 Settings 唯一控制；主题和 `.ui` 不能覆盖字号，`.ui` 的 `font/pointsize` 会被拒绝；
- Preview 支持 `maxTextWidth` 居中阅读列、代码块背景和代码块边距；`maxTextWidth=0` 保持 viewport 宽度；
- Mermaid 支持 `default` / `dark` / `forest` / `neutral` / `base` 主题和 8 个颜色变量；主题签名进入缓存 key，SVG sanitizer 使用相同颜色变量；
- 主题图片只支持 PNG/JPEG（≤2MiB、≤4096×4096），通过 `themeAsset` 逻辑 ID 注入，居中且不拉伸；
- `.ui` 支持 `contentMode: tabs|slots`；`slots` 模式使用 `central.preview` / `central.source`，可选 `central.outline`；
- `layouts` 支持手动布局变体，Settings 保存 `appearance/layoutId`；自动按宽度切换不实施；
- `.ui` 只替换 `QMainWindow::centralWidget()` 子树，Outline dock、标签页、工具栏、历史、设置和渲染 controller 保持 C++ 管理；
- 具名插槽通过 `centralSlot` 动态属性生成固定 QSS 规则，支持背景、边框、圆角和 padding；
- `samples/themes/aurora-dark` 同时验证 QSS、插槽样式和静态 `.ui` 布局；
- `ui-layout-test` 覆盖有效布局、缺失插槽、未知控件、外部 URL、样式表注入、超大 XML、路径越界、错误版本、字号属性、tabs/slots 切换、Source 显隐、布局变体和失败回退；
- `theme-test` 覆盖 Preview、Mermaid、图片资源和布局变体 schema；`svg-sanitizer-test` 和 `svg-cache-test` 覆盖 Mermaid 变量映射与缓存签名；
- 主题切换到内置主题时会重建默认中央布局，文档、历史、设置和 Outline 不受影响；
- 内置 light/dark 与阶段改动前截图对比，仅状态栏计时文本和极少量抗锯齿像素不同；
- 当前 CTest 共 15 个目标，全部通过；Mermaid smoke 24 个图类型 + 1 个主题变量检查全部通过。

## 7. 缓存与“渲染是纯函数”

实际使用的内容寻址缓存：

```text
key = SHA256(
  renderer-version |
  kind (math/mermaid) |
  source |
  display/options
)
```

缓存文件：`~/.cache/rich-text-renderer/rich-text-renderer/.../<key>.svg`

实际效果：

- 冷 30 blocks：约 404–433ms；
- 热 30 blocks：约 29–47ms。

“渲染是纯函数”这个判断基本成立，但需要补充：

- cache key 必须包含渲染器版本、配置、主题、DPI/字体；
- Mermaid v12 的 SVG 可能含随机 ID；v13 alpha 宣称确定性，但仍按输入+版本缓存，而不是只对输出 SVG 做 hash；
- MathJax 的输出依赖 MathJax 版本、packages、fontCache 配置；
- 如果主题/颜色变化，应重新渲染或做颜色变换，不能无条件复用；
- 缓存必须可失效，否则升级渲染器后会读到旧 SVG。

## 8. AppImage

> 当前 AppImage 已包含 renderer service、worker 脚本和 `config/settings.ini` 默认模板；代码改动后重新执行 `scripts/package-appimage.sh`。打包只接受 Release 构建。

构建方式：

1. 使用 `BUILD_TYPE=Release scripts/build.sh` 生成 Release 二进制；`package-appimage.sh` 校验 `CMAKE_BUILD_TYPE=Release`；
2. `linuxdeploy` 部署 Qt 依赖到 `AppDir`；
3. 复制 `qjs` 到 `usr/bin`，只复制运行时使用的 MathJax 和 Mermaid full worker 到 `usr/share/rich-text-renderer/renderer/dist`；
4. 复制 `config/settings.ini` 默认模板；
5. `appimagetool` 生成 AppImage；CI 使用固定版本的 AppImage runtime。

结果：

- 完整 AppImage：**34.9MB / 33.3MiB**；
- AppDir（未压缩）：约 85MB；
- 运行时渲染链（qjs + MathJax worker + Mermaid full worker）：约 5.4MB；
- 不再包含 WebEngine；
- 当前 AppImage 只在已安装 Qt 的 Fedora 主机上验证；Qt 平台/图像格式插件、`qt.conf` 与许可证材料尚未确认，自包含性未验证。

运行验证：

```bash
QT_QPA_PLATFORM=offscreen ./dist/rich-text-renderer-x86_64.AppImage \
  --appimage-extract-and-run --file samples/sample.md \
  --screenshot out/appimage.png
```

输出正常。

已知打包问题：

- `linuxdeploy` 的 strip 步骤在 Fedora 的 `.relr.dyn` section 上报错；
- 但 AppDir 已填充，`appimagetool` 仍可打包；
- CI 固定 linuxdeploy、appimagetool 和 AppImage runtime 版本；仍需固定打包基础镜像，避免宿主 strip 差异。

## 9. 未完成/未验证

- 跨消息任意拖选：未实现；
- 每条消息复制/代码块复制：未实现；
- 本地历史只保存文件路径，不保存滚动位置、锚点或未保存编辑；
- Source 按设计保持只读，不提供文件编辑和保存；
- 目录尚未根据预览滚动位置同步高亮当前节；
- 本地链接没有 workspace allowlist；单文件上限 32MiB；
- SQLite/Blob 缓存：当前是文件缓存；
- renderer service 的可见区域优先队列：未实现；
- renderer worker 崩溃恢复的自动化测试：未完成；
- QtSvg 全量语料：只测了公式、流程图、表格、中文/emoji；未测 RTL、复杂表格、SVG 滤镜、字体回退；
- Mermaid full 图集：常用 24 类 smoke test 通过；剩余低频/alpha 图类型仍需完整语料；
- Mermaid v13 alpha 的升级稳定性：未验证；
- QuickJS 安全边界：qjs 运行受信任的 bundle；SVG sanitizer 不是完整安全清洗器；
- AppImage 在干净虚拟机/无 Qt 环境下的验证：只在当前 Fedora 上通过 `--appimage-extract-and-run` 运行；
- Windows/macOS：未测。

## 10. 建议

1. **保留“渲染是纯函数 + 内容寻址缓存”这个架构**；
2. renderer service 已实现；下一步补可见区域优先队列与崩溃恢复自动化测试；
3. 对 Mermaid v13 alpha 做版本锁定和升级回归；
4. 把 SVG sanitizer 做成独立、可测试的组件，覆盖 MathJax `<use>`、Mermaid CSS 变量、外部资源、脚本/事件属性；
5. 用真实 AI 输出语料做 Markdown/公式/图表的视觉回归；
6. 在扩大使用范围之前，先实现复制、Blob 缓存和 AppImage CI；
7. 如果 Mermaid alpha 不稳定，降级为“v1 只支持 flowchart/sequence，Mermaid 不支持时显示源码 + 外部打开”。
