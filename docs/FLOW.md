# 程序流程图

> 对应代码：`src/app/main.cpp`、`src/ui/MainWindow.cpp`、`src/core/AppSettings.cpp`、`src/core/RendererService.cpp`、`src/core/RendererWorker.cpp`、`src/core/MarkdownPreprocessor.cpp`、`src/document/MarkdownDocumentBuilder.cpp`、`src/document/MarkdownOutline.cpp`、`src/ui/OutlinePanel.cpp`、`src/ui/SettingsDialog.cpp`、`src/ui/Theme.cpp`、`src/ui/CentralUiLayout.cpp`、`src/document/SvgObjectHandler.cpp`
>
> 本文件只放流程图；架构与模块说明见 [README.md](../README.md)、[ADR-0001](ADR-0001-rendering-pipeline.md)。

---

## 1. 启动流程

```mermaid
sequenceDiagram
    autonumber
    participant M as main()
    participant W as MainWindow
    participant A as AppSettings
    participant S as RendererService
    participant E as 事件循环

    M->>M: QApplication + org/app 名
    M->>M: 解析 --file / --dark / --screenshot / --toggle-theme
    M->>W: 构造 MainWindow(dark)
    W->>A: load()（模板默认值 + QSettings 用户覆盖）
    W->>W: 应用 UI 字号、Source 字号、主题、窗口几何
    W->>W: 创建按钮 / Outline / Preview+Source tabs / history
    W->>W: 应用 Outline/Source 可见性和 source highlighter
    W->>W: 注册 SvgObjectHandler 和 MarkdownLinkHandler
    W->>S: new RendererService(this)
    W->>S: configure(qjs, workers, cache, version, idle/timeouts)
    W->>W: connect 信号槽 + Ctrl+Return
    W->>W: updateHistoryUi()（空历史）
    M->>W: show()
    opt 有 --file
        M->>W: loadFile(path, false) → openLocalFile()
        W->>W: history.visit() + renderNow()
        W->>W: schedulePreviewRefresh()（0ms，load generation 校验）
        W->>W: finishPreviewLoad() → forcePreviewLayout()
        W->>W: 光标/滚动复位或 anchor 定位 + viewport 刷新
    end
    opt 有 --toggle-theme
        M->>W: singleShot(1500) → setDarkMode(!isDarkMode())
    end
    opt 有 --screenshot
        M->>W: singleShot(8000) → grab().save()
    end
    M->>E: app.exec()
    E-->>W: QProcess / QTimer 回调
```

### 1.1 文件加载入口

```mermaid
flowchart TD
    A["--file"] --> C["MainWindow::loadFile(path, reportErrors)"]
    B["Load 按钮"] --> D["QFileDialog"] --> C
    C --> E["openLocalFile()"]
    E --> F["renderNow()"]
    E --> G["schedulePreviewRefresh()（load generation）"]
    G --> H["finishPreviewLoad()"]
    H --> J["forcePreviewLayout()<br/>setTextWidth(viewport) + documentSize"]
    J --> I["普通加载复位 / anchor 定位"]
    I --> K["viewport()->update()"]
```

- `--file` 和 `Load` 共用同一条加载和收尾路径；`reportErrors` 只控制错误呈现方式；
- `chooseFile()` 只负责选择路径，不处理加载状态；
- `openLocalFile()` 负责读取、历史、base URL 和渲染编排；
- `renderNow()` 只负责把 Source 转换为 Preview 并调度渲染；
- `finishPreviewLoad()` 必须强制完成整篇文档布局，再执行位置收尾；仅查询 `documentSize()` 不会使部分布局失效；
- 异步 block 完成后再次执行布局校正，并在存在 pending anchor 时重新定位。

---

## 2. 一次渲染的时序

```mermaid
sequenceDiagram
    autonumber
    participant U as 触发源
    participant MW as MainWindow
    participant MP as MarkdownPreprocessor
    participant MDB as MarkdownDocumentBuilder
    participant RS as RendererService
    participant SC as SvgCache
    participant Q as qjs worker
    participant TD as QTextDocument

    U->>MW: 本地链接 / 历史切换 / Ctrl+Enter / 切主题
    MW->>RS: cancelAll()
    MW->>MW: ++m_generation
    MW->>MP: prepare(source)
    MP-->>MW: markdown + blocks[]
    MW->>TD: setMarkdown(markdown)
    Note over TD: math/mermaid 已变成文本 token<br/>代码块保留并带 BlockCodeLanguage
    MW->>TD: CodeHighlighter::rehighlight()
    MW->>MW: MarkdownOutline::fromDocument(TD) → OutlinePanel
    MW->>MW: Source tab 使用 MarkdownSourceHighlighter

    loop 每个 block
        MW->>RS: renderMath() / renderMermaid()
        RS->>SC: key(kind, source, display, theme)
        RS->>SC: read(key)
        alt 缓存命中
            SC-->>RS: SVG
            RS-->>MW: callback(fromCache=true)
        else 缓存未命中
            RS->>RS: single-flight 合并相同 key
            RS->>Q: NDJSON 请求
            Q-->>RS: NDJSON 结果
            RS->>SC: write(key, svg)
            RS-->>MW: callback(svg)
        end
        MW->>MDB: embedSvg(...)
        MDB->>TD: insertText(ObjectReplacementCharacter, SvgObjectFormat)
    end

    TD->>TD: 布局时调用 intrinsicSize()
    TD->>TD: 绘制时调用 SvgObjectHandler::drawObject()
    SvgObjectHandler->>SvgObjectHandler: QSvgRenderer::render() 可见对象
    MW->>MW: pending == 0 → 状态栏显示耗时与命中数
    MW->>TD: forcePreviewLayout()
    MW->>MW: 有 pending anchor 时复位并重新定位
    MW->>MW: viewport()->update()
```

block 为空时，`renderNow()` 在更新状态后返回；加载收尾仍通过 `schedulePreviewRefresh()` 完成完整布局和视口刷新。

---

## 3. 缓存 / worker / single-flight

```mermaid
flowchart TD
    A["RendererService::render(kind, source, options)"] --> B["SvgCache::key()"]
    B --> C{"SvgCache::read() 命中?"}

    C -- 是 --> D["读取 SVG"]
    D --> E["QTimer::singleShot(0)"]
    E --> F{"generation 匹配?"}
    F -- 否 --> X["丢弃"]
    F -- 是 --> G["callback(fromCache=true)"]

    C -- 否 --> H{"in-flight 已有相同 key?"}
    H -- 是 --> I["追加 callback"]
    H -- 否 --> J["创建 Inflight + RendererJob"]
    J --> K["交给 math/mermaid worker"]
    K --> L{"worker ready?"}
    L -- 否 --> M["懒启动 qjs + ready 握手"]
    L -- 是 --> N["写 NDJSON 请求"]
    M --> N
    N --> O["等待 worker 结果 / timeout"]
    O --> P{"generation 匹配?"}
    P -- 否 --> X
    P -- 是 --> Q{"error 为空?"}
    Q -- 是 --> R["写缓存"]
    Q -- 否 --> S["保留 error"]
    R --> T["callback(svg)"]
    S --> U["callback(error)"]
```

---

## 4. SVG 对象插入与绘制

```mermaid
flowchart TD
    A["embedSvg(document, token, svg, ...)"] --> B{"error 非空<br/>或 svg 为空?"}

    B -- 是 --> C["find(token)<br/>替换为 [render error: ...]"]
    B -- 否 --> D["sanitizeSvgForQt<br/>变量 / 颜色 / 尺寸 / 清理"]
    D --> E{"QSvgRenderer 有效?"}
    E -- 否 --> C
    E -- 是 --> F["取 defaultSize 或 viewBox<br/>按 maxWidth=860 等比缩放"]
    F --> G["构造 QTextCharFormat<br/>ObjectType = SvgObjectHandler::Type<br/>保存 SVG / width / height"]
    G --> H["find(token) → 选中 token"]
    H --> I["cursor.insertText(ObjectReplacementCharacter, format)"]
    I --> J["布局时 intrinsicSize() 返回尺寸"]
    J --> K["绘制时 drawObject() 直接 QSvgRenderer::render(rect)"]
```

> 这里不再为每个 block 常驻 QImage；400 blocks 的主进程 PSS 从 420MB 降到约 26MB。

---

## 4.1 代码块高亮

```mermaid
flowchart TD
    A["QTextDocument::setMarkdown"] --> B["每个 QTextBlock"]
    B --> C{"BlockCodeLanguage 非空?"}
    C -- 否 --> D["普通文本，不高亮"]
    C -- 是 --> E["CodeHighlighter::highlightBlock"]
    E --> F["按语言应用 QSyntaxHighlighter 格式"]
    F --> G["深/浅色主题 rehighlight"]
```

支持的语言：C++ / Python / JavaScript / JSON / Bash / LaTeX。

`latex` 和 `math` fence 仍按代码处理，不自动渲染为公式。

---

## 4.2 链接处理

```mermaid
flowchart TD
    A["anchorClicked(QUrl)"] --> B["LinkResolver::resolve"]
    B --> C{"类型"}
    C -- "#anchor" --> D["标题 slug → setTextCursor/ensureCursorVisible"]
    C -- "http/https" --> E["确认后系统浏览器"]
    C -- "relative/file" --> F["按 Markdown 文件目录解析"]
    F --> G["localFileRequested(path, fragment)"]
    G --> H["LocalFileLoader::load"]
    H --> I{"存在 / 非目录 / 非二进制 / ≤32MiB?"}
    I -- 否 --> J["提示错误，保留当前文档"]
    I -- 是 --> K["MainWindow::openLocalFile"]
    K --> L["NavigationHistory::visit"]
    K --> M["renderNow()"]
    K --> P["schedulePreviewRefresh() → finishPreviewLoad()"]
    P --> Q["普通加载复位到顶部；anchor 加载在异步 block 完成后重新定位"]
    C -- "mailto" --> N["确认后邮件客户端"]
    C -- "javascript/data/..." --> O["拒绝"]
```

`QTextDocument::setBaseUrl()` 使用当前 Markdown 文件所在目录；没有 base 的相对链接会被拒绝。
本地文件不再调用 `QDesktopServices`，只在当前窗口内重新加载。

### 4.3 本地文件历史

```mermaid
flowchart LR
    A["openLocalFile(HistoryMode::Record)"] --> B["NavigationHistory::visit"]
    B --> C["丢弃 forward 分支"]
    C --> D["push_back 新 entry"]
    D --> E["超过 64 条时 erase 最旧"]
    E --> F["updateHistoryUi()"]

    G["Back / Forward"] --> H["list iterator -- / ++"]
    H --> I["openLocalFile(HistoryMode::Restore)"]
    I --> F

    J["下拉列表选择"] --> K["setCurrentIndexNewestFirst()"]
    K --> I
```

- 使用 `std::list` 双向链表，最多 64 条；
- 最新记录在链表尾部；UI 反转后最新在上；
- 下拉框只显示文件名，tooltip 保存完整路径；
- 打开失败时回滚历史游标，不把坏记录留在当前位置。

### 4.4 目录与设置

```mermaid
flowchart TD
    A["renderNow / setMarkdown"] --> B["MarkdownOutline::fromDocument"]
    B --> C["OutlinePanel::setEntries"]
    C --> D["点击标题"]
    D --> E["切换到 Preview + scrollPreviewToBlock"]

    F["Settings 按钮"] --> G["同步 Outline/Source 可见性"]
    G --> H["SettingsDialog(current, defaults)"]
    H --> I["accept"]
    I --> J["applySettings(previous)"]
    J --> K["应用字体/主题/宽度/超时"]
    J --> L["保存 QSettings 用户覆盖"]

    M["config/settings.ini"] --> N["AppSettings::defaults()"]
    O["用户 settings.ini"] --> P["AppSettings::load()"]
    N --> P
```

- 模板只读，用户覆盖写入 `QSettings`；
- `--dark` 只覆盖当前进程；
- Source 只读，Markdown 高亮在 Source 文档上运行；
- 目录条目保存 blockNumber，不依靠生成 anchor；
- Outline dock 固定左侧，`Outline` 开关只负责隐藏/显示，不允许浮动或取下。

### 4.5 图片与 SVG 预览

```mermaid
flowchart TD
    A["Preview 点击"] --> B{"点击格式"}
    B -- "Markdown image" --> C["解析 document base URL"]
    C --> D["MediaPreviewWindow::showImageFile"]
    B -- "SvgObjectHandler object" --> E["读取 SvgProperty"]
    E --> F["MediaPreviewWindow::showSvg"]
    G["本地图片链接 / Load 图片"] --> H["openLocalFile 图片预检"]
    H --> D
```

- 图片点击优先于图片外层的链接；
- 图片和 SVG 使用独立的非模态预览窗口；
- 预览窗口使用 `QGraphicsView`，支持 Fit、1:1、5%–800% 缩放、`Ctrl`+滚轮和拖拽平移；
- `Save As` 保存原始栅格图片/SVG，栅格图片可转存其他格式，SVG 可导出 PNG；
- 远程图片不下载。

---

## 5. 取消、generation 与 idle

```mermaid
sequenceDiagram
    autonumber
    participant MW as MainWindow
    participant RS as RendererService
    participant W as RendererWorker
    participant P as qjs

    MW->>RS: cancelAll()
    RS->>RS: ++generation
    RS->>RS: 清空 in-flight
    RS->>W: cancelGeneration(gen)
    W->>W: 丢弃旧队列；当前旧请求完成但回调丢弃
    Note over W: 不 kill 当前进程，避免旧/新进程叠加
    P-->>W: 当前请求完成
    W->>W: finishCurrent() → generation 不匹配则丢弃
    W->>P: 继续处理新请求

    W->>W: 队列空且无 current
    W->>W: 启动 idle timer (60s)
    W->>P: 发送 {"cmd":"shutdown"}
    P-->>W: 退出
```

---

## 6. 进程模型

```mermaid
flowchart LR
    subgraph GUI["主进程（GUI 线程）"]
        MW["MainWindow"]
        MP["MarkdownPreprocessor"]
        MDB["MarkdownDocumentBuilder"]
        RS["RendererService"]
        SC["SvgCache"]
        OH["SvgObjectHandler"]
        TD["QTextDocument"]
    end

    subgraph WORKERS["qjs workers（懒启动，最多 2 个）"]
        Q1["qjs + mathjax-worker.mjs"]
        Q2["qjs + mermaid-full-worker.mjs"]
    end

    MW --> MP
    MW --> RS
    RS --> SC
    MW --> MDB
    MP --> MDB
    MDB --> TD
    TD --> OH
    OH -->|"可见对象"| TD
    RS -->|"NDJSON"| Q1
    RS -->|"NDJSON"| Q2
    Q1 -->|"SVG"| RS
    Q2 -->|"SVG"| RS
```

> 进程数上限 2；空闲 60s 退出；worker 有 RLIMIT_AS / RLIMIT_CPU 与请求超时。

---

## 7. UI 主题

```mermaid
flowchart TD
    A["AppSettingsData::themeId"] --> B["ThemeManager::loadTheme()"]
    B --> C{"内置 light/dark?"}
    C -- 是 --> D["Qt 资源中的 theme JSON"]
    C -- 否 --> E["AppDataLocation/themes/&lt;id&gt;/theme.json"]
    D --> F["ThemeSpec"]
    E --> F
    F --> G["QPalette"]
    F --> H{"useQss?"}
    H -- 是 --> I["QSS 模板 token 替换"]
    H -- 否 --> J["清空 QSS"]
    G --> K["QApplication"]
    I --> K
    J --> K
    F --> L{"layoutFile?"}
    L -- 否 --> M["C++ 默认中央骨架"]
    L -- 是 --> N["CentralUiBuilder::loadTheme()"]
    N --> O{"校验通过?"}
    O -- 否 --> M
    O -- 是 --> P["central.root + 具名插槽"]
    M --> Q["MainWindow::setCentralWidget()"]
    P --> Q
```

- 用户主题只提供 JSON token、QSS 开关和可选静态 `.ui`，不直接提供任意 QSS 或代码；
- 内置主题保持原有 QPalette 视觉，不启用 QSS，也不带 `.ui`；
- 用户主题默认启用 QSS，由固定模板生成；
- 主题缺失或校验失败时回退到 `light`；
- `.ui` 只替换 `QMainWindow::centralWidget()` 子树；主窗口、状态栏、Outline dock 和渲染 controller 仍由 C++ 持有；
- 必需插槽为 `central.toolbar.leading`、`central.toolbar.trailing`、`central.content`；可选插槽为 `central.top`、`central.bottom`、`central.empty`；
- `central.preview` 和 `central.source` 是 `central.content` 内部标签页的稳定内容插槽；
- `.ui` 只允许标准 Qt 控件和布局；未知控件、`<customwidgets>`、`<resources>`、`<connections>`、外部 URL 和 `styleSheet` 注入会被拒绝；
- `.ui` 缺失、解析失败、插槽缺失或校验失败时回退 C++ 默认中央布局；布局路径无效只拒绝布局，不回退主题颜色；
- 代码高亮、Source 高亮和 Mermaid 仍使用主题的 `dark` 标志；
- 当前不引入 QML。
