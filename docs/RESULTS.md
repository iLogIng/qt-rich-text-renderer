# Spike Results: QuickJS + MathJax + Mermaid → SVG → QtSvg

日期：2026-11-18

环境：

- Fedora Linux，x86_64；
- Qt 6.11.2（Widgets、Svg）；
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
| QuickJS-ng 运行 Mermaid v13 alpha | ✅ | `qjs --std mermaid-qjs.mjs diagram.mmd` 输出 SVG |
| MathJax SVG → QSvgRenderer | ✅ | `svgcheck` 输出 PNG，公式正确 |
| Mermaid SVG → QSvgRenderer | ⚠️ 需清洗 | 原始 SVG 出现黑块；归一化 CSS 变量/颜色后正常 |
| `QTextDocument` 嵌入 SVG | ✅ | 通过 `QSvgRenderer` 栅格化 + `QTextDocument::ImageResource` |
| 文件缓存 | ✅ | 二次渲染 30 blocks 约 29ms |
| AppImage | ✅ | 75.8MB，含 Qt + qjs + MathJax + Mermaid，无 WebEngine |

## 2. 体积

| 组件 | 大小 |
|---|---:|
| `qjs` 可执行文件 | 1.5MB |
| MathJax QuickJS bundle | 1.8MB |
| Mermaid QuickJS bundle (slim + flowchart/sequence/state) | 664KB |
| Qt 应用二进制 | 3.7MB |
| AppDir（含 Qt 依赖/插件） | 约 195MB |
| AppImage（完整） | **约 73–76MB** |
| AppImage（去掉 qjs/JS bundles） | 73.1MB |
| **渲染链增量** | **约 2–3MB（AppImage）/ 4MB（AppDir）** |

对比：QtWebEngine 通常会带来 130–180MB 的额外体积。这个方案避开了它。

## 3. 渲染性能

5 blocks 样例（4 math + 1 mermaid）：

- 冷缓存：**109ms / 128ms**（两次运行）；
- 热缓存：**16ms**；`cacheHits=5`。

30 blocks 基准（20 inline math + 10 mermaid）：

- 冷缓存：**433ms**，`cacheHits=0`；
- 热缓存：**29ms**，`cacheHits=30`。

冷缓存下每个 block 的耗时：

- math：约 60–110ms（5 blocks 场景）；
- mermaid：约 65–110ms；
- 30 blocks 时由于同时启动 30 个 `qjs` 进程，单 block 涨到约 140–420ms，总耗时 433ms。

这证明：

- one-shot 进程可以工作；
- 但 30 个 block 会形成进程峰值。生产版应使用 **懒启动 renderer service/pool**，空闲退出，避免进程风暴。

## 4. MathJax 结果

MathJax SVG 的一些性质：

- SVG 根节点 `width`/`height` 使用 `ex` 单位；`QSvgRenderer` 会按 `viewBox` 得到过大的默认尺寸，需要归一化为 px；
- 包含 `<use xlink:href="#...">`；QSvgRenderer 能正常渲染；
- 使用 `currentColor`；需要替换为具体颜色（QtSvg 能处理 currentColor，但为确保可预测，spike 中替换为 `#111111`）；
- `fontCache: 'local'` 输出可被 QtSvg 正确渲染；
- 典型的二次公式 SVG 约 6KB，积分/求和约 5.7KB。

结论：**MathJax + QuickJS 是三项里最确定的部分。**

## 5. Mermaid 结果

Mermaid v13 alpha 的关键性质：

- 官方声明无 DOM、确定性输出、Node/Deno/web worker 可运行；
- 在 QuickJS-ng 中，`mermaid/slim` + flowchart/sequence/state 可以运行；
- 输出 SVG 使用大量 CSS 变量（`--_p`、`--_tx`、`--_el` 等）；
- QtSvg 不解析 CSS 变量和 `color(from ...)`，原始 SVG 会出现黑块/缺失文字；
- 8 位 hex 颜色（`#e8e8e8cc`）在 QtSvg 中会变成黑色，需要转成 6 位；
- 根节点 `width="100%"` 需要归一化为 viewBox 尺寸；
- 处理后节点、边、边标签、中文、箭头均正常。

结论：**Mermaid v13 alpha 能跑通，但它是 alpha，且需要一层面向 QtSvg 的输出归一化。** 如果 Mermaid 升级，这层要跟着测。

## 6. Qt Markdown 表现

`QTextDocument::setMarkdown` 在本 spike 中表现：

- 标题、段落、粗体、链接：正常；
- 表格：正常；
- 行内代码/代码块：能被识别，但没有语法高亮，代码块视觉样式有限；
- 中文、emoji：正常；
- 复杂嵌套列表/脚注：未验证；
- 需要占位符替换才能插入自定义 SVG 对象；
- Qt 的 Markdown importer 不会自动保留占位符之外的自定义对象；本 spike 用 `QTextDocument::find(token)` + `insertImage`。

实现细节：

- 先扫描 Markdown，把 `$$...$$` 和 ` ```mermaid ` 替换为唯一 token；
- `QTextDocument::setMarkdown()` 渲染剩余 Markdown；
- 异步渲染完成后，用 `QTextDocument::find(token)` 找到 token，**直接使用 find 返回的选区**替换成 `QTextImageFormat`；
- 不能再次 `movePosition(..., token.size())`，否则会扩大选区并吞掉后续文本（spike 中实际踩过）。
- 资源名使用 `locvas-render://<token>`，通过 `QTextDocument::addResource(ImageResource, ...)` 注入 QImage。

## 6.1 深色模式

测试发现原始 MathJax SVG 使用 `currentColor`，如果固定替换成黑色，在深色主题下公式会融入背景。

修复方式：

- `MarkdownRenderer::embedSvg` 接收 `QPalette::Text`；
- `SvgSanitizer` 把 `currentColor` 替换为该前景色；
- 新增 `--dark` 运行参数与工具栏 `Dark mode / Light mode` 切换；
- 浅色模式改为显式 `#ffffff` 背景 + `#111111` 文字，不依赖系统 `standardPalette()`；
- Mermaid 渲染器接收 `dark`/`default` 主题参数，主题进入 cache key；
- `SvgSanitizer` 为 Mermaid 深色主题提供深色节点、浅色文字、浅色边；
- 已验证：浅色截图 `out/app-light-fixed.png`，深色切换截图 `out/app-toggle-dark.png`，AppImage 浅色/切换截图 `out/appimage-light-fixed.png`、`out/appimage-toggle-fixed.png`。

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

缓存文件：`~/.cache/locvas-spike/rich-text-render-spike/.../<key>.svg`

实际效果：

- 冷 30 blocks：433ms；
- 热 30 blocks：29ms。

“渲染是纯函数”这个判断基本成立，但需要补充：

- cache key 必须包含渲染器版本、配置、主题、DPI/字体；
- Mermaid v12 的 SVG 可能含随机 ID；v13 alpha 宣称确定性，但仍按输入+版本缓存，而不是只对输出 SVG 做 hash；
- MathJax 的输出依赖 MathJax 版本、packages、fontCache 配置；
- 如果主题/颜色变化，应重新渲染或做颜色变换，不能无条件复用；
- 缓存必须可失效，否则升级渲染器后会读到旧 SVG。

## 8. AppImage

构建方式：

1. `linuxdeploy` 部署 Qt 依赖到 `AppDir`；
2. 复制 `qjs` 到 `usr/bin`，MathJax/Mermaid bundle 到 `usr/share/rich-text-render/renderer/dist`；
3. 复制 sample；
4. `appimagetool` 生成 AppImage。

结果：

- 完整 AppImage：**约 73–76MB**（不同打包运行略有差异）；
- 去掉 qjs/JS bundles 的 base AppImage：**73.1MB**；
- 渲染链增量：约 **2–3MB**（AppImage）/ **4MB**（AppDir）。

运行验证：

```bash
QT_QPA_PLATFORM=offscreen ./rich-text-render-x86_64.AppImage \
  --appimage-extract-and-run --screenshot out/appimage.png
```

输出正常。

已知打包问题：

- `linuxdeploy` 的 strip 步骤在 Fedora 的 `.relr.dyn` section 上报错；
- 但 AppDir 已填充，`appimagetool` 仍可打包；
- 生产版应使用固定版本的 linuxdeploy/CI 环境，不要依赖宿主 strip。

## 9. 未完成/未验证

- 跨消息任意拖选：未实现；
- 每条消息复制/代码块复制：未实现；
- SQLite/Blob 缓存：当前是文件缓存；
- renderer service/pool：当前每个 block 一个 `qjs` 进程；
- QtSvg 全量语料：只测了公式、流程图、表格、中文/emoji；未测 RTL、复杂表格、SVG 滤镜、字体回退；
- Mermaid 33 种图：只测 flowchart/sequence/state 的少量样例；
- Mermaid v13 alpha 的升级稳定性：未验证；
- QuickJS 安全边界：qjs 运行受信任的 bundle；SVG sanitizer 不是完整安全清洗器；
- AppImage 在干净虚拟机/无 Qt 环境下的验证：只在当前 Fedora 上通过 `--appimage-extract-and-run` 运行；
- Windows/macOS：未测。

## 10. 建议

1. **保留“渲染是纯函数 + 内容寻址缓存”这个架构**；
2. 把 one-shot `qjs` 改成 **懒启动 renderer service**：启动后处理多个请求，空闲 30–60s 退出；
3. 对 Mermaid v13 alpha 做版本锁定和升级回归；
4. 把 SVG sanitizer 做成独立、可测试的组件，覆盖 MathJax `<use>`、Mermaid CSS 变量、外部资源、脚本/事件属性；
5. 用真实 AI 输出语料做 Markdown/公式/图表的视觉回归；
6. 在接入主项目之前，先实现复制、Blob 缓存和 AppImage CI；
7. 如果 Mermaid alpha 不稳定，降级为“v1 只支持 flowchart/sequence，Mermaid 不支持时显示源码 + 外部打开”。
