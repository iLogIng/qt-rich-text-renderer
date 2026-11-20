# 程序流程图

> 对应代码：`src/main.cpp`、`src/MainWindow.cpp`、`src/RendererService.cpp`、`src/RendererWorker.cpp`、`src/MarkdownPreprocessor.cpp`、`src/MarkdownDocumentBuilder.cpp`、`src/SvgObjectHandler.cpp`
>
> 本文件只放流程图；架构与模块说明见 [README.md](../README.md)、[ADR-0001](ADR-0001-rendering-pipeline.md)。

---

## 1. 启动流程

```mermaid
sequenceDiagram
    autonumber
    participant M as main()
    participant W as MainWindow
    participant S as RendererService
    participant E as 事件循环

    M->>M: QApplication + org/app 名
    M->>M: 解析 --file / --dark / --screenshot / --toggle-theme
    M->>W: 构造 MainWindow(dark)
    W->>W: applyTheme() + 注册 SvgObjectHandler
    W->>W: 创建按钮 / 编辑器 / 预览 / splitter
    W->>W: 创建 MarkdownLinkHandler、back/forward、历史下拉框
    W->>S: new RendererService(this)
    W->>S: configure(qjs, math worker, mermaid worker, cacheDir, version, idleMs)
    W->>W: connect 信号槽 + Ctrl+Return
    W->>W: updateHistoryUi()
    W->>W: loadSample() → openLocalFile() → history.visit()
    W->>S: cancelAll()
    W->>W: renderNow()
    M->>W: show()
    opt 有 --file
        M->>W: loadFile(path) → openLocalFile()
        W->>W: history.visit() + renderNow()
    end
    opt 有 --toggle-theme
        M->>W: singleShot(1500) → toggleTheme()
    end
    opt 有 --screenshot
        M->>W: singleShot(8000) → grab().save()
    end
    M->>E: app.exec()
    E-->>W: QProcess / QTimer 回调
```

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
```

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
    K --> M["renderNow + applyPendingAnchor"]
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
