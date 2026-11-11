# UI 渲染黑屏问题记录

## 现象

`blacklight_demo.exe` 能创建窗口并清屏，但 UI 页面没有正常铺满窗口，画面只剩左上角的小块和一条异常的斜线。进程输出显示 RmlUi 已初始化、字体文件也已加载。

## 定位

1. `RmlUiTree::BuildElement()` 原先把除少数表单控件外的所有标签都创建成 `div`，因此 `html`、`body` 等页面结构标签丢失。
2. 默认样式表使用了 `html, body { height: 100%; }`，但树中不存在对应的原生标签，页面高度链断开，`height: 100%` 的布局结果塌缩。
3. `<style>` 内容原先没有从 HTML 树中提取为 RmlUi 样式表。
4. 默认样式表和用户样式表分别调用 `SetStyleSheetContainer()` 时，后一次设置会替换前一次，导致默认字体规则丢失。RmlUi 随后报告 `No font face defined`。

## 修复

- 保留原始 HTML 标签名创建 RmlUi 元素。
- `html`、`head`、`body`、`style` 作为文档结构节点展开，不创建可见重复容器。
- 提取内嵌 `<style>` 文本。
- 将默认 CSS、外部 CSS 和内嵌 CSS 合并后一次加载。
- 默认字体声明改为 RmlUi 可识别的单一字体名 `Arial`。
- 将普通 HTML 属性复制到原生 RmlUi 元素。

## 验证

使用 `D:\VisualStudio2026\MSBuild\Current\Bin\MSBuild.exe` 构建 `build\x64\debug\BlackLight3D.slnx` 成功。Demo 启动日志不再出现 `No font face defined`；剩余警告来自 RmlUi 不支持的浏览器 CSS 属性，例如 `transition`、`border` 简写和 `list-style-position`，不会阻止基础布局绘制。
