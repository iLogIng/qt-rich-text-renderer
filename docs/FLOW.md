# 程序流程图

> 对应代码：`src/main.cpp`、`src/MainWindow.cpp`、`src/RendererClient.cpp`、`src/MarkdownRenderer.cpp`
>
> 本文件只放流程图；架构与模块说明见 [README.md](../README.md)、[ADR-0001](ADR-0001-rendering-pipeline.md)。

---

## 1. 启动流程

```mermaid
sequenceDiagram
    autonumber
    participant M as main()
    participant W as MainWindow
    participant R as RendererClient
    participant E as 事件循环

    M->>M: QApplication + org/app 名
    M->>M: 解析 --file / --dark / --screenshot / --toggle-theme
    M->>W: 构造 MainWindow(dark)
    W->>W: applyTheme()
    W->>W: 创建按钮 / 编辑器 / 预览 / splitter
    W->>R: new RendererClient(this)
    W->>R: configure(qjs, mathjax, mermaid, cacheDir, version)
    W->>W: connect 信号槽 + Ctrl+Return
    W->>W: loadSample() → loadFile() → renderNow()
    Note over W: 请求已派出，但 exec() 尚未开始<br/>回调还未投递
    M->>W: show()
    opt 有 --file
        M->>W: loadFile(path)
        W->>R: cancelAll()
        Note over W: 取消 sample 启动的渲染
        W->>W: renderNow()
    end
    opt 有 --toggle-theme
        M->>W: singleShot(1500) → toggleTheme()
    end
    opt 有 --screenshot
        M->>W: singleShot(8000) → grab().save()
    end
    M->>E: app.exec()
    E-->>W: 开始投递 QProcess::finished 等回调
```

对应：`main.cpp:10-50`、`MainWindow.cpp:52-115`、`MainWindow.cpp:117-127`。

---

## 2. 一次渲染的时序

```mermaid
sequenceDiagram
    autonumber
    participant U as 触发源
    participant MW as MainWindow
    participant MR as MarkdownRenderer
    participant RC as RendererClient
    participant FS as 文件缓存
    participant Q as qjs 子进程
    participant TD as QTextDocument

    U->>MW: loadFile / Ctrl+Enter / 切主题
    MW->>RC: cancelAll()
    MW->>MW: ++m_generation
    MW->>MR: prepare(source)
    MR-->>MW: markdown + blocks[]
    MW->>TD: setMarkdown(markdown)
    Note over TD: math/mermaid 已变成文本 token

    loop 每个 block
        MW->>RC: renderMath() / renderMermaid()
        RC->>RC: key = SHA256(kind, version, source, options)
        RC->>FS: 查 <key>.svg
        alt 缓存命中
            FS-->>RC: SVG
            RC-->>MW: callback(fromCache=true)
        else 缓存未命中
            RC->>Q: QProcess::start(qjs ...)
            Q-->>RC: stdout SVG 或 stderr 错误
            RC->>FS: 未取消时写 <key>.svg
            RC-->>MW: callback(svg)
        end
        MW->>MR: embedSvg(document, token, svg, ...)
        MR->>TD: addResource() + insertImage()
        MW->>MW: --m_pending
    end

    MW->>MW: pending == 0 → 状态栏显示耗时与命中数
```

对应：`MainWindow.cpp:171-247`、`RendererClient.cpp:76-194`、`MarkdownRenderer.cpp:145-219`。

---

## 3. 单个 block 的缓存 / 进程决策

```mermaid
flowchart TD
    A["render(kind, source, script, options)"] --> B{"缓存文件存在?"}

    B -- 是 --> C["读取 SVG"]
    C --> D["QTimer::singleShot(0)"]
    D --> E{"generation 匹配?"}
    E -- 否 --> X["丢弃"]
    E -- 是 --> F["callback(fromCache=true)"]

    B -- 否 --> G["写临时文件 .tex / .mmd"]
    G --> H["QProcess::start(qjs)"]
    H --> I{"15s 内退出?"}
    I -- 否 --> J["kill()"]
    I -- 是 --> K["finished / errorOccurred"]
    J --> K

    K --> L{"已取消?<br/>property 或 generation"}
    L -- 是 --> X
    L -- 否 --> M{"error 为空且 svg 非空?"}
    M -- 是 --> N["写缓存"]
    M -- 否 --> O["保留 error"]
    N --> P["callback(svg)"]
    O --> Q["callback(error)"]
```

对应：`RendererClient.cpp:82-194`。

---

## 4. SVG 回填与 token 替换

```mermaid
flowchart TD
    A["embedSvg(document, token, svg, error)"] --> B{"error 非空<br/>或 svg 为空?"}

    B -- 是 --> C["find(token)<br/>替换为 [render error: ...]"]
    B -- 否 --> D["sanitizeSvgForQt<br/>变量 / 颜色 / 尺寸 / 清理"]

    D --> E{"QSvgRenderer 有效?"}
    E -- 否 --> C
    E -- 是 --> F["取 defaultSize 或 viewBox<br/>按 maxWidth=860 等比缩放"]
    F --> G["QImage ×2 + QPainter 渲染<br/>setDevicePixelRatio(2.0)"]
    G --> H["document->addResource<br/>locvas-render://token"]
    H --> I{"find(token)?"}
    I -- 否 --> J["返回 false，仅记录日志"]
    I -- 是 --> K["选中 token → insertImage"]
```

对应：`MarkdownRenderer.cpp:145-219`、`SvgSanitizer.cpp:207-246`。

---

## 5. 取消与 generation

```mermaid
sequenceDiagram
    autonumber
    participant MW as MainWindow
    participant RC as RendererClient
    participant P as qjs 进程

    MW->>RC: cancelAll()
    RC->>RC: ++generation
    loop 每个 active 进程
        RC->>P: setProperty("locvasCancelled", true)
        RC->>P: kill()
    end
    MW->>MW: ++m_generation
    P-->>RC: finished（被杀 / 崩溃）
    RC->>RC: finish(): 检测已取消
    Note over RC: 不写缓存、不回调
    RC->>RC: deleteLater() 进程与临时文件
```

对应：`RendererClient.cpp:35-45`、`RendererClient.cpp:132-153`、`MainWindow.cpp:173-175`。

---

## 6. 进程模型

```mermaid
flowchart LR
    subgraph GUI["主进程（GUI 线程）"]
        MW["MainWindow"]
        RC["RendererClient"]
        MR["MarkdownRenderer"]
        TD["QTextDocument"]
    end

    subgraph CHILD["one-shot 子进程"]
        Q1["qjs + mathjax-qjs.mjs"]
        Q2["qjs + mermaid-qjs.mjs"]
        QN["qjs ..."]
    end

    MW --> MR
    MW --> RC
    MR --> TD
    RC -->|QProcess| Q1
    RC -->|QProcess| Q2
    RC -->|QProcess| QN

    Q1 -->|stdout SVG| RC
    Q2 -->|stdout SVG| RC
    QN -->|stdout SVG| RC
```

对应：`RendererClient.cpp:119-193`。

> 当前模型没有并发上限：N 个 block 对应 N 个进程。优化方向见 [TODO.md](../TODO.md) 的“当前优化决策”。
