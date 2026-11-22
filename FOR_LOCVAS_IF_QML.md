# locvas UI 选型：Widgets 与 QML（当前不采用 QML）

## 当前决定

当前阶段不采用 QML 作为 locvas / `rich-text-renderer` 的 UI 实现，也不启动 QML 重构。

决定依据：

- 本地富文本渲染方案已经验证，当前重点是稳定渲染行为和文件处理；
- Preview 依赖 `QTextDocument`、`QTextObjectInterface` 和 `SvgObjectHandler`，QML 无法直接复用；
- QML 需要额外实现 `DocumentSurface`、QML 主题 API、Qt Quick 部署和第三方代码信任模型；
- 这些成本不解决当前核心问题，反而可能给已经稳定的渲染链路引入回归。

UI 配置采用 **Widgets + QPalette + QSS + 主题 token**。本文保留 QML 对比和 future spike 条件，作为未来重新评估的依据，不代表当前实施计划。

## 1. 文档定位

本文比较 locvas UI 层的两种技术路线：

- Qt Widgets + QSS；
- Qt Quick / QML。

重点不是语法差异，而是两种 UI 模型如何承载主题、第三方布局和现有文档渲染管线。

当前 `rich-text-renderer` 的实现和 [FOR_LOCVAS.md](FOR_LOCVAS.md) 中的契约以 Widgets 为准；QML 路线不启动。

## 2. 当前技术基线

当前渲染管线：

```text
Markdown
  -> MarkdownPreprocessor
  -> QTextDocument::setMarkdown
  -> RendererService
  -> QuickJS-ng MathJax / Mermaid worker
  -> SvgCache
  -> SvgSanitizer
  -> MarkdownDocumentBuilder
  -> SvgObjectHandler（QTextObjectInterface）
  -> QTextBrowser
```

关键事实：

- Markdown 排版依赖 `QTextDocument`；
- 数学和 Mermaid 通过 QuickJS-ng 子进程输出 SVG；
- SVG 通过 `QTextObjectInterface` 在文档绘制阶段嵌入；
- UI 层当前使用 Widgets，Preview 是 `QTextBrowser`，Source 是 `QPlainTextEdit`；
- 渲染核心与 UI 层已经通过 `RendererService`、`MarkdownOutline`、`LinkResolver` 等模块分离。

这意味着 UI 可以替换，但 **Preview 的文档承载方式不能简单替换**。QML 的 `Text` 或 `TextArea` 不是 `QTextBrowser` 的直接等价物。

## 3. Widgets + QSS 模型

### 3.1 设计方向

保留当前 Widgets UI：

- `QTextBrowser` 作为 Preview；
- `QPlainTextEdit` 作为只读 Source；
- `QDockWidget` / `QTreeWidget` 作为 Outline；
- `QDialog` 作为 Settings 和媒体预览；
- 新增 `ThemeManager`，把主题 token 映射为 `QPalette` 和 QSS。

主题 token 示例：

```json
{
  "schemaVersion": 1,
  "colors": {
    "window": "#ffffff",
    "base": "#ffffff",
    "text": "#111111",
    "accent": "#0b57d0",
    "link": "#0b57d0"
  },
  "typography": {
    "family": "Noto Sans CJK SC",
    "baseSize": 14
  },
  "metrics": {
    "radius": 8,
    "spacing": 8
  }
}
```

内置主题和用户主题都由 `ThemeManager` 加载，生成 QPalette 和 QSS。主题文件只允许数据，不执行代码。

### 3.2 优势

- 直接复用现有 Preview、Source、Outline、Settings 和媒体预览；
- 不需要引入 Qt Quick/QML 运行时；
- AppImage 体积和启动成本基本不变；
- 文本选择、链接、anchor、历史、IME、无障碍和桌面输入行为保持现状；
- 主题加载失败不会影响主界面结构；
- 实现和测试成本最低。

### 3.3 限制

- QSS 主要控制颜色、字体、边框、背景、内边距和简单状态；
- 无法替换整体布局；
- 复杂动画、变换、毛玻璃、非矩形控件支持有限；
- 难以支持第三方“整套 UI 皮肤”；
- 主题自由度受现有控件结构约束。

### 3.4 适用目标

- 以桌面阅读器和文档渲染为核心；
- 主要诉求是深浅色、强调色、字号、密度和视觉统一；
- 不要求用户替换窗口布局；
- 优先保证渲染正确性、稳定性和低维护成本。

## 4. QML 模型

### 4.1 设计方向

QML 负责窗口、工具栏、面板、设置、动画和主题；C++ 继续负责文档、渲染、文件和进程。

建议的模块边界：

```text
Locvas.Core（C++）
  文档模型、渲染服务、缓存、链接、历史、设置

Locvas.UI（QML module）
  AppController、DocumentModel、OutlineModel、ThemeManager

DocumentSurface（C++ QQuickItem）
  承载 QTextDocument，复用现有 Markdown/SVG 渲染管线
```

QML 外壳可以使用 Qt Quick Controls 2，并通过稳定的 QML module 访问 C++ 能力。

### 4.2 Preview 的关键实现

QML 不能直接用 `Text` 或 `TextArea` 复现当前 Preview：

- `QTextDocument` 的 Markdown importer、表格和自定义对象不保证等价；
- `QTextObjectInterface` 不应假设会被 QML 文本节点调用；
- 把整篇文档先渲染成图片会失去选择、链接、anchor 和可访问性。

需要在 QML 中提供自定义文档表面，例如：

```text
DocumentSurface : QQuickPaintedItem
  - 持有 QTextDocument
  - 复用 MarkdownPreprocessor / RendererService / SvgObjectHandler
  - documentLayout()->draw()
  - 处理内容高度、滚动、命中测试、链接和 anchor
```

这条路径可以保留现有渲染管线，但需要重新实现滚动、选择、链接、媒体点击和 Outline 联动。`QQuickPaintedItem` 使用 CPU 绘制，QML 的 GPU 优势主要体现在外壳和动画，不会自动加速 `QTextDocument`。

Source 相对容易迁移：QML `TextArea` 可以暴露 `QQuickTextDocument`，在其 `QTextDocument` 上挂接现有 `MarkdownSourceHighlighter`。

### 4.3 QML 与 QuickJS-ng 的边界

两者不是重复实现：

| 运行时 | 职责 |
|---|---|
| QML/V4 JS | UI 属性绑定、轻量交互、主题逻辑 |
| QuickJS-ng 子进程 | MathJax/Mermaid 渲染、隔离、超时和内存限制 |

正确调用链：

```text
QML UI
  -> C++ controller/model
  -> RendererService
  -> qjs subprocess
  -> SVG
  -> DocumentSurface
```

不应让 QML 直接承载 MathJax/Mermaid，也不应把 QuickJS 用作 UI 绑定引擎。QML 的 JS 运行在主进程，不适合作为不可信渲染任务的隔离边界。

### 4.4 优势

- 整体布局可由 QML 声明式描述，适合第三方主题替换；
- 动画、状态、变换和响应式布局能力强；
- 适合面向普通用户的现代化界面；
- QML module 可以形成稳定的主题/布局接口；
- 天然适合把 UI 与 C++ 核心分层。

### 4.5 成本与风险

- 需要引入 Qt Quick、Quick Controls、QML 运行时和部署配置；
- AppImage 体积、启动时间和平台插件数量增加；
- 需要实现并维护 `DocumentSurface`；
- Source、Outline、媒体预览、Settings 都需要迁移或桥接；
- 需要建立 QML 测试、截图回归和主题兼容测试；
- 外部 QML 是可执行代码，不是纯样式；
- 需要稳定的 QML API 和版本策略，否则第三方主题会随版本失效。

### 4.6 适用目标

- 面向普通用户的现代桌面应用；
- 需要完整布局替换和第三方主题生态；
- 接受为 UI 运行时、部署和 API 兼容性付出成本；
- 有专门的长期投入维护主题 SDK。

## 5. 两种模型的对比

| 维度 | Widgets + QSS | QML |
|---|---|---|
| 现有 Preview 复用 | 直接 | 需要 `DocumentSurface` |
| Source 高亮复用 | 直接 | `QQuickTextDocument` + 高亮器 |
| 主题颜色/字体/密度 | 足够 | 足够 |
| 整体布局替换 | 不支持 | 支持 |
| 动画和复杂视觉 | 有限 | 强 |
| 第三方主题生态 | 仅 token 级 | 可做布局级 |
| 运行时依赖 | 低 | 中到高 |
| AppImage 体积 | 基本不变 | 需要测量，通常增加 |
| 文本选择/链接/IME | 现有控件成熟 | 需要逐项实现或适配 |
| 测试成本 | 低到中 | 中到高 |
| 安全边界 | 数据驱动主题风险低 | 外部 QML 需要信任模型 |
| 从零开发 UI | 快但表现力有限 | 前期成本高，长期迭代快 |
| 从当前实现迁移 | 不迁移 | UI 重写，核心可复用 |

## 6. 选型中需要重点斟酌的考量点

### 6.1 用户是否真的需要“整体布局替换”

需要区分两类需求：

- **主题配置**：颜色、字体、字号、密度、强调色；
- **界面替换**：侧栏位置、工具栏结构、阅读区布局、动画和组件组织。

前者用 Widgets + QSS 可以完成；后者才需要 QML。  
如果第三方生态只停留在配色和字体，QML 的收益不足以覆盖成本。

### 6.2 Preview 是否能保持功能对等

QML 方案必须先回答：

- 表格、数学、Mermaid 是否完整显示；
- 标题 anchor 和 Outline 是否仍可用；
- 文本选择、链接点击、图片/SVG 预览是否仍可用；
- Source 高亮是否保持；
- 大文档性能是否可接受。

如果 `DocumentSurface` 不能复用现有渲染管线，QML 方案会变成两套渲染实现。

### 6.3 主题 API 是否需要长期稳定

允许第三方替换布局，等同于发布一个 UI SDK。至少需要：

- `apiVersion` 和 `schemaVersion`；
- QML module 名称和导入路径；
- 数据模型和命令接口；
- 错误回退；
- 主题包清单；
- 兼容性测试。

没有版本化 API，第三方主题只是一次性演示，无法形成生态。

### 6.4 第三方主题的信任模型

外部 QML 会执行代码，不是 CSS。  
需要明确：

- 只允许本地受信任主题；
- 公开市场是否需要签名、审核或进程隔离；
- QML 是否允许网络、文件和进程接口；
- 主题异常时如何回退；
- 主题更新与应用升级如何兼容。

如果无法接受 QML 代码在主进程运行，布局级主题生态需要更重的隔离方案，成本会显著上升。

### 6.5 Qt Quick 的部署成本

QML 需要额外部署：

- Qt Quick / QML runtime；
- Quick Controls 样式和插件；
- QML import path；
- 平台插件和图形后端；
- 可能的 QML 编译缓存。

这些都需要在 AppImage、Wayland/X11、高 DPI 和不同发行版上验证。

### 6.6 测试与可访问性

Widgets 版本已有文本选择、IME、无障碍和桌面交互基础。  
QML 自定义 `DocumentSurface` 需要重新验证：

- 键盘导航；
- 文本选择和复制；
- 输入法；
- 屏幕阅读器；
- 高 DPI；
- 链接和焦点管理；
- 大文档滚动性能。

### 6.7 团队与工期

QML 不是只学语法：

- C++/QML 属性绑定和模型设计；
- QML 模块和资源打包；
- Qt Quick Test；
- 图形后端和平台差异；
- 主题 SDK 和版本兼容。

如果 locvas 是从零开始，这些成本可以前置；如果是在现有 Widgets 产品上迁移，成本会高得多。

### 6.8 是否保留当前渲染参考实现

建议保留当前 `rich-text-renderer` 的 Widgets 实现作为：

- 渲染行为参考；
- 回归基准；
- 性能基线；
- QML `DocumentSurface` 的对照实现。

QML spike 不应直接替换生产渲染器。

## 7. 两种落地路线

### 7.1 Widgets 路线

1. 引入 `ThemeManager` 和主题 token schema；
2. 主题 token 映射到 QPalette + QSS；
3. Settings 增加主题选择、字号、密度和“打开主题目录”；
4. 用户主题目录只接受 JSON 和资源，不接受代码；
5. 保持现有 Preview 和交互不变。

适合快速提升视觉一致性，且不改变渲染架构。

### 7.2 QML 路线（当前不实施）

以下是未来重新评估时的候选路线，不属于当前计划：

1. 新建独立 `qml-spike` 目标或项目；
2. 复用 core/document 渲染库；
3. 实现 `DocumentSurface`，先完成 README/sample 的功能对等；
4. QML 外壳实现工具栏、Outline、Settings 和主题切换；
5. Source 使用 QML `TextArea` + `QQuickTextDocument` 高亮；
6. 定义版本化 `Locvas.UI` module；
7. 先支持 token 主题，再支持整体 QML 布局主题；
8. 最后决定是否迁移生产 UI。

## 8. QML spike 的验收门槛

建议至少验证：

- README、sample 和长文档完整渲染；
- 表格、数学、Mermaid、链接、anchor、Outline；
- Source 只读和 Markdown 高亮；
- 媒体预览、缩放和另存；
- 主题 token 切换和错误回退；
- 至少一个可替换的 QML 布局主题；
- 30/400 block 的性能和内存；
- AppImage 体积、启动时间和 Wayland/X11 行为；
- QML 主题 API 的版本边界。

如果这些门槛不能通过，Widgets + QSS 是更稳妥的生产选择。

## 9. 结论

当前采用 **Widgets + QPalette + QSS + 主题 token**：

- 保留已经验证的 `QTextDocument` + QuickJS 渲染管线；
- 不引入 Qt Quick/QML 运行时和 `DocumentSurface`；
- 不引入第三方 QML 代码的信任和兼容性问题；
- 主题只覆盖颜色、字体、间距、控件外观和内置布局选项；
- 不支持第三方替换整体窗口布局。

QML 路线仅在以下条件同时成立时重新评估：

- locvas 的产品目标明确需要第三方整体布局/皮肤生态；
- 当前渲染管线和本地文件交互已经稳定；
- 有可投入的 QML API、`DocumentSurface`、测试和部署预算；
- 第三方主题的信任模型已经确定。

在条件满足前，QML 只作为技术备选，不进入实施范围。

## 10. 不采用 QML 后的 UI 自由度

不采用 QML 并不意味着界面只能使用 Qt 默认外观，但需要明确边界。

仍然可以提供：

- 内置浅色、深色、高对比和护眼主题；
- 强调色、链接色、选中色、字体、字号、行距和密度；
- 按钮、工具栏、标签页、Dock、滚动条、输入框、状态栏的 QSS 样式；
- 内置布局选项，例如 Outline 显示/隐藏、Source 页签显示/隐藏；
- 主题 token 驱动的背景和图标资源。

不能提供：

- 第三方替换整个窗口布局；
- WebView/WebEngine 级别的 HTML/CSS/JS 自由定制；
- 任意动画、复杂变换和毛玻璃效果；
- 用户直接编写 QML 或 QSS 来改变控件结构；
- 与当前渲染管线无关的第三方 UI 插件。

WebView 式的自由度来自完整的 HTML/CSS/JS 运行时，代价是 WebEngine 依赖、体积、启动、安全边界和渲染一致性。

QML 可以避免 WebEngine，但仍会引入 Qt Quick 运行时、文档适配器和 QML API 兼容成本。

因此当前选择是：

```text
视觉配置足够开放
布局结构保持受控
不引入新的 UI 运行时
不为主题能力牺牲渲染稳定性
```

如果未来产品目标确实需要“第三方整体界面替换”，再以独立 spike 评估 QML；当前不作为生产路线。
