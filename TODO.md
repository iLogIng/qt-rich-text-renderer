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
| 5 | 代码块语法高亮 + 每条消息/代码块复制 | P0-1、P0-2 |
| 6 | 流式增量 + 取消 + 选区冻结 | P1-8 |
| 7 | 高 DPI / 字体 / 主题 / 响应式宽度 / 无障碍 | P1-11、P1-12、P2-14 |
| 8 | 干净 VM AppImage + CI + 许可证材料 | P0-6 |
| 9 | 真实 AI 输出语料 + golden tests | P0-5 |
| 10 | Mermaid 版本策略与图类型扩展 | P1-10 |
| 11 | 跨消息任意拖选（P2，后置） | P2-13 |

### 为什么是这个顺序

- **先 renderer service**：当前 30 blocks 会起 30 个 qjs，先解决进程峰值，后面所有性能测试才有意义。
- **再 tokenizer**：公式/图块如果被 Markdown 结构误判，后面所有渲染都不可信。
- **再 sanitizer**：Mermaid/MathJax 输出是不可信内容，安全层必须早于产品化。
- **再缓存**：缓存是“渲染是纯函数”的落点；没有版本失效的缓存比没有缓存更危险。
- **再做高亮/复制**：这是“好不好用”的分水岭，但不影响渲染正确性。
- **最后才做流式/高 DPI/跨消息选区**：这些是体验优化，应该在基础管线稳定后做。

## P0：决定“是否好用”的核心缺口

### 1. 代码块语法高亮

- [ ] 识别 fenced code block 的语言标记
- [ ] 用 `QSyntaxHighlighter` 或自有高亮器实现 C++ / Python / JSON / JavaScript / Bash
- [ ] 代码块背景、边框、等宽字体、行距与深/浅色主题一致
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

- [ ] 把 one-shot `qjs` 改成懒启动 renderer service
- [ ] 多请求复用同一个 QuickJS runtime
- [ ] 空闲 30–60s 退出
- [ ] 请求队列、并发上限、超时、取消
- [ ] 渲染进程崩溃后的恢复
- 验收：30 blocks 冷启动不产生 30 个进程峰值；首屏耗时和当前 one-shot 接近或更好。

### 4. SVG / 输入安全加固

- [ ] `SvgSanitizer` 白名单：脚本、事件属性、外部链接、`foreignObject`
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

- [ ] 用块级 tokenizer 替代逐行扫描：尊重 code span、围栏长度、引用、列表、表格
- [ ] 占位符在嵌套结构中的稳定性测试
- [ ] `\[...\]` 块级数学
- [ ] `$...$` 行内数学（需避免货币误判）
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
- [x] 浅色模式显式白底黑字
- [x] Mermaid 深色主题
- [x] AppImage 打包与运行验证
- [x] 30 blocks 冷/热基准
- [x] 深色模式 TeX 融入背景修复

## 验证命令

```bash
scripts/test.sh
scripts/bench.sh
scripts/package-appimage.sh
```

## 非目标 / 不再讨论

- QtWebEngine
- QML / MarkQml 渲染链路
- 自写 TeX 布局
- 自写 Mermaid 布局
- 引入 Node 运行时
- 完整 HTML 渲染
- 为“一切皆插件”继续扩大协议
