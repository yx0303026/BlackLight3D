# Engine Core Runtime

本阶段把 BlackLight3D 从“窗口 + RmlUi 清屏示例”扩展为可以承载游戏逻辑的 C++17 运行时骨架。所有新增类型都位于 `BlackLight::Engine`，渲染实现仍通过 `IGraphicsBackend` 注入，SDK 头文件不依赖 OpenGL 细节。

## 模块

- `Time.h/.cpp`：`FrameClock` 提供单调时钟、最大帧间隔裁剪和固定时间步累加器。`GameTime::fixed_steps` 可用于统计一帧执行了多少次确定性更新。
- `Input.h/.cpp`：`InputState` 保存键盘、鼠标按键、鼠标位置和滚轮的当前状态及按下/释放边沿。平台后端只需要调用 `SetKeyDown`、`SetMouseButtonDown` 等方法。
- `Ecs.h/.cpp`：`World` 使用实体代际编号避免悬空句柄，组件按 `type_index` 分池。`Each<A, B>` 在一条遍历中访问满足组件集合的实体。
- `Content.h/.cpp`：`ContentManager` 以内容根目录为边界，缓存不可变二进制和 UTF-8 文本资产，适合在游戏初始化或后台加载阶段复用。
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
};

int main() {
    SampleGame game;
    return game.Run();
}
```

## Windows 后端

`WindowsOpenGLBackend` 现在会：

1. 创建可重复初始化的 Win32 窗口和 OpenGL 上下文。
2. 将键盘、鼠标按键、鼠标位置和滚轮消息写入 `InputState`。
3. 在窗口尺寸变化时同步 OpenGL viewport、RmlUi context dimensions 和 drawable size。
4. 使用 RmlUi 6.3 的 `RenderInterface`/`RenderManager` 生命周期，而不是旧版本不存在的全局 `SetRenderManager` API。

当前 `SimpleRenderInterface` 是可工作的空硬件接口，负责让 RmlUi 生命周期和 UI 树能够运行；它没有实现纹理上传和实际几何绘制。下一阶段应替换为现代 OpenGL/Vulkan/DirectX 设备层，并让 `SpriteBatch` 的提交回调直接写入持久映射的顶点环。

## 构建与回归

在 Visual Studio 开发者环境中：

```powershell
cmake --build build/x64/debug --config Debug
ctest --test-dir build/x64/debug -C Debug --output-on-failure
```

本阶段的构建验证结果：引擎 DLL、静态库、Demo 和 UI XML 测试均通过；MSVC 仍会对导出类中使用 STL 成员发出 C4251 警告，后续 SDK ABI 稳定化时应将实现移入 PImpl。
