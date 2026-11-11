# Engine Core Runtime

The current application hierarchy is documented in
[Window and Game Hosts](window-game-architecture.md). Desktop applications derive
from `Window`; games derive from `Game` and implement `OnRender(GameTime)`.

本阶段把 BlackLight3D 从“窗口 + RmlUi 清屏示例”扩展为可以承载游戏逻辑的 C++17 运行时骨架。所有新增类型都位于 `BlackLight::Engine`，渲染实现仍通过 `IGraphicsBackend` 注入，SDK 头文件不依赖 OpenGL 细节。

## 模块

- `Time.h/.cpp`：`FrameClock` 提供单调时钟、最大帧间隔裁剪和固定时间步累加器。`GameTime::fixed_steps` 可用于统计一帧执行了多少次确定性更新。
- `Input.h/.cpp`：`InputState` 保存键盘、鼠标按键、鼠标位置和滚轮的当前状态及按下/释放边沿。平台后端只需要调用 `SetKeyDown`、`SetMouseButtonDown` 等方法。
- `Ecs.h/.cpp`：`World` 使用实体代际编号避免悬空句柄，组件按 `type_index` 分池。`Each<A, B>` 在一条遍历中访问满足组件集合的实体。
- `Content.h/.cpp`：`ContentManager` 以内容根目录为边界，缓存不可变二进制和 UTF-8 文本资产，适合在游戏初始化或后台加载阶段复用。
- `Logger.h/.cpp`：`Logger` 是进程级、线程安全的日志器，支持级别过滤、控制台输出、可选日志文件和自定义 sink。`BL_LOG_INFO("Category") << ...` 宏在级别被过滤时不会构造消息。
- `SpriteBatch.h/.cpp`：`SpriteBatch` 收集 2D 命令，并按纹理或深度排序后交给宿主回调。实际 GPU 纹理和顶点上传不进入 SDK ABI。
- `Game.h/.cpp`：`Game` 统一后端初始化、输入采样、可变更新、固定更新、渲染、呈现和退出生命周期。

## 典型入口

```cpp
class SampleGame final : public BlackLight::Engine::Game {
protected:
    bool OnInitialize() override {
        auto player = GetWorld().CreateEntity();
        GetWorld().Add<BlackLight::Math::Vector2>(player, BlackLight::Math::Vector2{32, 32});
        return true;
    }

    void OnFixedUpdate(const GameTime& time) override {
        GetWorld().Each<BlackLight::Math::Vector2>(
            [&](Entity, BlackLight::Math::Vector2& position) {
                position.x += static_cast<float>(time.fixed_elapsed_seconds * 60.0);
            });
    }

    /// Draws the scene before the engine-owned UI overlay.
    void OnRender(const BlackLight::Engine::GameTime&) override {}
};

int main() {
    SampleGame game;
    return game.Run();
}
```

```cpp
BlackLight::Engine::Logger::Get().SetMinimumLevel(BlackLight::Engine::LogLevel::Info);
BL_LOG_INFO("Render") << "GPU backend initialized: " << name;
BL_LOG_ERROR("Content") << "Missing asset: " << path;
```

## Windows 后端

`WindowsOpenGLBackend` 现在会：

1. 创建可重复初始化的 Win32 窗口和 OpenGL 上下文。
2. 将键盘、鼠标按键、鼠标位置和滚轮消息写入 `InputState`。
3. 在窗口尺寸变化时同步 OpenGL viewport、RmlUi context dimensions 和 drawable size。
4. 使用 RmlUi 6.3 的 `RenderInterface`/`RenderManager` 生命周期，而不是旧版本不存在的全局 `SetRenderManager` API。
5. 使用 RmlUi 自带的 FreeType 字体引擎，并在初始化后加载 Windows Arial 字体。将 `RMLUI_FONT_ENGINE` 设为 `none` 会导致 RmlUi 6.3 在没有自定义 `FontEngineInterface` 时初始化失败。

当前 Windows OpenGL 接口绘制 RmlUi 编译几何、上传生成的 RGBA 纹理（包括 FreeType 字形图集），并支持 scissor 裁剪。外部图片文件解码和高级图层/滤镜效果尚未实现；该兼容管线渲染器后续可替换为现代 OpenGL/Vulkan/DirectX 设备层，并让 `SpriteBatch` 的提交回调直接写入 GPU 顶点缓冲。

## RmlUi 字体初始化修复

错误 `[RmlUi ERROR] No font engine interface set!` 的直接原因是依赖配置关闭了 RmlUi 的内置字体引擎。构建时启用 FreeType 后，`Rml::Initialise()` 会自动创建默认字体接口；Windows 后端随后从 Windows 系统目录加载 Arial，供 UI 文本使用。UI 树构建时也为文档设置 Arial 和白色文字，避免缺少字体属性警告。重新配置现有构建目录后再构建，避免沿用缓存的 `RMLUI_FONT_ENGINE=none`。

## UI 控件与样式

XML 支持 `Panel`、`Fill`、`Border`、`Button`、`ListBox`、`ListView` 和 `DataGrid` 容器控件。控件共用 `auto_size`、`width`、`height`、`c:id` 与 `style` 属性，并可嵌套子控件。`<layout>` 可包含具备唯一 `c:id` 的 `<style>` 块及一个 `Screen`。`style="xx:panel-class"` 引用 ID 为 `xx` 的内联样式表并为元素添加 `panel-class`；`style="xx"` 只引用样式表。文档会先加载内置基础样式，再加载自定义样式，因此自定义声明可以覆盖基础外观。

## 构建与回归

在 Visual Studio 开发者环境中：

```powershell
cmake --build build/x64/debug --config Debug
ctest --test-dir build/x64/debug -C Debug --output-on-failure
```

本阶段的构建验证结果：引擎 DLL、静态库、Demo 和 UI XML 测试均通过；MSVC 仍会对导出类中使用 STL 成员发出 C4251 警告，后续 SDK ABI 稳定化时应将实现移入 PImpl。
