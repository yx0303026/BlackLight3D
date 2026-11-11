# Engine Core Runtime

The current application hierarchy is documented in
[Window and Game Hosts](window-game-architecture.md). Desktop applications derive
from `Window`; games derive from `Game` and implement `OnRender(GameTime)`.

本阶段把 BlackLight3D 扩展为可承载游戏逻辑的 C++17 运行时骨架。渲染设备接口位于 `BlackLight::Engine::Rendering`，SDK 头文件不暴露 OpenGL 或 D3D11 类型。当前设备已支持 Win32 窗口生命周期、清屏和呈现；GPU 资源、绘制提交与 UI 适配仍待完成，详见 [Rendering Device](rendering-device.md)。

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

## 渲染设备

旧 `IGraphicsBackend` 和 `engine/backends` 已移除。`Window` 使用
`RenderingDevice`，可选择 OpenGL 或 Direct3D 11，并驱动统一帧生命周期。
设备可创建或附着 Win32 HWND。当前资源和绘制接口仍会对未实现的后端操作
明确报错，不代表资源/绘制能力已经等价。

## UI 控件与样式

XML 支持 `Panel`、`Fill`、`Border`、`Button`、`ListBox`、`ListView` 和 `DataGrid` 容器控件。控件共用 `auto_size`、`width`、`height`、`c:id` 与 `style` 属性，并可嵌套子控件。`<layout>` 可包含具备唯一 `c:id` 的 `<style>` 块及一个 `Screen`。`style="xx:panel-class"` 引用 ID 为 `xx` 的内联样式表并为元素添加 `panel-class`；`style="xx"` 只引用样式表。文档会先加载内置基础样式，再加载自定义样式，因此自定义声明可以覆盖基础外观。

## 构建与回归

在 Visual Studio 开发者环境中：

```powershell
cmake --build build/x64/debug --config Debug
ctest --test-dir build/x64/debug -C Debug --output-on-failure
```

本阶段的构建验证结果：引擎 DLL、静态库、Demo 和 UI XML 测试均通过；MSVC 仍会对导出类中使用 STL 成员发出 C4251 警告，后续 SDK ABI 稳定化时应将实现移入 PImpl。
