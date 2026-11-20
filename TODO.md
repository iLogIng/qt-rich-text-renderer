# TODO

> 项目：rich-text-render spike
>
> 记录日期：2026-11-18
>
> 原则：不引入 QtWebEngine，不引入 MarkQml/QML 渲染链路，不引入 Node 运行时；渲染链路保持 QuickJS + SVG。

## Polish 顺序

> 以下是“从 spike 到可日常使用”的建议顺序，汇总自下面的 P0/P1/P2；不替代详细清单。
> 每完成一项，都回到真实 AI 输出语料做一次冷/热缓存与视觉回归。

| 顺序 | 打磨项 | 对应现有条目 |
|---:|---|---|
| 1 | renderer service / pool：替代 one-shot qjs，解决进程峰值 | P0-3 |
| 2 | 块级 Markdown tokenizer：尊重 code span、围栏、引用、列表、表格 | P1-9（语料回归见 P0-5） |
| 3 | XML 白名单 SVG sanitizer：脚本/事件/外链/foreignObject | P0-4 |
| 4 | SQLite/Blob 缓存：single-flight、原子写、LRU、版本失效 | P1-7 |
| 5 | 每条消息/代码块复制（代码高亮已完成） | P0-2 |
| 6 | 流式增量 + 取消 + 选区冻结 | P1-8 |
| 7 | 高 DPI / 字体 / 主题 / 响应式宽度 / 无障碍 | P1-11、P1-12、P2-14 |
| 8 | 干净 VM AppImage + CI + 许可证材料 | P0-6 |
| 9 | 真实 AI 输出语料 + golden tests | P0-5 |
| 10 | Mermaid 版本策略与图类型扩展 | P1-10 |
| 11 | 跨消息任意拖选（P2，后置） | P2-13 |

### 为什么是这个顺序

- **先 renderer service**：当时 30 blocks 会起 30 个 qjs，先解决进程峰值，后面所有性能测试才有意义。
- **再 tokenizer**：公式/图块如果被 Markdown 结构误判，后面所有渲染都不可信。
- **再 sanitizer**：Mermaid/MathJax 输出是不可信内容，安全层必须早于产品化。
- **再缓存**：缓存是“渲染是纯函数”的落点；没有版本失效的缓存比没有缓存更危险。
- **再做高亮/复制**：这是“好不好用”的分水岭，但不影响渲染正确性。
- **最后才做流式/高 DPI/跨消息选区**：这些是体验优化，应该在基础管线稳定后做。

## 渲染进程优化（已实施）

优化前 one-shot `qjs` 模型没有并发上限。当时 30 blocks 场景：

- 峰值 qjs 进程数：**34–35**（包含启动时 sample 渲染未取消的 4–5 个进程）；
- qjs 进程组 PSS 峰值：**280.8MB**；
- qjs 进程组 RSS 峰值：**409.4MB**；
- 主进程 PSS：**39.5MB**；
- 主进程 RSS：**78.2MB**。

修复 stale-render 取消 bug 后，30 blocks 峰值 qjs 进程数降到 **30**，但仍是 one-shot 模型，30 blocks 的 qjs PSS 仍约 **285–290MB**。

400 个唯一 block 外推：

- qjs PSS 约 **3.2GB**；
- qjs RSS 约 **4.7GB**；
- 若单进程更重，达到 6.6GB 也不奇怪。

这是可用性问题，不是单纯性能优化。

### 优化后实测

- 30 blocks（full Mermaid）：qjs 进程 **2**，qjs PSS **41.0MB**，主进程 PSS **21.2MB**，总 PSS **62.2MB**，冷启动约 **322ms**；
- 400 blocks（full Mermaid）：qjs 进程 **2**，qjs PSS **45.7MB**，主进程 PSS **26.3MB**，总 PSS **72.0MB**，冷启动约 **1.6s**；
- slim 3 图对比：30 blocks 总 PSS 44.8MB，400 blocks 55.7MB；
- 400 blocks 热缓存：总 PSS 约 **48.4MB**，耗时约 **236ms**；
- idle 2s 测试：qjs worker 已退出；
- 自定义 `QTextObjectInterface` 不再为每个 block 常驻 QImage，主进程 PSS 从 400+MB 降到约 26MB。

### 目标（验收口径）

| 指标 | 目标 |
|---|---|
| qjs worker 数 | **≤2**（1 MathJax + 1 Mermaid） |
| 总应用 PSS（Qt + worker） | **≤128MB** |
| qjs 渲染器进程组 PSS | 观测值，不作为独立硬门槛 |
| 400 个唯一 block 冷渲染 | 先测，再定；建议 ≤10s |
| 单 block 热渲染 | MathJax <5ms；Mermaid <20ms（待实测） |
| 空闲回收 | 60s 后退出 |
| 队列上限 | 建议 64；可见内容优先 |

> 早期 slim ≤40MB / full ≤50MB 的 qjs 子目标已撤销：那是 full 图集实现前的预算。
> 当前 full 图集约 41–46MB，30/400 blocks 总应用 62.2/72.0MB，已满足需求。
> 后续只为 **总应用 ≤128MB** 负责，不再为压低 qjs 单项数字牺牲完整图集。

### 实施结果

已实现：

- 两个懒启动 worker：MathJax / Mermaid；
- worker NDJSON 协议：
  - `stdin: {"id","source","display","theme"}`
  - `stdout: {"id","svg","error","ms"}`
- cache hit 不进入 worker；
- 每类 worker 串行处理，总并发 ≤2；
- single-flight；
- generation 取消；
- 空闲 60s 退出；
- 请求超时与 worker 重启路径；
- worker `RLIMIT_AS` / `RLIMIT_CPU`；
- 自定义 SVG 对象绘制，去掉 QImage 常驻。

尚未实现：

- 可见区域优先队列（当前 FIFO）；
- worker 失败回退 one-shot（当前直接报错）；
- worker 崩溃恢复的自动化测试。

## P0：决定“是否好用”的核心缺口

### 1. 代码块语法高亮

- [x] 识别 fenced code block 的语言标记（`QTextFormat::BlockCodeLanguage`）
- [x] 用 `QSyntaxHighlighter` 实现 C++ / Python / JSON / JavaScript / Bash / LaTeX
- [x] 代码块深/浅色主题高亮
- [ ] 代码块背景、边框、等宽字体、行距精细调整
- [ ] 高亮精确性：注释/字符串包含 `//`、`#`、引号时避免误着色
- [ ] 仅在未来需要更精确的多语言高亮时评估 KSyntaxHighlighting（KDE Frameworks 6）：Fedora 需要 `kf6-syntax-highlighting-devel` 和 `kf6-syntax-highlighting`，CMake 链接 `KF6::SyntaxHighlighting`，并适配多语言代码块状态；当前不启用
- [ ] 代码块复制入口与高亮一起设计
- [ ] 长代码块性能测试（1000 行以上）
- 验收：代码块可读性接近常见 Markdown 阅读器；切换主题不破坏高亮。

### 2. 每条消息复制 + 代码块复制

- [ ] C++ 复制服务：`copyMessage(id)` / `copyCodeBlock(id)`
- [ ] 消息级复制按钮或上下文菜单
- [ ] 代码块级复制按钮或上下文菜单
- [ ] 复制纯文本 / Markdown 源文本的语义明确
- [ ] 不把复制逻辑塞进渲染 widget
- 验收：用户不依赖鼠标拖选也能复制消息和代码。

### 3. Renderer service / pool

- [x] 把 one-shot `qjs` 改成懒启动 renderer service
- [x] 多请求复用同一个 QuickJS runtime
- [x] 空闲 30–60s 退出
- [x] 请求队列、并发上限、超时、取消
- [x] SVG 对象绘制（QTextObjectInterface，避免 QImage 常驻）
- [ ] 渲染进程崩溃后的自动恢复测试
- [ ] 可见区域优先队列
- 验收：30 blocks → 2 workers、总 PSS 62.2MB；400 blocks → 2 workers、总 PSS 72.0MB（full Mermaid）；已通过。

### 4. SVG / 输入安全加固

- [ ] `SvgSanitizer` 白名单：脚本、事件属性、外部链接、`foreignObject`
- [ ] 本地链接 workspace allowlist（当前允许窗口内打开任意本机非二进制文件，单文件上限 32MiB）
- [ ] MathJax/Mermaid 输入大小、嵌套深度、渲染时间限制
- [ ] QuickJS 内存/栈/中断限制
- [ ] 对恶意 SVG/TeX/Mermaid 的回归语料
- [ ] 明确 qjs bundle 是受信任代码，第三方 JS 不在本期范围
- 验收：安全语料不触发脚本、外部请求或无限渲染。

### 5. 真实 AI 输出语料 + 视觉回归

- [ ] 收集真实 AI 输出：长 Markdown、表格、代码块、公式、Mermaid、中文/emoji
- [ ] 建立 golden 截图或结构化断言
- [ ] 覆盖浅色/深色主题
- [ ] 覆盖 1 条、10 条、100 条消息
- 验收：每次渲染器/主题/布局改动都有回归测试。

### 6. AppImage CI 与干净环境验证

- [ ] 固定 Qt、QuickJS、Node、linuxdeploy、appimagetool 版本
- [ ] 在干净 Fedora/Ubuntu 虚拟机跑 AppImage smoke test
- [ ] 处理 `linuxdeploy` 在 Fedora `.relr.dyn` 上的 strip 报错
- [ ] 校验 AppImage 内 `qjs` / JS bundles / sample 路径
- [ ] 加入 Qt LGPL、MathJax、Mermaid 等许可证材料
- 验收：CI 产物在无开发环境的机器上可启动、可渲染、可缓存。

## P1：产品化与体验完善

### 7. SQLite / Blob 缓存

- [ ] SVG 缓存从文件迁移到 SQLite + Blob
- [ ] cache key 包含 renderer 版本、kind、source、theme、DPI、字体
- [ ] 缓存清理与容量上限
- [ ] 渲染器升级后的失效策略
- 验收：重复内容不重复渲染；升级后不会读到旧结果。

### 8. 流式增量渲染

- [ ] 只对已闭合的 math/mermaid block 触发渲染
- [ ] 当前消息的增量更新与占位符
- [ ] 停止/取消时终止渲染
- [ ] 用户选中文本时冻结重渲染
- 验收：流式输出下 UI 不卡顿，选区不丢，停止按钮有效。

### 9. Markdown 子集扩展

- [x] 块级 tokenizer：代码围栏、code span、`$...$`/`\[...\]` 数学
- [x] 占位符在代码块/行内 code 中的隔离
- [ ] blockquote / 嵌套列表中的 fence 识别
- [ ] 占位符在表格等嵌套结构中的稳定性测试
- [ ] 嵌套列表、脚注、任务列表
- [ ] 明确并记录支持的 GFM 子集
- 验收：真实 AI 输出中的常见 Markdown 不出现明显走形。

### 10. Mermaid 版本策略

- [ ] 固定 `13.0.0-alpha.3`，记录升级流程
- [ ] 只启用需要的图类型（flowchart/sequence/state）
- [ ] 建立 Mermaid 语料与回归
- [ ] 不支持/失败时显示源码 + 外部打开入口
- 验收：Mermaid 升级不会静默破坏渲染。

### 11. 数学排版细节

- [ ] 行内公式字号与正文匹配
- [ ] 块级公式居中
- [ ] 基线对齐、行距、上下标视觉检查
- [ ] 深/浅色主题下公式颜色验证
- 验收：数学公式与正文排版协调，不出现“贴边/过大/过小”。

### 12. 主题一致性

- [x] 浅色模式显式白底黑字，不依赖系统标准调色板
- [x] 深色模式显式深色 palette
- [x] Mermaid 深色/浅色主题参数
- [ ] 主题进入 cache key 的完整审计
- [ ] 高 DPI / 字体变化下的主题测试
- 验收：切换主题后编辑器、预览、数学、Mermaid 视觉一致。

## P2：后续增强

### 13. 跨消息任意拖选

- [ ] 独立 SelectionModel
- [ ] 跨 item 选择、自动滚动、选区 overlay
- [ ] 复制序列化
- 说明：成本中高，不建议在 v1 先做。

### 14. 无障碍

- [ ] 屏幕阅读器可读的 timeline
- [ ] 数学/Mermaid 的替代文本
- [ ] 键盘导航与复制

### 15. 长会话

- [ ] 消息级虚拟化
- [ ] 图片/SVG 缓存上限
- [ ] 100+ 消息的内存与滚动测试

### 16. 跨平台

- [ ] Linux 之外的打包验证
- [ ] 字体、路径、进程组差异
- [ ] AppImage 之外的发行格式

### 17. 与主项目整合

- [ ] 接入 Blob 存储
- [ ] 接入 SQLite 会话缓存
- [ ] 接入主项目的 Surface/composer
- [ ] 与 locvas 的 ADR-0005/0006 对齐

## 已完成

- [x] QuickJS-ng + MathJax 3.2.2 → SVG
- [x] QuickJS-ng + Mermaid 13.0.0-alpha.3 → SVG
- [x] QSvgRenderer → QImage → QTextDocument
- [x] Markdown 占位符 + 异步嵌入
- [x] 内容寻址文件缓存
- [x] 深色/浅色主题切换
- [x] Mermaid 深色主题
- [x] AppImage 打包与运行验证
- [x] 30 blocks 冷/热基准
- [x] 深色模式 TeX 融入背景修复
- [x] 浅色模式显式白底黑字（不依赖系统 standardPalette）
- [x] stale render 取消 bug：文件/主题切换时终止旧 qjs 进程
- [x] 30-block 进程上限回归测试（`scripts/test-process-bound.sh`）
- [x] renderer service：两个懒启动 qjs worker，max 2
- [x] NDJSON worker 协议、single-flight、generation 取消、idle 回收
- [x] 自定义 SVG 对象绘制，去掉 QImage 常驻
- [x] 128MB 总应用 PSS 目标：30 blocks 62.2MB，400 blocks 72.0MB（full Mermaid）
- [x] `scripts/mem-bench.sh`、`scripts/test-idle.sh`
- [x] 块级 Markdown tokenizer（代码围栏/行内 code/数学分隔符）
- [x] `$...$`、`\(...\)`、`$$...$$`、`\[...\]`
- [x] `QTextFormat::BlockCodeLanguage` + `QSyntaxHighlighter` 代码高亮
- [x] Mermaid full worker：常用 24 类 smoke test 通过
- [x] `scripts/test-markdown.sh`、`scripts/test-mermaid-full.py`
- [x] Markdown base URL + `MarkdownLinkHandler`（锚点/本地/远程/拒绝策略）
- [x] 本地文件在窗口内导航；`LocalFileLoader` 拒绝目录/二进制/超大文件
- [x] `NavigationHistory` 双向链表 + back/forward + 64 条上限
- [x] 工具栏右侧历史下拉框：最新在上、显示文件名、tooltip 完整路径
- [x] 将 MarkdownRenderer 拆为 `MarkdownPreprocessor` 和 `MarkdownDocumentBuilder`
- [x] 抽出独立 `LinkResolver` 和 `SvgCache`
- [x] 建立 `rich-text-render-core`、`rich-text-render-document`、`rich-text-render-ui` 三个静态库
- [x] 删除未参与构建的 `RendererClient`
- [x] 模块测试：`link-resolver-test`、`link-handler-test`、`markdown-preprocessor-test`、`svg-cache-test`、`svg-sanitizer-test`、`document-rendering-test`、`navigation-history-test`、`local-file-loader-test`
- [x] 记录 KSyntaxHighlighting 的未来依赖和迁移边界；当前只使用 Qt `QSyntaxHighlighter`

## 验证命令

```bash
scripts/test.sh
scripts/test-markdown.sh
scripts/test-mermaid-full.py
scripts/test-process-bound.sh
scripts/test-idle.sh
scripts/bench.sh
scripts/package-appimage.sh
```

## 当前维护边界

- Markdown 功能冻结为 Qt importer 加现有预处理器支持的子集，不新增 front matter、脚注、GFM 扩展或自写 parser；
- 代码高亮继续使用 Qt `QSyntaxHighlighter` 和 `CodeHighlighter`，不在当前阶段引入 KSyntaxHighlighting；
- 维护期只处理正确性 bug、安全问题、依赖升级、Qt/发行版兼容、性能回归、测试和打包；
- 已记录但暂不实现的能力：代码块复制、跨消息选区、图片资源策略、可见区域优先队列、KSyntaxHighlighting 替换。

## 非目标 / 不再讨论

- QtWebEngine
- QML / MarkQml 渲染链路
- 自写 TeX 布局
- 自写 Mermaid 布局
- 引入 Node 运行时
- 完整 HTML 渲染
- 为“一切皆插件”继续扩大协议
