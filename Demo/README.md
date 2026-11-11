# BlackLight3D Demo 

这是一个演示 BlackLight UI XML 解析器的命令行应用程序。

## 项目结构

```
Demo/
├── src/
│   └── main.cpp           # 主程序（XML 解析和可视化）
├── UI/
│   └── Main.xml           # 演示 UI 定义文件
├── CMakeLists.txt         # CMake 构建配置
└── README.md              # 本文件
```

## 构建步骤

### 1. 配置项目

```bash
cd D:\Code\BlackLight3D
cmake --preset=x64-debug
```

### 2. 编译 Demo

```bash
cmake --build build/x64/debug --target blacklight_demo --config Debug
```

或使用完整编译：

```bash
cmake --build build/x64/debug --config Debug
```

## 运行 Demo

编译完成后，运行生成的可执行文件：

**Windows (x64 Debug)：**
```bash
cd build/x64/debug
blacklight_demo.exe
```

或带参数指定 XML 文件：
```bash
blacklight_demo.exe UI\Main.xml
```

## 输出示例

```
BlackLight3D UI Demo - XML Parser Test

Parsing XML from: UI\Main.xml
---

=== UI Tree Structure ===
[Screen] id="main_screen"
  [Panel] id="title_panel" auto_size=true width=320px
    [Text] content="
    Welcome to BlackLight3D
  "
  [Panel] id="content_panel" width=100% height=200px
    [Panel] id="left_panel" width=50% height=100%
      [Text] content="
      Left Content
    "
    [Panel] id="right_panel" width=50% height=100%
      [Text] content="
      Right Content
    "
========================

✓ Parse successful!
Screen ID: main_screen
Children count: 2
```

## 功能

Demo 程序演示了以下功能：

1. **XML 文件加载** - 从磁盘读取 XML 文件
2. **XML 解析** - 使用 BlackLight UI XML 解析器
3. **树结构显示** - 以树形格式展示 UI 元素层次
4. **属性显示** - 显示元素的 ID、布局属性等
5. **错误报告** - 显示解析错误和行号

## 修改 UI 定义

编辑 `UI\Main.xml` 文件来改变 UI 结构。支持以下元素和属性：

### 元素
- **Screen** - 根元素，代表整个屏幕
- **Panel** - 容器元素，可嵌套

### 属性
- **c:id** 或 **id** - 元素标识符（唯一）
- **auto_size** - 是否根据内容自动调整大小（true/false）
- **width** - 宽度，支持单位：px, %, dp, em（如 "320px", "50%"）
- **height** - 高度，支持单位：px, %, dp, em（如 "100%", "auto"）

### 例子

```xml
<Screen xmlns:c="urn:blacklight:ui" c:id="my_screen">
  <Panel c:id="top_panel" height="80px" width="100%">
    Top Bar
  </Panel>
  <Panel c:id="main_panel" height="100%" width="100%" auto_size="true">
    Main Content
  </Panel>
</Screen>
```

## 构建选项

### 指定架构
- `-DBLACKLIGHT_ARCH=x64` - 64 位（默认）
- `-DBLACKLIGHT_ARCH=x86` - 32 位

### 禁用依赖获取
- `-DBLACKLIGHT_FETCH_DEPENDENCIES=OFF` - 使用已安装的依赖

### 禁用测试构建
- `-DBLACKLIGHT_BUILD_TESTS=OFF` - 不编译测试

## 依赖

- **RmlUi 6.3** - UI 框架库（自动获取）
- **TinyXML2 11** - XML 解析库（自动获取）
- **C++17** - 语言标准

## 故障排除

### 问题：找不到 RmlUi 或 TinyXML2
**解决方案：** 确保 `BLACKLIGHT_FETCH_DEPENDENCIES=ON`（默认启用），或手动安装这些库。

### 问题：编译错误
**解决方案：** 
1. 清理构建目录：`rm -rf build/`
2. 重新配置：`cmake --preset=x64-debug`
3. 重新编译

### 问题：找不到 UI\Main.xml
**解决方案：** Demo 的 `POST_BUILD` 命令会自动复制 UI 文件到输出目录。如果没有，手动运行：
```bash
xcopy UI build\x64\debug\UI /E /I /Y
```

## 源代码说明

### main.cpp 结构

| 函数 | 功能 |
|------|------|
| `LoadXmlFile()` | 从文件系统读取 XML 内容 |
| `PrintUITree()` | 递归打印 UI 树结构 |
| `PrintParseResult()` | 显示解析结果或错误 |
| `main()` | 程序入口，调用解析器 |

## 相关文件

- [BlackLight UI 解析器头文件](../../BlackLight/include/rmlui/UIXmlParser.h)
- [UI 元素定义](../../BlackLight/include/rmlui/UIElement.h)
- [演示 UI 文件](UI/Main.xml)

