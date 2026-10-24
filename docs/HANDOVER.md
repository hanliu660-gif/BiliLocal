# BiliLocal 64 位自建版 · 交接文档

> 更新时间：2026-10-24 深夜（**第八版：体检瘦身轮**）——`master` = 7f8685a（瘦身）← 473ba62（Win11 UI）← `3f86f74`（VLC 3.0.24）← `3155020`（单击暂停+扁平QSS+输入路由修复）← `c834050`（event(Init) 全限定名修复）← `cbc0b42`（启动续播+目录记忆）← `c8679e6`（现代化轮：主题/图标/面板/测试）← `d7847e8`（清理）← `49c173c`（单平台化+i18n）← `4702ab1`；已推送 `github.com/hanliu660-gif/BiliLocal`（remote 名 `github`），exe 1,093,120 → 948,224 → 964,608 → 925,696 字节；dist 358M → 176M。
> 写给新会话：先读这份，再看 `src\` 的 git 历史——**每个提交都有根因和证据**。
> 一句话：把开源播放器 BiliLocal 0.4.2（GPL-3.0，2016 年停更）改造成 64 位私人 Windows 播放器：**VLC 3 + d3d11 硬解直出（Embed 唯一输出）+ 单底栏 UI**，日常播服务器录播 + 弹幕，CPU 从 ~210% 降到 ~11%。
>
> **路径基准（2026-10-24 修订）**：本文是工程级文档，**正本在工程根 `C:\DSH\APP\BiliLocal\交接文档.md`**；git 仓库（`src\`，即 GitHub 仓库根）内的 `src\docs\HANDOVER.md` 是它的同步备份副本——**更新正本后 `cp 交接文档.md src\docs\HANDOVER.md`，随仓库提交推送**，两份内容必须保持一致。下表 `build\dist\deps\tools\test` 等目录在仓库外、不入 git。

---

## 0. 用户约束（必须先看）

- **改代码/改行为前先问一句**（他明确要求过）。
- **不要抢焦点**：不要移动鼠标、不要抢前台。验证靠「投递消息 / 截图 / 读日志 / 改配置重启」，不要用真实光标。
- 要**实测证据**（截图、日志原文、UIA 读数），不要"应该可以"。
- 路径别写死在自己脚本里，工程可能被搬。

## 1. 目录与关键路径

| 路径 | 内容 |
| --- | --- |
| `src\` | 源码 git 仓库：`master` = `3f86f74`（全部工作）；tag `v0.5.0-vmem` = 早期 vmem 线回滚点 |
| `src\docs\HANDOVER.md` | 交接手册的仓库备份副本（正本在工程根 `交接文档.md`，更新后同步过来） |
| `build\` | 影子构建（qmake + mingw32-make），产物 `build\release\BiliLocal.exe` |
| `dist\` | **可直接运行的成品**：exe + Qt5 + VLC 3.0.24（FFmpeg 8.1.2）+ 插件 + 翻译（Embed 硬解直出版） |
| `deps\` | 工具链：Qt 5.15.2 (mingw81_64)、MinGW 8.1.0、VLC SDK 头文件/导入库（在 `deps\thirdparty`；旧运行库目录 deps\vlc 连同 2.2.8/3.0.21 各备份已全删，VLC 运行库只存在于 dist） |
| ~~`backup\`~~ | 已删除（2026-10-24 用户决定）：git 历史就存在 `src\.git` + GitHub 远端里，不再保留离线 bundle |
| `tools\` | 调试/测试脚本（2026-10-24 只留 8 个通用脚本，见第 8 节） |
| `test\` | `embed.xml` = 测试弹幕 fixture（8KB；媒体自备：任意视频同目录同名即可） |
| `%LOCALAPPDATA%\BiliLocal\Config.txt` | 运行时配置（JSON） |
| `%LOCALAPPDATA%\BiliLocal\cache\msg.log` | 日志（超 10MB 自动轮转成 .old） |

> 2026-10-24 清理：`dist-vlc2\`（旧 vmem 线）、`dumps\`（4.9G WER 转储）、`build-dbg/fp\`（旧构建树）、`embed-wt\` worktree、`BiliLocal-64bit.patch`（过时补丁）已删除——回滚材料都在 `backup\` 和 git 历史里。

## 2. 当前状态

- **exe**：`dist\BiliLocal.exe`，948,224 字节，SHA256 `1F0A9097…`（构建自 `3f86f74` 同树）
- **源码**：`master` = **`3f86f74`** ← `3155020`（单击暂停/QSS/路由修复）← `c834050`（event(Init) 全限定名修复）← `cbc0b42`（启动续播）← `c8679e6`（现代化轮）← `d7847e8`（清理）← `49c173c` ← `4702ab1`；上游 = `9d1cb05`；回滚点 tag `v0.5.0-vmem` = `46b68b9`
- **运行库**：VLC **3.0.24**（FFmpeg 8.1.2，d3d11 直出；2026-10-24 自 3.0.21 升级——130+ 安全修复是动机，A/B 30 秒窗口实测 CPU 持平 0.7%→0.9%；**注意 dist\plugins 里混住着 Qt 平台插件（platforms/imageformats/mediaservice/styles），整目录替换 VLC 时必须把这四个目录合并回来，否则 Qt 起不来**；3.0.21 旧库备份在临时目录，确认稳定后可弃）；dist 只带 exe 实际导入的 5 个 Qt DLL（Concurrent/Core/Gui/Network/Widgets），Qt5Multimedia/Qt5OpenGL 均已移除
- **配置**：`Output=Embed` 已成唯一输出（键保留但代码不再读取）；`Immediate=True`；`HardwareDecode` 已无消费者（d3d11 固定开启，Arguments 手写 `--avcodec-hw=`/`--vout=` 仍可覆盖）
- **i18n**：源码全英文串，中文走 `dist\locale\zh_CN\bililocal.qm`（143/143 全译，含繁体）；改 UI 文案后记得 lupdate→补译→lrelease→部署 qm
- **备份**：git 历史 = 本地 `src\.git` + GitHub fork 双保险（离线 bundle 与 Z: 副本已按用户决定删除）；vmem 旧线源码完整保留在 tag `v0.5.0-vmem`，重编需自行从 VideoLAN 存档获取 VLC 2.2.8

### 2.1 第四版清理记录（2026-10-24，三轮回，+66/−1114 行 / 33 文件）

删掉的死物（全部经 grep 确认零引用/零消费者后才删）：
- **死入口/死控件**：Info 永远 0×0 的弹幕表格及其右键菜单、「导出弹幕」功能链（`Danmaku::saveToFile`→`Utils::customUrl`）、`Interface::percent/warning/hide` 三条完整调用链、Prefer 性能页整页（**含能把 Output 写坏成已知崩溃线的三个单选钮**）、界面页「检查更新」复选框与背景图 UI、插件页签（此构建永远空表）、`Calendar`/`Jump` 死类、`Local::synchronize`、`Utils::parseSite`、`Parse::FutureRecord`（QFutureWatcher 危险路径）、Running/VPlayer 调试残留、`.pro` 里 Post/Seek/Sign 死引用与整块 NO_OPENSSL 死配置
- **死后端**：win32 不再编译 QMM/NIL（源码保留在树里供 linux/android 块引用）
- **顺手修复**：置顶设置改完即时生效（`Home::setWindowFlags` 挪入 private slots，原来 invokeMethod 静默失败）、语言列表改用 `Utils::localPath` 不再依赖启动目录、AcFun 解析 reserve 算错、List/Danmaku 变量遮蔽、Menu/Info 死状态 isStay
- 验证：每轮重编后空载启动 + Z: 真实播放（窗口树确认 Direct3D11 vout 子窗铺满、截图确认画面渲染）；弹幕可见性仍需人眼（GL 层抓屏不可见）

### 2.2 第五版：单平台化 + 全项目体检（2026-10-24 傍晚）

**架构决策**：本工程定位为"私人 Windows 播放器"，删除所有不可达路径——
- **vmem 视频输出线整体移除**：Opaque/Widget/Window/Detach 四个 Private、VPlayer 的 PixelBuffer/fmt/lck/dsp 内存帧回调（~120 行）、ARender 的 drawPlay/drawStop/drawTime/fitRect/宽高比/首选尺寸/setFormat/setBuffer、OpenGLRender 的 7 个 YUV/RGB 视频着色器与程序、APlayer::decode 信号。**Embed 成为唯一输出**（弹幕精灵/图集/笔画着色器 ASprite/SyncTextureSprite/OpenGLAtlas 全保留——它们是弹幕绘制路径，不是视频路径）
- **跨平台配置移除**：.pro 的 linux/macx/android 块、QPlayer/NPlayer/RasterRender 全套源文件、Quick2 界面（UI+QML+Quick2Private）、ABuffer/PFormat 头。回滚材料：tag `v0.5.0-vmem` + backup bundle
- **Home 顺带清除 Embed 下无效的 UI**：右键菜单的「比率/缩放」子菜单（写 setVideoAspectRatio/按视频尺寸开窗，Embed 下无可视效果）、起播按视频尺寸重摆窗口与播完恢复几何、GL 进度条预览（Embed 下不可见，底栏进度条才是有效 UI）
- **死资源**：qrc 移除 stop/next/previous.png 与 :/Text/DATA；删 res/00-02.jpg、INFO（老更新检查数据）；**Bundle 推送机制整体删除**（Res.qrc 无 /Bundle 前缀，push 一直是空转）
- **修复**：msg.log 超 10MB 轮转成 .old；单实例崩溃后 removeServer 重试；`timeMutex` 改 QRecursiveMutex（setTime 持锁 processEvents 的重入地雷）；`QString::SkipEmptyParts`→`Qt::SkipEmptyParts`、`toTime_t`→`toSecsSinceEpoch`、catch 按引用（清掉全部编译警告）
- **仓库卫生**：logs\ 634MB、dumps\ 1.2GB(WER) 整删；tools\ 只留 8 个通用脚本（setcfg/pwc/shot/perfrun/kids/embed-probe/dense/fix-bom）；根目录过时 patch 删除；dist 移除 Qt5OpenGL.dll
- 验证：Z: 真实播放（Direct3D11 vout 子窗 + 截图图像分析确认画面）+ 无 Qt5OpenGL 启动正常；**弹幕可见性仍需人眼看一次**
### 2.3 现代化轮（c8679e6，2026-10-24 晚）
- UI：h:mm:ss 时长时间格式；深色主题（/Interface/Theme 0浅/1深/2跟随系统 + DWM 深色标题栏，设置-界面页切换）；底栏 打开/设置/全屏/列表 换 QPainter 矢量图标；倍速按钮单击循环 1.0→3.0；Ctrl+滚轮=弹幕不透明度；快捷键页改 QKeySequenceEdit 按键捕获；「文件与弹幕」面板重做（最近播放+搜索+双击/Del）
- 源码：c++17；VPlayer 补 override；APlayer::Event enum class（跨线程 invokeMethod 已注册 metatype）；QStringRef 全部退役
- 测试：`src\test\tst_parse.pro`（qmake+QtTest，7 用例全绿）——构建：`qmake ..\src\test\tst_parse.pro && mingw32-make` 后跑 `build-test
elease\tst_parse.exe`；**顺手修了上游 ASS 颜色 bug**（\c&H…& 的尾 & 使 toInt 得 0 → 带色 ASS 弹幕全丢；现为 chop+字节序转换）
- 跳过：CMake 迁移（本机无 cmake，不交付未验证的构建系统）
### 2.4 Win11 设计语言轮（32c6597…473ba62，2026-10-25 凌晨）
- **全局 QSS 重写（双主题）**：Win11 分层表面（深：窗 #202020/卡 #2b2b2b/输入 #2f2f2f；浅：#f3f3f3/#ffffff）、8px 卡片圆角、强调色 #4cc2ff(深)/#0067c0(浅)+黑字选中态、Segoe UI Variable→YaHei UI 字体栈；新增对象名钩子 `Card/CardTitle/CardNote/SepLine/NavList` 供设置页使用；palette 同步对齐（**业务绘制一律走 palette 取色，别写死颜色**）
- **Prefer 重排**：顶部页签 → 左导航列表+右 QStackedWidget；6 页并 5 页（Thanks+License 合并进关于）；GroupBox 单输入行 → 卡片内"说明左/控件右"行式布局；裸输入框类型化（倍率/跳转秒/密度上限=SpinBox 带后缀，密度 0 显示"不限"；**速度/存活时间保留可编辑下拉预设——值是含 %{width} 的表达式字符串，键没变**）；`/Interface/Size` 的"尾随空格=锁定"暗约定显式化为「锁定」复选框（Home::closeEvent 判定逻辑未动）；Shield 导入/导出从纯右键升级为常规按钮。所有配置键、信号、重启/重解析逻辑零改动
- **Editor**：Track 绘制调色板化（原硬编码灰底白条在深色下刺眼）；空池提示/滚动条描边交给主题；列表下加 合并/归类/分离/删除 按钮排（右键和快捷键保留）
- **测试基建**：`BILILOCAL_UI_TEST=prefer/editor/both`（与 EWindow 钩子同一编译开关）进程内自动开对话框巡览，Prefer 巡览时窗口标题带页码「设置 - N」；**外部投递点击对模态弹窗无效（Qt 按真实光标路由）——验证对话框必须走进程内钩子**；Win32 GetWindowTextW 读 Qt 标题会是乱码，用 UIA Name 读（验证脚本样例在 %TEMP%\verify_ui3.ps1）
- **i18n**：新增 13 串全译（zh_CN/zh_TW 各 164/164），qm 已部署 dist
- 验证：深/浅两主题 × Prefer 五页 + Editor 共 12 张截图逐张人验（渲染、译名、控件状态全对，六图六哈希证明巡览真实切换）；exe 948,224 → 964,608 字节
### 2.5 体检+瘦身轮（0c4f7b7…7f8685a，2026-10-25 凌晨）
- **dist 358M → 176M**：升级 3.0.24 时旧 3.0.21 插件树残留在 `dist\plugins\` 顶层、新树被放进了嵌套的 `plugins\plugins\`——改名验证确认嵌套树是死重（142M 已删）；再裁掉 gui/stream_out/access_output/services_discovery/visualization/mux/keystore/control/stream_filter/stream_extractor/video_filter（BiliLocal 只用 libvlc 核心，VLC 自带 GUI 等全无用），裁后实测播放正常（截图 24 万独立色）。**整目录替换 VLC 时记得沿用「先验证再删」流程，Qt 四目录(platforms/imageformats/mediaservice/styles)必须保留在顶层**
- **插件机制+网络栈整体移除**：Plugin::load 每次启动对 dist\plugins 的 758 个 DLL 逐个 LoadLibrary 探测（纯浪费）；联网功能死后 Load 的网络队列(AccessPrivate 167 行)、Running::unrecognizedComment（零接收者）全部不可达。已删 Plugin.*、AccessPrivate.h；**Load 重写为纯本地加载器**（读文件→剥BOM→嗅探→同步解析→入池，文件间用排队槽让步不冻结界面），`getHead()` 改 `busy()`；QtNetwork 保留（QLocalSocket 单实例在 Qt5 属于该模块，已在 .pro 注明）。exe 964,608 → 925,696 字节
- **List 目录扫描合并**：打开一个视频原来最多 3 次全目录列举（itemFromFile 的 danmakuNearby+分组扫描、mediaChanged 兜底再扫）——Z: 是 WebDAV，每次列举都有可感延迟。现合并为 1 次（danmakuNearby 上移复用；itemFromFile 单次列举同时收集同名弹幕与同系列分组）
- **小项**：QFontDatabase 字体枚举进程内缓存（Utils::fontFamilies）；Config.h 未用模板特化加 [[maybe_unused]]（MinGW 8.1 要求放 template<> 之后）；单实例 waitForReadyRead 加 500ms 显式超时
- **踩坑记录**：①invokeMethod 按名调用要求目标是槽——`private slots:` 而非普通私有方法；②Danmaku::append 按 source 路径去重，「同一 xml 传两遍」造不出 pool=2，测 Editor 自动弹出要用两个不同路径的文件；③dequeue 时机：任务在 processNext 开头取出，尾部残留 dequeue 会吞掉下一任务（已修+加了 Load: processed 日志）
- 验证：双不同源 xml → 两条 Load: processed 日志 + pool=2 + Editor 自动弹出；单实例二次启动进程数保持 1 且文件转发加载；tst_parse 7/7；UI 巡览 6 图 6 哈希；Z: 续播截图正常
- **补修（44fbb3e）**：cbc0b42 的「记住打开目录」只接了 Menu 面板，底栏打开按钮（常用入口）仍传空目录，靠原生对话框进程内记忆装作正常——重启即失忆。现底栏与面板共用 /Interface/OpenDir（选完写回、重启有效）

## 3. 怎么用（用户视角）

1. 双击 `dist\BiliLocal.exe`；拖入 `*.flv`/`*.mp4`（或命令行传路径），同目录同名 `*.xml` 自动挂弹幕
2. 鼠标移动 → 底部**单控制条**出现（深色半透明）；停 2 秒自动收起
3. 底部条从左到右：`▶` 播放暂停、时间（超 1 小时自动 h:mm:ss）、进度条（点哪跳哪、可拖）、`弹幕·开/关`、`1.0x` 倍速（单击循环 1.0→1.25→1.5→2.0→3.0）、音量、四个图标按钮（打开=文件夹、设置=调节杆、全屏=四角、列表=三横线，悬停有提示）
4. `列表` 打开「文件与弹幕」悬浮面板——**不抢主窗口键盘焦点**（快捷键全程可用）
5. **快捷键**：**单击画面=暂停/继续**（250ms 延迟判定避免与双击全屏冲突，`/Interface/Sensitive` 开关默认开）；滚轮=音量（未开媒体也正常，音量常驻 VPlayer 成员）；**Ctrl+滚轮=弹幕不透明度**；数字 0-9=百分比跳转；Shift+←/→=±60 秒；←/→=±10 秒；↑↓=音量；M=静音；T=置顶；F=全屏、F11=全屏、双击画面=全屏；Esc=退出全屏；Ctrl+←/→=弹幕延迟；**长按右键=3 倍速**（松开恢复，短按右键=菜单）
6. **断点续播**：关闭时记住位置，重开自动跳回（>10 秒且未看完才生效；`Player/Resume=false` 可关）
7. **启动自动续播**（2026-10-24 新增）：无参数启动直接播最近一条有断点的记录（`Player/AutoResume`，设置-播放页可关；命令行传文件/拖拽不触发）；「打开」对话框记住上次目录（`Interface/OpenDir`）

> **验证注意**：弹幕层是 QOpenGLWindow，对 BitBlt/PrintWindow 抓屏**不可见**——截图全黑不代表画面黑（视频/底栏用 `tools\pwc.ps1` 能抓到）。**验证弹幕必须人眼看，别信截图！**

## 4. 怎么重编 / 部署

```powershell
$deps = 'C:\DSH\APP\BiliLocal\deps'
$qt   = "$deps\5.15.2\mingw81_64"
$mg   = "$deps\Tools\mingw810_64\bin"
$env:PATH = "$mg;$qt\bin;" + $env:PATH
cd C:\DSH\APP\BiliLocal\build
# 改过 .pro / 新增文件才需要重跑 qmake（改了先删 Makefile*）
& "$qt\bin\qmake.exe" ..\src\BiliLocal.pro THIRDPARTY=C:/DSH/APP/BiliLocal/deps/thirdparty CONFIG+=release
mingw32-make -j8 release
# 部署（必须先停进程）
Stop-Process -Name BiliLocal -Force -ErrorAction SilentlyContinue
Copy-Item build\release\BiliLocal.exe ..\dist\ -Force
```

- 源码里有中文 `QStringLiteral`/`tr()` 的文件必须保持 **UTF-8 BOM**；改完跑 `tools\fix-bom.ps1`。
- 改了 `.pro`（或编译旗标如 c++ 标准）后重编若报 `PCH files were found, but they were invalid`：删掉 `build\release\Common.h.gch` 再 make。
- 回滚点：`git checkout v0.5.0-vmem` + 重编 = 回到最初的 vmem 线（VLC 2.2.8 运行库已删，见文末附：快速回滚）。

## 5. 代码结构速览（写给要改代码的你）

- **架构**：`Local` 服务定位器，`lApp->findObject<T>()` 取 12 个模块（Config/Shield/Danmaku/Running/Interface/APlayer/ARender/Load/List…）
- **播放**：`VPlayer`（libvlc，唯一后端）。视频经 `set_hwnd` 直出到原生子窗口 `ESurface`（d3d11va 硬解 + direct3d11 vout，参数在 VPlayer 构造时固定注入，可被 Arguments 手写覆盖）
- **弹幕**：`Load`（仅本地文件）→ `Parse::parseComment`（**同步**，直接返回结果）→ `Danmaku` 模型 → `Running`（池线程布局防碰撞）→ `EWindow`（透明 GL 覆盖层，兼作输入面）绘制；弹幕文字精灵走 `ASprite`/`SyncTextureSprite`/`OpenGLAtlas` 图集
- **UI**：`Home`（主窗，快捷键/全屏/seek 横带）+ `Info`（单底栏）+ `Menu`（文件与弹幕悬浮面板）+ `Editor`（弹幕池编辑）+ `Prefer`（设置，6 页）
- 每层的关键决策都写在 git 提交说明里：`git log --oneline` + `git show <hash>`

## 6. 修过的坑（历史存档，现象 → 根因 → 修法）

1. 中文路径乱码 → `CommandLineToArgvW` 取 argv
2. 录播姬 xml 带 BOM 解析 0 条 → 剥 `EF BB BF`
3. 死成员指针崩溃（WER dump 定位）→ 删 + 置空
4. 底栏拉不回来（动画状态脱节）→ `isShown()=isVisible()` + 先 stop 动画
5. 不动鼠标不收起 → 400ms 常驻轮询
6. 全屏白块 → 三层几何同步 + 延时重排
7. 退出全屏丢最大化 → wasMaximized + DWM 禁过渡
8. 进度条不灵敏 → eventFilter 接管 + 整条横带
9. 设置存不住 → %LOCALAPPDATA% + 去抖 + 原子写
10. VLC3+vmem 崩 / VLC2 无法硬解喂 vmem → **架构性互斥，最终走 Embed 直出路线**（证据链见 embed 系列提交）
11. MinGW `::time` 撞名 C 库 → 改名 `timeMutex`
12. MinGW 链式赋值 QFlags 报错（MSVC 容忍）→ 拆开写
13. lambda 按引用捕获 ctor 局部变量 → 悬空 UB → 改静态
14. **DWM MPO 启用时 BitBlt/PrintWindow 抓屏黑屏缺层**（视频/弹幕层隐身）→ 用 `pwc.ps1`（PW_RENDERFULLCONTENT）；弹幕 GL 层任何抓屏都不可见，**只能人眼验证**
15. Parse 异步 ResultDelegate（QFutureWatcher 链）在在线模块移除后不触发 → **同步化 VectorRecord**
16. Win32 子类化转发鼠标消息
17. **enum class 槽参数必须全限定**：moc 记录的是声明原文（Event），跨线程 invokeMethod 的 Q_ARG(APlayer::Event,...) 给的是全限定名——不匹配时**静默失败**（无警告、返回值没人查），症状是 state 永远 Stop（拖不动/无时长/标题不变/List 存 Time=-1）。修法：槽签名写全 event(APlayer::Event)。另：**图像分析验证 UI 数字不可靠**（会迎合 prompt 里的示例数字），时长/进度这类断言要用行为证据（标题、配置断点、日志）
18. （教训）invokeMethod 返回值从不检查——关键投递点要么查返回值，要么改信号转发
19. **childAt 无视 WA_TransparentForMouseEvents**：EWindow::route 转发点击时 childAt 命中透明 ESurface → 事件进黑洞（视频区单击/双击/拖动全失灵）。转发前要沿 parentWidget 跳过穿透控件
20. **QWidget::mouseDoubleClickEvent 默认转发 mousePressEvent**：重写双击后调基类 = 双击又触发一遍单击逻辑；不需要基类行为就别调：投递消息不达 vout proc + VLC 重建窗口后钩子悬空崩溃 → 整体废弃，改覆盖层路由输入


## 7. 已知问题 / 未完成

1. **Z: 网络盘 + Embed 模式的长时段表现未验证**（本地文件全通过；Z: 读速抖动属挂载链路问题，但 d3d11 直出下表现未知）
2. 大 xml（数万条）解析在 GUI 线程同步执行，可能卡顿数百 ms~秒级（Parse 同步化后引入；本地录播 xml 无感，如需要可再做异步，但要用可靠机制——别回到 QFutureWatcher；注意 Load 的 onFinish 回调里 `&task` 引用，异步化时会悬空）
3. `Editor`/`Prefer` 对话框还是 2015 年老 UI（功能正常，纯观感问题）
4. 倍速按钮长按右键的 350ms 阈值/3.0 倍率是拍脑袋值，可按手感调

## 8. 工具脚本（`tools\`，2026-10-24 精简后仅 8 个）

| 脚本 | 用途 |
| --- | --- |
| `setcfg.py <key> <value>` | 改 Config.txt 任意键（布尔/数字/字符串自动识别） |
| `pwc.ps1` | **PrintWindow(PW_RENDERFULLCONTENT) 抓窗口**——抓视频/底栏用这个 |
| `shot.ps1` | BitBlt 抓屏（D3D/GL 层会黑，仅参考） |
| `perfrun.ps1` | 起播放器采样 CPU（`-Media` 指定本地文件、`-Exe` 指定 exe） |
| `kids.ps1` / `embed-probe.ps1` | 列窗口树 / 窗口树+父窗口+类名 |
| `dense.py` | 解析**指定**录播 xml 求密集窗口（注意：脚本内路径写死，需改源码指路） |
| `fix-bom.ps1` | 源文件 BOM 对齐 git HEAD |

（历史上几十个 `fs*/popcheck*/stuckslide*` 等一次性排查脚本已删，如需从 git/备份找。）

**进程内自动化验证钩子**（比外部投递消息可靠——后者会被 Qt 按真实光标位置路由而失准）：设环境变量 `BILILOCAL_EMBED_TEST=1` 启动（`cmd /c "set BILILOCAL_EMBED_TEST=1&& start ..."），EWindow 每 300ms 注入鼠标事件：第 30/50 拍画面中央双击（验证全屏闭环）、第 35 拍单击（验证单击暂停），配合 `msg.log` 中 `click-toggle` / `VPlayer::event` 日志判读结果。

**PS 5.1 铁律**：一律 `-NoProfile -ExecutionPolicy Bypass -File` 调用；脚本存 UTF-8 带 BOM、**不写中文字面量**；`$Pid` 只读、`Move/Cli/lp` 是别名；`Start-Process -RedirectStandardError` 别配 `-Wait`；子进程会继承 stdout 管道（给 `UseShellExecute=$false` 的启动加 stdio 重定向，否则调用方假死）。

## 9. 环境与依赖

- Qt **5.15.2** mingw81_64：`deps\5.15.2\mingw81_64`
- MinGW **8.1.0**：`deps\Tools\mingw810_64\bin`
- VLC **3.0.24**：运行库在 `dist`（升级方法：官网下 win64 安装包静默解包 /S，替换 dist 的 libvlc/libvlccore/plugins/lua，**必须把 plugins 里 Qt 的 platforms/imageformats/mediaservice/styles 四目录合并回来**）；构建用头文件/导入库在 `deps\thirdparty`（3.0.21 SDK，libvlc API 兼容）
- Python：`C:\Users\23239\.dsh\dsh-runtimes\dsh-next-primary-runtime\dependencies\python\python.exe`
- VC 运行库（VLC3 需要）：系统已装 vcruntime140/140_1/msvcp140
- 网络盘：`Z:` = RaiDrive 挂载 WebDAV，读速 0.1–45 MB/s 抖动；读慢时播放器"未响应"属挂载问题

## 10. 下一步建议

1. **Z: 真录播日常使用观察**（Embed 模式下挂载抖动的表现是唯一未验证项）
2. `Editor`/`Prefer` 对话框现代化（低优先，纯观感）
3. 大 xml 异步解析（仅当某天录播 xml 大到可感知卡顿再做；别用 QFutureWatcher）
4. 渲染层（EmbedPrivate/ARender/OpenGL）**已稳定，别动**

## 附：快速回滚

- **源码回滚**：`git checkout v0.5.0-vmem` → 重编（回到最初的 vmem 线）
- ~~旧线运行时~~：VLC 2.2.8 运行库与免编译 vmem exe 已于 2026-10-24 删除。真要跑 vmem 线：从 tag 重编 exe + 自行从 VideoLAN 官方存档（historical releases）下载 VLC 2.2.8 win64 + 配置改 `Output=Window`。注意：vmem 线是已知崩溃/高 CPU 路线，仅作考古用途
- **全历史恢复**：`git clone https://github.com/hanliu660-gif/BiliLocal.git <目录>`（或从本地 `src\` 直接拷贝）
