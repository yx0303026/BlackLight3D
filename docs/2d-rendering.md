# 2D 渲染与平台跳跃 Demo

本次完成 `BlackLight/todolist.md` 的 1.3 全部七项及里程碑 A。公开入口由
`engine/Engine.h` 汇总，平台图像解码、GPU 提交、字体生成和图集算法留在
`BlackLight/src/engine`；图集、动画与文字的可变内部存储使用 PImpl。

## 图像与纹理

`DecodeImage(ContentManager&, path)` 使用 WIC 解码缓存文件的第一帧，返回
`DecodedImage`：从上到下、紧密排列、非预乘 alpha 的 RGBA8。PNG 的透明度保留，
JPEG 转换为不透明 RGBA。COM 已有 STA/MTA apartment 时均可使用；空数据、非法格式、
无效尺寸和超出 WIC 缓冲区限制的图像会抛出异常。

`LoadTexture` 在解码后通过当前设备上传纹理；纹理上传必须发生在设备所属线程。
路径相对于 `ContentManager` 的内容根目录。二进制缓存保留在内容管理器内，
解码结果和 GPU 纹理由调用者持有。

```cpp
auto texture = LoadTexture(GetContent(), *GetRenderingDevice(),
                           "platformer-atlas.png", Rendering::TextureFilter::Nearest);
```

默认过滤为 Linear；像素美术可选 Nearest。文件加载不会自动生成 mip 链。
`RenderingDevice::CreateTexture` 也可以直接上传 `DecodedImage::rgba` 或图集打包结果。

## SpriteBatch 与相机

`SpriteBatch` 保留命令收集和排序职责；`SpriteBatchRenderer` 负责 GPU 资源与提交。
渲染器持有动态顶点缓冲、Sprite GLSL/HLSL 管线和一像素白纹理。
每个精灵生成六个顶点，相邻且纹理相同的精灵合成一个 draw call。
容量不足时扩充缓冲；OpenGL 使用 orphan + sub-data，D3D11 使用 WRITE_DISCARD。

```cpp
SpriteBatchRenderer renderer(*GetRenderingDevice(), &GetFrameStats());
Camera2D camera;
camera.SetViewport(0, 0, 1280, 720);
camera.SetPosition({640, 360});
camera.SetZoom(1.0f);
renderer.SetCamera(camera);

const auto handle = renderer.RegisterTexture(texture);
auto &batch = GetSpriteBatch();
batch.SetSubmitCallback([&renderer](const std::vector<SpriteCommand> &commands)
{
    renderer.Submit(commands);
});
batch.Begin(SpriteSortMode::Deferred);
SpriteCommand sprite;
sprite.texture = handle;
sprite.position = {320, 240};
sprite.size = {40, 60};
sprite.origin = {20, 30};
sprite.rotation = 0.2f;
batch.Draw(sprite);
batch.End();
```

渲染器、相机与纹理必须存活到提交结束；示例中的局部对象在实际 `Game` 中应作为成员持有。
注册纹理使用借用关系，不能在注销前销毁或移动纹理对象。句柄 0 表示白纹理，
可用于纯色矩形。结束使用时先清除批次回调，再销毁文字、注销外部纹理，最后释放渲染器。

`position` 是旋转原点的世界位置，`origin` 是精灵局部坐标中的像素偏移；
`rotation` 使用弧度，正角度在 Y 向下的画面中顺时针旋转。
`scale` 可为负以镜像精灵。UV 的 `(0, 0)` 对应图像左上角。
`depth` 用于排序，当前 Sprite 管线关闭深度测试；`layer` 为应用保留字段。

Deferred 保留绘制次序，适合透明精灵；Texture 对纹理句柄稳定排序，可能改变重叠关系。
FrontToBack / BackToFront 按 depth 稳定排序，Immediate 每次 Draw 直接提交。

Camera2D 的位置表示视口中心，Zoom 表示每世界单位对应的像素数，Y 轴向下。
每次窗口缩放后应更新视口。坐标互转使用同一组位置、缩放和视口参数：

```cpp
const auto screen = camera.WorldToScreen({100, 200});
const auto world = camera.ScreenToWorld(GetInput().GetMousePosition());
```

Demo 使用整个窗口视口，并限制跟随相机在关卡范围内；窗口比关卡更大时居中显示。

## 位图文字

`TextRenderer` 通过 GDI 将 Arial 的可打印 ASCII 字符生成透明字形图集，
绘制前完成 GDI 刷新，随后作为纹理注册到 SpriteBatchRenderer。
支持换行、缩放和颜色，非 ASCII 字节用 `?` 替代。
字体高度范围为 1–128 像素，`GetLineHeight()` 返回未缩放行高。

```cpp
TextRenderer text(*GetRenderingDevice(), renderer, 20);
batch.Begin();
text.Draw(batch, "TRAILHEAD", {40, 1320});
batch.End();
```

文字遵循当前相机，适合世界标签或调试文本。固定 HUD 可以使用独立的像素相机，
也可将屏幕位置通过 ScreenToWorld 转换后绘制，并相应抵消相机缩放。
复杂排版、Unicode 和交互控件使用 RmlUi；Demo 的菜单、HUD、暂停及结束界面均来自
`Demo/ui/Main.bui`，位图字体用于起点的世界标签。

## 图集与动画

`SpriteAtlas` 将具名像素矩形映射为归一化 UV，检查区域是否越界。
`SpriteRegion::Apply` 写入纹理和 UV；命令尺寸保持默认 `(1, 1)` 时自动采用区域像素大小。

```cpp
SpriteAtlas atlas(handle, texture.GetWidth(), texture.GetHeight());
atlas.AddRegion("idle", 0, 0, 16, 24);
atlas.AddRegion("run", 16, 0, 16, 24);
SpriteAnimation animation({atlas.GetRegion("idle"), atlas.GetRegion("run")}, 0.12, true);
animation.Update(elapsed_seconds);
animation.GetCurrentFrame().Apply(sprite);
animation.Pause();
animation.Play();
animation.SetLooping(false);
animation.Reset();
```

动画默认播放，Pause 保留帧和剩余时间；Play 恢复，已结束时从头重播；Reset 从第一帧重新播放。
非循环序列在最后一帧停留一个完整帧间隔后结束，可由 IsFinished / IsPlaying 查询。
非法帧间隔会抛异常；Update 忽略无效或非正时间，并避免按每个流逝帧逐次循环。

`SpriteAtlasBuilder` 可将多张已解码图像打包成 RGBA8 图集：

```cpp
SpriteAtlasBuilder builder;
builder.AddImage("player", DecodeImage(GetContent(), "player.png"));
builder.AddImage("coin", DecodeImage(GetContent(), "coin.png"));
const auto packed = builder.Build(1024, 1);
Rendering::TextureDesc desc;
desc.width = packed.image.width;
desc.height = packed.image.height;
desc.initial_data = packed.image.rgba.data();
desc.initial_data_size = packed.image.rgba.size();
auto packed_texture = GetRenderingDevice()->CreateTexture(desc);
auto packed_atlas = packed.CreateAtlas(renderer.RegisterTexture(packed_texture));
```

打包使用按高度稳定排序的 shelf 布局和透明 padding；名称必须唯一。
输出宽度不超过指定上限，优先选取二次幂宽度，高度向上取二次幂。
返回像素和区域元数据，不写文件、不压缩纹理。Demo 使用随项目提供的 PNG 图集及具名区域。

## 帧统计与 GPU 计时

Game 自动管理 FrameStats；渲染器传入 `&GetFrameStats()` 后累计精灵 draw call、
提交顶点数和纹理切换。BeginFrame 清空当帧计数，EndFrame 发布计数，
因此 OnUpdate 中取得的是已完成帧的数据，不会因新帧清零而显示全零。
这些绘制计数仅覆盖 SpriteBatchRenderer；RmlUi 和自定义 CommandList 提交不包含在内。
纹理切换统计跟踪该渲染器相邻提交，包括跨帧的最后与第一次绑定。

FPS 按约一秒的完整帧起始间隔平均，包含呈现等待。
CPU 毫秒数覆盖消息泵、更新和绘制提交，在 Present 前结束，排除显式的垂直同步呈现等待；
它是 CPU 侧经过时间，不等于线程实际消耗的 CPU 时间。
Demo 以约 5 Hz 更新统计文本，金币、生命与菜单切换立即更新。

Window 在应用绘制及 UI 绘制前后调用 BeginGpuTimerQuery / EndGpuTimerQuery。
OpenGL 使用 GL_TIME_ELAPSED，D3D11 使用 timestamp + timestamp-disjoint；
两者都通过两个查询槽异步读取，不等待未完成结果，繁忙时跳过采样。
D3D11 丢弃 disjoint / 无效频率结果，查询失败走统一设备错误处理。
Game 把最近取得的 GPU 毫秒数写入快照；尚无结果时为 0，GPU 结果可能滞后数帧。
GPU 时间包括精灵与 UI，计时范围不包括 Present。

独立使用 RenderingDevice 时，由调用方配对 Begin/End，并用 TryGetGpuTimeMilliseconds 轮询。
在 Game/Window 已管理的绘制范围内，不应再次嵌套同一设备的 GPU 查询。

## OpenGL 与 UI 状态

RmlUi GL2 使用兼容管线客户端数组，SpriteBatch 使用着色器、VAO 和 VBO。
UI 路径必须先切到自己的默认 VAO，再保存客户端数组状态，并在同一个 VAO 上恢复；
把 Sprite VAO 状态恢复到默认 VAO 会污染下一帧的 UI 数组，在本机 NVIDIA 驱动上曾导致崩溃。

RenderUi 显式使用纹理单元 0 和客户端纹理单元 0，暂时解除单元 0 的 sampler 对象，
并恢复应用 VAO、缓冲、着色器、FBO、纹理单元和 sampler。不要删除这些边界处理。
无需改用不带 UI 的 Demo 或关闭 GPU 查询来规避问题。

## 生命周期与当前边界

应用在 OnDeviceLost 中释放 SpriteBatchRenderer、TextRenderer、外部纹理和批次回调，
在 OnDeviceRestored 中重建并重新注册。设备恢复前的纹理句柄不得继续提交。
UI 树在渲染设备关闭之前销毁，字体的 renderer 必须比 TextRenderer 活得更久。

当前实现面向 Windows 和 OpenGL 3.3 compatibility / D3D11，不新增跨平台、音频、
通用物理或完整字体排版引擎。Demo 的平台落地为关卡专用运动逻辑，
不把这些玩法规则放入 SDK。数学、场景、音频与通用碰撞任务仍在后续清单中。

构建、运行路径和操作见 [Demo 说明](../Demo/README.md)。

## 本次验证

2026-11-13 完成 x64 Debug 全量构建，包含共享 / 静态引擎、Demo 与编辑器。
OpenGL Demo 已实际启动，确认开始界面、移动与跳跃、暂停界面、相机跟随，
以及非零 draw call / 顶点 / 纹理切换与 CPU/GPU 计时；窗口缩放、最小化与恢复后仍可响应，
关闭窗口后正常返回 0。小窗口使用紧凑菜单与统计栏布局。

未新增或运行测试。D3D11 路径通过编译，本轮运行核验使用 OpenGL；
尚未对整条关卡的收集、受伤、通关与重开流程逐一完成实机操作核验。
