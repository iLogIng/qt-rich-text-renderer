# TODO

> 项目：rich-text-renderer
>
> 记录日期：2026-11-18
>
> 最近更新：2026-11-23
>
> 原则：不引入 QtWebEngine，不引入 QML/MarkQml UI 或渲染链路，不引入 Node 运行时；UI 采用 Widgets + QPalette/QSS，渲染链路保持 QuickJS + SVG。

## Polish

### QSS UI Polish

> 当前项目仍是 `rich-text-renderer`，作为 locvas 的技术选型与 spike 验证。QSS/UI 工作不改变 `QTextDocument` + QuickJS 渲染管线，不引入 QML。

- [x] 在引入 QSS 前整理 `src/` 下平铺的源文件：按现有 `core`、`document`、`ui` 边界移动到子目录，只更新 CMake 源列表和 include 路径，保持行为与测试不变
- [x] 设计主题 JSON schema、`ThemeSpec` 和 `ThemeManager`；内置 `light` / `dark` 主题必须保持当前视觉
- [x] 用 `QPalette` + QSS 模板生成主题样式，禁止把用户 JSON 直接当作任意 QSS 注入
- [x] 将 `AppSettingsData::Theme` 迁移为 `themeId`，保留 `appearance/theme=light|dark` 的旧配置映射
- [x] Settings 的 Appearance 页提供主题选择、字号/密度、主题目录入口和加载失败回退
- [x] 决定代码高亮、Source 高亮、MathJax 和 Mermaid 如何消费主题语义；首版使用主题的 `dark` 标志，代码配色单独配置留待后续
- [ ] 增加主题加载、校验、回退、设置迁移、QSS token 替换和视觉回归测试（主题/回退/QSS 测试已完成，golden 视觉回归待补）
- [x] 更新 README、FOR_LOCVAS 和 FOR_LOCVAS_IF_QML，记录 QSS 主题入口、用户主题目录、.ui 边界和当前非目标

#### XML Static UI Polish

> 触发条件：QSS 主题、主题回退和视觉检查稳定后启动。本阶段仍不引入 QML。
>
> `.ui` 只替换 `QMainWindow::centralWidget()` 的子树，不替换 `QMainWindow` 本身，也不改变菜单、状态栏和 Outline 的 `QDockWidget` 停靠行为。
>
> 主题包指 `QStandardPaths::AppDataLocation/themes/<id>/` 下的目录。内置主题可以不带 `.ui`，此时继续使用当前 C++ 中央布局。
>
> 本阶段允许用户主题目录中的静态 `.ui`，但只从主题包目录内加载；禁止外部 URL、未知控件和任意样式表注入。解析或校验失败时，回退到 C++ 默认中央布局，而不是保留上一次主题的布局。

##### A. 具名插槽与中央骨架

- [x] 在 C++ 中央内容骨架中定义稳定的插槽名和 `objectName`，至少覆盖：
  - `central.root`
  - `central.toolbar.leading`
  - `central.toolbar.trailing`
  - `central.content`
  - `central.preview`
  - `central.source`
  - `central.top`
  - `central.bottom`
  - `central.empty`
- [x] 插槽宿主使用普通 `QWidget` + 布局容器；空插槽默认隐藏，`sizePolicy` 不占位，避免留下视觉空洞。`central.preview` / `central.source` 是 `central.content` 内标签页的稳定内容宿主
- [x] C++ 通过稳定插槽名注入现有工具栏按钮、`PreviewBrowser`、`QPlainTextEdit` 和媒体/状态组件，不改变控制器、信号槽和历史逻辑
- [x] 内置主题未提供 `.ui` 时，保持当前 C++ 中央布局和视觉不变
- [x] 增加测试：默认布局、插槽存在性、空插槽零占位、注入后控件类型和现有启动/加载/导航行为不变

##### B. 主题包 `.ui` 加载

- [x] 增加 `Qt6::UiTools` 依赖和 `QUiLoader` 子类；只在 UI 层引入，不进入 core/document 库
- [x] 主题 manifest 增加布局字段和版本字段，例如 `layoutFile`、`layoutApiVersion`；布局文件必须位于主题包目录内
- [x] 使用 `QUiLoader` 只加载 `QMainWindow::centralWidget()` 的中央内容骨架，不加载或替换 `QMainWindow`
- [x] 通过具名插槽把现有控件注入加载后的骨架；缺少必需插槽时视为无效主题
- [x] 用户主题目录允许携带 `layout.ui`；内置主题可以没有 `.ui`，没有时使用 C++ 默认布局
- [x] 增加测试：加载测试 `.ui`、缺少必需插槽、非法 XML、布局文件路径越界、内置主题无 `.ui`

##### C. 校验、安全与回退

- [x] 使用 `QUiLoader::createWidget()` 白名单，只允许标准 Qt 控件和当前阶段明确提供的插槽宿主
- [x] 禁止加载未知控件类、主题目录外的资源、`http/https` 外部 URL 和任意 `styleSheet` 属性注入
- [x] 限制 `.ui` 文件大小、控件数量、嵌套深度和资源路径；所有资源解析必须限制在主题包目录内
- [x] 校验必需插槽、对象名和布局版本；任何解析、校验或注入失败都回退到 C++ 默认中央布局
- [x] 回退时清理已创建的临时控件树，不能影响当前文档、历史、设置或渲染 worker
- [x] 增加测试：未知控件、外部 URL、样式表注入、超大 XML、缺失插槽、错误版本和回退路径

##### 自定义控件接入形式

- [x] A. **标准 Qt 控件 + 插槽宿主**：当前采用。`.ui` 只能使用标准控件和应用提供的具名插槽容器；现有 Preview、Source、工具栏由 C++ 注入，不执行主题代码。
- [ ] B. **应用预注册的自定义控件**：未来可选。控件由应用编译并通过 `createWidget()` 注册，主题只能按稳定类名使用；需要独立的控件 API 和兼容性测试。
- [x] C. **主题携带的第三方编译控件**：当前不采用。主题包含 `.so` / `.dll` 并由应用动态加载，会引入二进制兼容、代码执行和安全风险，不在 `.ui` 静态主题体系内。

##### 验收

- [x] C++ 默认中央布局、内置主题和现有功能测试不回归
- [x] 用户主题 `.ui` 可替换中央排列和插槽内容，但不改变主窗口、菜单、状态栏、Outline dock 和渲染控制器
- [x] 无效 `.ui` 自动回退默认布局，并记录可诊断错误
- [x] 更新 README、FOR_LOCVAS 和主题示例，明确主题包目录、插槽名、支持的控件和 `.ui` 边界

### Defer Polish

> 原有 Polish 顺序暂后置；以下是“从验证版本到可日常使用”的建议顺序，汇总自下面的 P0/P1/P2；不替代详细清单。
> 每完成一项，都回到真实 AI 输出语料做一次冷/热缓存与视觉回归。

| 顺序 | 打磨项 | 状态 | 对应现有条目 |
|---:|---|---|---|
| 1 | renderer service / pool：替代 one-shot qjs，解决进程峰值 | 已完成 | P0-3 |
| 2 | 块级 Markdown tokenizer：尊重 code span、围栏、引用、列表、表格 | 待完成 | P1-9（语料回归见 P0-5） |
| 3 | XML 白名单 SVG sanitizer：脚本/事件/外链/foreignObject | 待完成 | P0-4 |
| 4 | SQLite/Blob 缓存：single-flight、原子写、LRU、版本失效 | 待完成 | P1-7 |
| 5 | 每条消息/代码块复制（代码高亮已完成） | 部分完成 | P0-1、P0-2 |
| 6 | 流式增量 + 取消 + 选区冻结 | 待完成 | P1-8 |
| 7 | 高 DPI / 字体 / 主题 / 响应式宽度 / 无障碍 | 部分完成 | P1-11、P1-12、P2-14 |
| 8 | 干净 VM AppImage + CI + 许可证材料 | 部分完成：CI 与 Release AppImage 已有；干净 VM 待验证 | P0-6 |
| 9 | 真实 AI 输出语料 + golden tests | 待完成 | P0-5 |
| 10 | Mermaid 版本策略与图类型扩展 | 部分完成：版本固定与 24/24 smoke 已有；升级回归待补 | P1-10 |
| 11 | 跨消息任意拖选（P2，后置） | 待完成 | P2-13 |

#### 为什么是这个顺序

- **先 renderer service**：优化前 30 blocks 会起 30 个 qjs，先解决进程峰值，后面所有性能测试才有意义。
- **再 tokenizer**：公式/图块如果被 Markdown 结构误判，后面所有渲染都不可信。
- **再 sanitizer**：Mermaid/MathJax 输出是不可信内容，安全层必须早于产品化。
- **再缓存**：缓存是“渲染是纯函数”的落点；没有版本失效的缓存比没有缓存更危险。
- **再做高亮/复制**：这是“好不好用”的分水岭，但不影响渲染正确性。
- **最后才做流式/高 DPI/跨消息选区**：这些是体验优化，应该在基础管线稳定后做。

## 渲染进程优化（已实施）

优化前 one-shot `qjs` 模型没有并发上限。30 blocks 场景：

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

- [x] GitHub Actions CI/CD：Release 构建、测试、AppImage、源码包和 tag 发布
- [x] 固定 Qt、QuickJS、Node、linuxdeploy、appimagetool 和 AppImage runtime 版本
- [x] AppImage 打包强制要求 Release 构建
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

### 15. 长文档

- [ ] 消息级虚拟化
- [ ] 图片/SVG 缓存上限
- [ ] 100+ 消息的内存与滚动测试

### 16. 跨平台

- [ ] Linux 之外的打包验证
- [ ] 字体、路径、进程组差异
- [ ] AppImage 之外的发行格式

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
- [x] 建立 `rich-text-renderer-core`、`rich-text-renderer-document`、`rich-text-renderer-ui` 三个静态库
- [x] 删除未参与构建的 `RendererClient`
- [x] Source 改为只读 tab，使用 Qt `QSyntaxHighlighter` 做 Markdown 语法高亮
- [x] `MarkdownOutline` 从 `QTextDocument` 提取标题，左侧 Outline dock 点击跳转
- [x] Outline dock 固定左侧，通过 `Outline` 按钮/设置隐藏或显示，不允许浮动或取下
- [x] Outline 跳转把目标 block 对齐到 Preview 顶部
- [x] Load 使用 Qt 文件对话框，避免 KDE KIO/KDirModel 差异，并增加路径/大小/mtime 审计
- [x] 点击 Markdown 图片、本地图片链接和渲染 SVG，在独立窗口预览
- [x] 图片/SVG 预览支持 5%–800% 缩放、Fit、1:1、Ctrl+滚轮、拖拽平移和另存为
- [x] `AppSettings` 使用只读模板默认值 + QSettings 用户覆盖
- [x] `config/settings.ini` 完整配置模板，打包进 AppImage
- [x] Settings 对话框：Appearance / View / Preview / Advanced，支持字体大小、主题、目录和 Source 可见性
- [x] 模块测试：`link-resolver-test`、`link-handler-test`、`main-window-startup-test`、`app-settings-test`、`settings-dialog-test`、`markdown-preprocessor-test`、`markdown-outline-test`、`svg-cache-test`、`svg-sanitizer-test`、`document-rendering-test`、`navigation-history-test`、`local-file-loader-test`、`media-preview-test`、`theme-test`、`ui-layout-test`
- [x] 启动默认空白，`Load` 打开文件选择器，`--file` 显式加载；应用代码不再硬编码 `sample.md` 路径
- [x] `Load` 与 `--file` 统一经过 `loadFile()`、`openLocalFile()` 和加载后收尾
- [x] 加载后强制完成整篇文档布局，修复长文档后续 block 零高度问题
- [x] 普通加载复位、有效/无效 anchor 和异步 block 完成后的定位行为
- [x] visible-window 加载、README 完整布局、anchor 和 status-only 错误回归测试
- [x] 删除 Lecture-03-04 排查用的文件 hash 审计，保留路径/大小/mtime 日志
- [x] GitHub Actions：Release 构建/测试、AppImage、源码包、SHA256SUMS 和 tag Release
- [x] 删除未使用的运行时 accessor，并合并 `AppSettings` 的模板/用户覆盖解析
- [x] 删除未使用的 TeX/Mermaid fixtures 和 slim Mermaid worker
- [x] AppImage 只打包运行时使用的 MathJax worker 与 Mermaid full worker，不包含测试用 one-shot bundle
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
- Source 是只读查看器，不进入文档编辑、保存、外部修改同步或 dirty 状态范围；
- 代码高亮继续使用 Qt `QSyntaxHighlighter` 和 `CodeHighlighter`，不在当前阶段引入 KSyntaxHighlighting；
- 配置系统使用只读 `config/settings.ini` 模板和 QSettings 用户覆盖，不新增其他配置框架；
- 维护期只处理正确性 bug、安全问题、依赖升级、Qt/发行版兼容、性能回归、测试和打包；
- 已记录但暂不实现的能力：代码块复制、跨消息选区、远程图片下载、图片预览旋转、可见区域优先队列、当前节滚动高亮、KSyntaxHighlighting 替换。

## 非目标 / 不再讨论

- QtWebEngine
- 文档编辑、保存和 dirty 状态（Source 只读）
- QML / MarkQml 渲染链路
- 自写 TeX 布局
- 自写 Mermaid 布局
- 引入 Node 运行时
- 完整 HTML 渲染
- 为“一切皆插件”继续扩大协议
