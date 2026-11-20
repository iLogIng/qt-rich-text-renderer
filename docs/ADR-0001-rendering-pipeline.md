# ADR-0001: 富文本渲染采用 Markdown + QuickJS SVG 管线

- Status: Accepted
- Date: 2026-11-18
- Deciders: rich-text-render spike

## Context

Qt Widgets 的 Markdown 渲染在 AI 聊天场景下不完整：没有语法高亮、没有 TeX、没有 Mermaid，复杂 GFM 也可能走形。WebEngine 可以解决渲染正确性，但会带来 130–180MB 的 Chromium 体积、启动开销，并与“不做浏览器外壳”的目标冲突。

此前评估过 QML + MarkQml，但无法同时满足 Qt Widgets 集成、选区/复制、流式更新与维护成本要求。

本 spike 验证另一条路径：

- Markdown 用 Qt 原生 importer；
- TeX 用 QuickJS + MathJax 输出 SVG；
- Mermaid 用 QuickJS + Mermaid v13 alpha 输出 SVG；
- SVG 通过自定义 `QTextObjectInterface` 在绘制时由 `QSvgRenderer` 直接绘制；
- 渲染结果按内容寻址缓存；
- 不引入 WebEngine，不引入 Node 运行时。

## Decision

1. **Markdown**
   - 使用块级 tokenizer + `QTextDocument::setMarkdown`；
   - 通过占位符机制插入数学/Mermaid 对象；
   - 支持 `\(...\)`、`$...$` 行内数学和 `$$...$$`、`\[...\]` 块级数学；
   - fenced code 全部作为代码，只有 `mermaid` 例外；
   - 使用 `QTextFormat::BlockCodeLanguage` + `QSyntaxHighlighter` 做代码高亮；
   - 代码块复制后置。

2. **TeX**
   - 使用 MathJax 3.2.2 + `liteAdaptor`，在 QuickJS-ng 中渲染为 SVG；
   - `fontCache: 'local'`；
   - 根节点 `ex` 尺寸归一化为 px；
   - `currentColor` 替换为 `QPalette::Text`，保证深色模式下可读。

3. **Mermaid**
   - 使用 Mermaid `13.0.0-alpha.3`；
   - 默认 full worker；常用 24 类 smoke test 通过；
   - QuickJS-ng 执行；
   - 允许 alpha 风险，但必须锁定精确版本；
   - 输出 SVG 经过 QtSvg 兼容归一化：CSS 变量解析、8 位 hex 颜色降级、`width="100%"` 转 viewBox。

4. **SVG → Qt**
   - 对 SVG 做 `SvgSanitizer` 归一化；
   - 将 SVG、尺寸写入自定义 `QTextCharFormat`；
   - 用 `QChar::ObjectReplacementCharacter` 替换 token；
   - `SvgObjectHandler` 在 `intrinsicSize()` 返回尺寸，在 `drawObject()` 时用 `QSvgRenderer` 直接绘制；
   - 不为每个 block 常驻 QImage，只绘制可见对象。

5. **缓存**
   - cache key = 内容 + kind + renderer 版本 + 配置 + 显示模式；
   - 缓存 SVG 文件；
   - 生产版应迁移到 SQLite/Blob；
   - 缓存失效由渲染器版本/主题/DPI 触发。

6. **进程模型**
   - 两个懒启动 qjs worker：MathJax / Mermaid；
   - NDJSON 协议、single-flight、generation 取消、空闲 60s 退出；
   - worker 有 RLIMIT_AS / RLIMIT_CPU 和请求超时；
   - 不使用 WebEngine 常驻进程。

7. **不采用**
   - QtWebEngine 局部嵌入；
   - MarkQml/QML 渲染链路；
   - KaTeX HTML/CSS（Qt 没有 CSS 引擎）；
   - 自己实现 TeX/Mermaid 布局。

## Consequences

### Positive

- 30 blocks 冷启动约 250–320ms，400 blocks 约 1.6s；
- 30 blocks 总 PSS 约 62.2MB；400 blocks 总 PSS 约 72.0MB（full Mermaid 图集）；
- qjs worker 数上限 2，空闲 60s 退出；
- 自定义 SVG 对象避免了 400 个 QImage 常驻；
- AppImage 约 35.3MB，无 WebEngine；
- 无 WebEngine、无 Node 运行时；
- MathJax SVG 在 QSvgRenderer 下工作良好；
- Mermaid v13 alpha 的 DOM-free 渲染在 QuickJS 中可运行。

### Negative

- Mermaid v13 alpha 不是生产版本，API/输出可能变化；
- 需要维护 SVG 归一化/清洗层；
- renderer service 需要继续补可见区域优先和崩溃恢复测试；
- Qt Markdown 仍有子集限制；
- 复制、Blob 缓存、跨消息选区未完成；
- QtSvg 不是完整 SVG 渲染器，需要持续测试。

### Risks

- Mermaid alpha 升级破坏输出；
- QtSvg 对复杂 SVG 特性支持不足；
- 恶意 SVG/MathJax/Mermaid 输入的安全问题；
- worker 崩溃/超时恢复的边界情况；
- 缓存 key 设计不完整导致错误复用。

## Alternatives considered

- **QtWebEngine 局部嵌入**：体积/启动成本过高，违背项目目标。
- **QML + MarkQml**：无法满足需求，已评估并放弃。
- **KaTeX + HTML/CSS**：Qt 没有 CSS 引擎，不适合。
- **自写 TeX 布局**：成本过高。
- **自写 Mermaid 渲染器**：成本过高，且 v13 alpha 已提供 DOM-free 路径。
- **只显示 TeX/Mermaid 源码**：可以作为降级方案，但达不到完整 AI 聊天体验。

## References

- `docs/RESULTS.md`
- `README.md`
- Mermaid v13 alpha: https://mermaid.js.org/intro/v13-alpha.html
- MathJax direct Node: https://docs.mathjax.org/en/v4.1/server/direct.html
- `mathjax-svg-rs`: https://lib.rs/crates/mathjax-svg-rs
