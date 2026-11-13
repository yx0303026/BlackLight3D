# Cloud Hop

`Demo` 现在是可游玩的 2D 平台跳跃小游戏。角色从山脚出发，沿平台向上跳跃，
收集七枚金币，避开尖刺，抵达顶端旗帜。三条生命用尽后可重新开始；
抵达终点即可通关，金币是额外的收集目标。

## 构建与运行

从仓库根目录在 Visual Studio 开发环境中执行：

```powershell
cmd /c "call D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat && cmake --preset x64-debug -DBLACKLIGHT_BUILD_TESTS=OFF && cmake --build --preset x64-debug --target blacklight_demo"
.\build\x64\debug\blacklight_demo.exe
```

已有构建目录时可以仅执行：

```powershell
cmd /c "call D:\VisualStudio2026\VC\Auxiliary\Build\vcvars64.bat && cmake --build build/x64/debug --target blacklight_demo"
```

需要 Windows、C++17、CMake/Ninja、MSVC 和支持 OpenGL 3.3 compatibility 的显卡驱动。
依赖固定为 RmlUi 6.3 和 FreeType；首次配置可能需要下载依赖。
Demo 默认使用 OpenGL，字体来自 Windows Fonts 目录的 Arial regular/bold。

`Content/platformer-atlas.png` 由 CMake 自动复制到可执行文件旁。
UI 在构建时从 `ui/Main.bui` 嵌入，修改后重新构建即可，不需要部署额外 UI 文件。
内容根目录与日志路径相对于可执行文件，不依赖启动时的工作目录。

## 操作

| 操作 | 按键或控件 |
| --- | --- |
| 开始 | START GAME 或 Enter |
| 左右移动 | A / D 或左右方向键 |
| 跳跃 | Space、W 或上方向键 |
| 暂停 / 继续 | Escape，或暂停菜单的 RESUME |
| 重新开始 | 暂停菜单的 RESTART；失败 / 通关后按 R 或点击按钮 |
| 退出 | 关闭窗口 |

平台为可从下方穿过的单向平台。跳跃输入带短暂缓冲，避免渲染更新快于固定更新时丢失按键。
角色移动使用固定步长，相机跟随并限制在关卡范围内。左右朝向会翻转，腾空时略微旋转，
行走与金币均有动画表现。尖刺扣除生命并回到起点，已收集的金币保留到重新开局。

## 代码与资源

| 路径 | 内容 |
| --- | --- |
| `src/main.cpp` | `PlatformerGame : Game`、关卡、运动、落地、收集、生命与 UI 事件 |
| `ui/Main.bui` | HUD、开始 / 暂停 / 失败 / 通关界面、按钮及样式 |
| `Content/platformer-atlas.png` | 角色、金币、平台、尖刺和旗帜图集 |
| `CMakeLists.txt` | 静态引擎链接、UI 嵌入与内容复制 |

主画面由 ContentManager → WIC → Texture → SpriteAtlas / SpriteAnimation →
SpriteBatchRenderer 绘制。`Camera2D` 提供跟随投影与坐标互转，`TextRenderer` 绘制
起点的 `TRAILHEAD` 世界标签。菜单与 HUD 使用现有 BUI / RmlUi UI 系统。

底部显示 FPS、CPU/GPU 毫秒数、精灵 draw call、顶点数和纹理切换。
CPU 时间在 Present 前结束，FPS 包含呈现等待；draw / vertex / swap 只统计精灵提交，
GPU 时间包含精灵与 UI。查询结果可能滞后数帧，启动时 GPU 显示 0 属于尚无完成样本。

实现说明和生命周期见 [2D 渲染指南](../docs/2d-rendering.md)。启动或运行异常记录到
可执行文件旁的 `blacklight_demo.log`。
