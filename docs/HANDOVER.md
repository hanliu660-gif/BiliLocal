# BiliLocal 64 位自建版 · 交接文档

> 更新时间：2026-10-25（**第十二版：交互打磨+设置审计轮（归档版）**；同日校正版次哈希、目录表与工具数）——`master` = 最新提交（`git log -1 --oneline`，2026-10-25 05:30 之后全是纯文档提交）；**最后一个代码提交 = `81aab8f`**（Danmaku 背景角色穿透修复）← `a9fbcb3`（悬停冻结持久化）← `63f9a4b`（设置审计）← 8d9c8ec（悬停冻结+发送者数据）← 44fbb3e（底栏目录记忆补修）← 7f8685a（瘦身）← 473ba62（Win11 UI）← `3f86f74`（VLC 3.0.24）← `3155020`（单击暂停+扁平QSS+输入路由修复）← `c834050`（event(Init) 全限定名修复）← `cbc0b42`（启动续播+目录记忆）← `c8679e6`（现代化轮：主题/图标/面板/测试）← `d7847e8`（清理）← `49c173c`（单平台化+i18n）← `4702ab1`；已推送 `github.com/hanliu660-gif/BiliLocal`（remote 名 `github`），exe 1,093,120 → 948,224 → 964,608 → 925,696 → 943,616 字节；dist 358M → 176M（2026-10-25 裁定不再瘦身，`lua\` 与 `opengl32sw.dll` 保留）。
> 写给新会话：先读这份，再看 `src\` 的 git 历史——**每个提交都有根因和证据**。
> 一句话：把开源播放器 BiliLocal 0.4.2（GPL-3.0，2016 年停更）改造成 64 位私人 Windows 播放器：**VLC 3 + d3d11 硬解直出（Embed 唯一输出）+ 单底栏 UI**，日常播服务器录播 + 弹幕，CPU 从 ~210% 降到 ~11%。
>
> **路径基准（2026-10-24 修订）**：本文是工程级文档，**正本在工程根 `C:\DSH\APP\BiliLocal\交接文档.md`**；git 仓库（`src\`，即 GitHub 仓库根）内的 `src\docs\HANDOVER.md` 是它的同步备份副本——**更新正本后 `cp 交接文档.md src\docs\HANDOVER.md`，随仓库提交推送**，两份内容必须保持一致。下表 `build\dist\deps\tools\test` 等目录在仓库外、不入 git。
>
> **三份文档的分工（写新内容前先对号）**：判据一句话——**这句话换台机器/换个人还成立吗？**
> - 成立，且是“项目是什么/有什么功能/怎么构建/许可” → `src\README.md`（对外名片，英文）
> - 不成立（本机路径、用户约束、当前状态哈希、验证钩子、坑、待办、回滚） → 本文
> - 只做路由不承载内容 → 工程根 `README.md` 桩
> - 产品用法（功能/快捷键/续播）属前者，验证/排查手法属本文

---

## 0. 用户约束（必须先看）

- **改代码/改行为前先问一句**（他明确要求过）。
- **不要抢焦点**：不要移动鼠标、不要抢前台。验证靠「投递消息 / 截图 / 读日志 / 改配置重启」，不要用真实光标。
- 要**实测证据**（截图、日志原文、UIA 读数），不要"应该可以"。
- **功能/交互的验收测试交给用户本人做**（他明确要求）：自动化只负责日志取证与时序验证，别为 UI 行为搭复杂的模拟点击链路。
- 路径别写死在自己脚本里，工程可能被搬。

## 1. 目录与关键路径

| 路径 | 内容 |
| --- | --- |
| `src\` | 源码 git 仓库（全部工作在 `master`）；tag `v0.5.0-vmem` = 早期 vmem 线回滚点 |
| `src\docs\HANDOVER.md` | 交接手册的仓库备份副本（正本在工程根 `交接文档.md`，更新后同步过来） |
| `build\` | 影子构建（qmake + mingw32-make），产物 `build\release\BiliLocal.exe` |
| `build-test\` | 单测构建树（`src\test\tst_parse.pro` 的产物，只链 Parse 与 Utils 两个 TU）。跑 `release\tst_parse.exe` 前要把 `deps\5.15.2\mingw81_64\bin` 与 `deps\Tools\mingw810_64\bin` 放进 PATH，否则缺 Qt5Test/Core |
| `dist\` | **可直接运行的成品**：exe + Qt5 + VLC 3.0.24（FFmpeg 8.1.2）+ 插件 + 翻译（Embed 硬解直出版） |
| `deps\` | 工具链：Qt 5.15.2 (mingw81_64)、MinGW 8.1.0、VLC SDK 头文件/导入库（在 `deps\thirdparty`；旧运行库目录 deps\vlc 与 3.0.21 包已删，VLC 运行库只存在于 dist；`deps\dl` 仅剩 vlc-2.2.8-win64.zip（vmem 考古用）） |
| ~~`backup\`~~ | 已删除（2026-10-24 用户决定）：git 历史就存在 `src\.git` + GitHub 远端里，不再保留离线 bundle |
| `tools\` | 调试/测试脚本（13 个，2026-10-25 收编 3 个对话框验证脚本，见第 8 节） |
| `test\` | `embed.xml` = 测试弹幕 fixture；`rec.xml` + `rec.flv` = 指向 Z: 真实录播的**符号链接**（Z: 未挂载时这两个链接悬空，单测不依赖它们） |
| `%LOCALAPPDATA%\BiliLocal\Config.txt` | 运行时配置（JSON） |
| `%LOCALAPPDATA%\BiliLocal\cache\msg.log` | 日志（超 10MB 自动轮转成 .old） |

> 2026-10-24 清理：`dist-vlc2\`（旧 vmem 线）、`dumps\`（4.9G WER 转储）、`build-dbg/fp\`（旧构建树）、`embed-wt\` worktree、`BiliLocal-64bit.patch`（过时补丁）已删除——回滚材料只在 git 历史里（`backup\` 本体也已删,别再找它）。
> 2026-10-25 再清一次：`dumps\` 又积了 3 个 dump（00:14/00:20/00:21 各一个，共 1.9G，交互轮排查期残留）连同目录一并删除。**WER 转储默认写这里，排查完记得清**；别让它横向长到几个 G。

## 2. 当前状态

- **exe**：`dist\BiliLocal.exe`，943,616 字节，SHA256 `E9A9967D…`（构建自 `81aab8f`，2026-10-25 06:14 部署；**构建确定性已两证**：①与 `build\release\BiliLocal.exe` 逐字节相同；②同一份源码在空目录重编，产物只差 4 个字节——PE 头的时间戳/校验和，代码段完全一致 ⇒ dist 与源码一一对应，没有"改了忘重编"的死角）
- **源码**：`master` = 最新提交（**从 2026-10-25 05:30 起改为不写死文档哈希**——写死的那个值在提交的瞬间就过期了，当前值请用 `git log -1 --oneline` 取）；**最后一个代码提交 = `81aab8f`**（Danmaku 背景角色穿透修复）← `a9fbcb3`（悬停冻结持久化）← `63f9a4b`（设置审计）← `8d9c8ec`（悬停冻结+发送者数据）← `44fbb3e`（底栏目录记忆补修）← `7f8685a`（瘦身轮）← `473ba62`（Win11 UI 轮）← `3f86f74`（VLC 3.0.24）← `3155020` ← `c834050` ← `cbc0b42` ← `c8679e6` ← `d7847e8` ← `49c173c` ← `4702ab1`；上游 = `9d1cb05`；回滚点 tag `v0.5.0-vmem` = `46b68b9`
- **运行库**：VLC **3.0.24**（FFmpeg 8.1.2，d3d11 直出；升级缘由见提交 3f86f74）；**dist\plugins 混住着 Qt 平台插件（platforms/imageformats/mediaservice/styles），整目录替换 VLC 时必须把这四个目录合并到顶层，否则 Qt 起不来**（升级/裁剪流程见第 9 节）；dist 只带 exe 实际导入的 5 个 Qt DLL（Concurrent/Core/Gui/Network/Widgets）
- **配置**：`Output=Embed` 已成唯一输出（键保留但代码不再读取）；`Immediate=True`；`HardwareDecode` 已无消费者（d3d11 固定开启，Arguments 手写 `--avcodec-hw=`/`--vout=` 仍可覆盖）
- **i18n**：源码全英文串，中文走 `dist\locale\zh_CN\bililocal.qm`（187/187 全译，含繁体）；改 UI 文案后记得 lupdate→补译→lrelease→部署 qm
- **备份**：git 历史 = 本地 `src\.git` + GitHub fork 双保险（离线 bundle 与 Z: 副本已按用户决定删除）；vmem 旧线源码完整保留在 tag `v0.5.0-vmem`，重编需自行从 VideoLAN 存档获取 VLC 2.2.8

### 2.0 上一会话纪要（2026-10-25 归档,写给接棒的会话）

- **已交付并推送（全部在 github/master 上，最后一个代码提交是 `81aab8f`）**：悬停冻结单条弹幕(静止持久)/右键弹幕数据块(全文/用户名+哈希/时间/位置/真实同源计数)/屏蔽发送者按 hash 真实生效/屏蔽页双列重构/弹幕专属右键菜单/菜单对齐与文案翻转/设置审计轮(见 §2.1 沿革)/文件面板「清空」
- **明确放弃,勿重做**：底栏「弹幕·开」按钮的右键菜单(a4b5acf)——三层窗口结构盲区,详见 §2.1 与 §6 23-25
- **未决**：无（2026-10-25 裁定：dist 不再瘦身、Z: 长时段观察结案，见 §10 末尾）
- **验证分工（用户明确要求）**：功能/交互的验收测试交给用户本人做,自动化只负责日志与时序取证;最后两轮(菜单对齐+设置审计)为自测通过,未经用户逐项验收
- 单测 9/9（2026-10-25 复跑 `build-test\release\tst_parse.exe` 确认：7 用例 + init/cleanup = 9 passed）；i18n 187/187（`zh_CN.ts` 194 条 message − 7 条 vanished，0 unfinished）
- **编译警告：42 → 3**（2026-10-25 连清两轮：`a5f95fe` 清 6 个位置、`81aab8f` 清最后 1 个）：已清 7 个位置——`Config.h:44` 的 `fromJsonValue<QVariant>` defined-but-unused **29 条**（匿名命名空间里的头文件模板，每个 TU 各得一份；同文件 QJsonArray/QJsonObject 两个特化早有 `[[maybe_unused]]`，QVariant 那个漏了，补一行全清）、`endl` 3 处（Prefer.cpp:668/670/673，`QTextStreamFunctions::endl`→`Qt::endl`）、`QWheelEvent::globalPos` 3 处（Home.cpp:695/700 → `globalPosition().toPoint()`；EmbedPrivate.cpp:166 → `globalPosition()`）、`Danmaku.cpp:114` 的 fall-through 1 条（内层 switch 只认模式 7/8 就穿透到 `case ModeRole`，普通弹幕取 `BackgroundRole` 拿到的是 mode 整数——**实测这不是你能看见的 bug**：`QVariant(int).canConvert<QBrush>()==false`、`QColor→QBrush==true`，视图把整数当「无背景」直接忽略；但它确实违反角色契约，现在显式 `return QVariant()`）。**剩下 3 条是刻意留的**：`-Wimplicit-fallthrough` 1 条（`Running.cpp:276` 的 `case 1` 落到 `case 0`，入队本来就该两模式同路）、`libvlc_video_set_subtitle_file` 2 条（VPlayer.cpp:462，VLC 3 标弃用但照常可用，换 VLC 4 才必须改）

### 2.1 版本沿革

每轮的完整根因与证据都在对应 git 提交信息里（本仓库的纪律），此处只留索引，**别在文档里重复维护细节**（会腐化）：

| 提交 | 主题 |
| --- | --- |
| `46b68b9` | vmem 线回滚点 tag `v0.5.0-vmem`（VLC 2.2.8，考古用） |
| `5c33397`…`82a3d13` | Embed 直出重写（d3d11 原生子窗 + GL 覆盖层路由输入）+ 现代快捷键 |
| `4702ab1` | 单底栏 UI；死在线功能移除 |
| `49c173c` / `d7847e8` | 死代码大清理 + 单平台化（vmem/跨平台/在线路径全删） |
| `c8679e6` | 现代化：主题/图标/面板/QtTest；顺手修上游 ASS 颜色 bug |
| `cbc0b42` / `c834050` | 启动续播 + 目录记忆；enum class 槽全限定修复 |
| `3155020` / `3f86f74` | 单击暂停 + 输入路由修复；VLC 3.0.24（130+ 安全修复） |
| `32c6597`…`473ba62` | Win11 设计语言轮（双主题 QSS、Prefer 重排 6→5 页、BILILOCAL_UI_TEST 钩子） |
| `0c4f7b7`…`7f8685a` | 瘦身轮（dist 358M→176M、插件机制+网络栈移除、Load 本地化、List 扫描合并） |
| `44fbb3e` | 底栏打开接入 /Interface/OpenDir（cbc0b42 只接了 Menu 面板，底栏重启即失忆） |
| `54e718c`…`8d9c8ec` | 交互轮:悬停冻结单条弹幕(状态保持,静止不解冻)+右键弹幕数据块(全文换行可选中/脱敏用户名+user_hash/发送时间/位置/真实同源计数)+屏蔽发送者按 hash 真实生效;录播姬毫秒时间戳修复(2038 bug);测试钩子循环化 |
| （交互轮打磨） | 屏蔽页双列重构(去 下拉选类型/共享输入框/焦点决定删除 三段式);弹幕专属右键菜单(点弹幕只留 信息/复制/屏蔽,通用功能只在空白处右键);菜单快捷键列按 sizeHint+最长快捷键加宽(修 播放列表/Ctrl+L 重叠);开关行文案翻转(打开/关闭弹幕);菜单勾选与底栏文案统一以 Shield 屏蔽库为真值源;音频菜单移除 提高音量/降低音量 |
| a4b5acf | **放弃底栏弹幕右键菜单**(用户决定,勿重做):进程内事件合成+菜单弹出全通,但真实点击始终不出现——覆盖层/原生窗/弹层三层焦点与显示盲区;相关坑见 §6 23-25 |
| （设置审计轮） | 全库配置键交叉审计:删死项(版本信息复选框/无框窗口+Home 拖窗残留)、补漏(播放页「播放行为」卡=断点续播/自动连播/循环/跳转间隔挪入;弹幕外观加轨道间距 /Danmaku/Grating)、Shield 类型加 tooltip、关于页加构建信息卡(日期/Qt/libVLC)、文件面板加「清空」(只清播放日期,文件/断点/绑定保留)。注意:「防挡字幕」就是 /Danmaku/Protect,面板已有入口故未重复 |
| `a5f95fe` | 警告清理 42→4：`Config.h` 的 `QVariant` 特化补 `[[maybe_unused]]`(清掉 29 条 defined-but-unused)、`endl`→`Qt::endl`(3 处)、`globalPos()`→`globalPosition()`(3 处)；余 4 条(fallthrough ×2、VLC 3 弃用 API ×2)刻意保留，理由写在 §2.0 |
| `81aab8f` | Danmaku 的 `BackgroundRole` 不再穿透到 `ModeRole`（普通弹幕曾把 mode 整数当背景刷返回；Qt 不认 int→QBrush，所以没有可见变化，属契约修复），警告 4→3 |

## 3. 怎么用（用户视角）

产品功能、快捷键、断点续播、自动续播、目录记忆等说明已整体移至 `src\README.md` 的 **Usage** 节——那是“换台机器也成立”的产品事实，正本在对外名片里，本节不再重复维护。只留一条验证相关的注意：

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

- **架构**：`Local` 服务定位器，`lApp->findObject<T>()` 取 9 个模块（Config/Shield/Danmaku/Running/Interface/APlayer/ARender/Load/List）
- **播放**：`VPlayer`（libvlc，唯一后端）。视频经 `set_hwnd` 直出到原生子窗口 `ESurface`（d3d11va 硬解 + direct3d11 vout，参数在 VPlayer 构造时固定注入，可被 Arguments 手写覆盖）
- **弹幕**：`Load`（仅本地文件）→ `Parse::parseComment`（**同步**，直接返回结果）→ `Danmaku` 模型 → `Running`（池线程布局防碰撞）→ `EWindow`（透明 GL 覆盖层，兼作输入面）绘制；弹幕文字精灵走 `ASprite`/`SyncTextureSprite`/`OpenGLAtlas` 图集
- **UI**：`Home`（主窗，快捷键/全屏/seek 横带）+ `Info`（单底栏）+ `Menu`（文件与弹幕悬浮面板）+ `Editor`（弹幕池编辑）+ `Prefer`（设置，5 页：播放/界面/屏蔽/快捷/关于）
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
16. **enum class 槽参数必须全限定**：moc 记录的是声明原文（Event），跨线程 invokeMethod 的 Q_ARG(APlayer::Event,...) 给的是全限定名——不匹配时**静默失败**（无警告、返回值没人查），症状是 state 永远 Stop（拖不动/无时长/标题不变/List 存 Time=-1）。修法：槽签名写全 event(APlayer::Event)。另：**图像分析验证 UI 数字不可靠**（会迎合 prompt 里的示例数字），时长/进度这类断言要用行为证据（标题、配置断点、日志）
17. （教训）invokeMethod 返回值从不检查——关键投递点要么查返回值，要么改信号转发
18. **childAt 无视 WA_TransparentForMouseEvents**：EWindow::route 转发点击时 childAt 命中透明 ESurface → 事件进黑洞（视频区单击/双击/拖动全失灵）。转发前要沿 parentWidget 跳过穿透控件
19. **QWidget::mouseDoubleClickEvent 默认转发 mousePressEvent**：重写双击后调基类 = 双击又额外触发一遍单击逻辑——不需要基类行为就别调。（历史路线 Win32 子类化转发鼠标消息已整体废弃：投递消息不达 vout proc + VLC 重建窗口后钩子悬空崩溃 → 改覆盖层路由输入）
20. **invokeMethod 按名调用要求目标是槽**：普通私有方法会静默失败（只有 "No such method" 打进日志才看得见）；Load::processNext 即例，修法 `private slots:`
21. **Danmaku::append 按 source 路径去重**：同一 xml 传两遍造不出 pool=2——测 Editor 自动弹出要用两个不同路径的文件
22. **队列消费的 dequeue 时机**：任务在处理函数开头取出；尾部残留 dequeue 会吞掉下一个任务（Load 重写踩过，靠 `Load: processed` 日志定位）
23. **包 if 时检查作用域内栈对象的寿命是否覆盖 exec**：弹幕专属菜单把四个子菜单的栈 QMenu 关进了 if 作用域,exec 在作用域外——子菜单先析构,菜单项凭空消失(视频/音频/字幕/弹幕集体隐身)。声明提函数作用域,更深层的子菜单用堆分配
24. **覆盖层吞 WM_CONTEXTMENU,且 QWindow 系的 Leave/ContextMenu 都走 event() 虚函数**：右键菜单事件落在最上层的 EWindow 头上,不转发则底层按钮的 customContextMenuRequested 永不触发;leaveEvent/contextMenuEvent 这些 QWidget 名字在 QOpenGLWindow 上不存在(override 编译报错),必须在 event() 里按类型分发
25. **childAt 不保证递归到叶子**：右键转发只命中 Info 容器本体而非内部按钮——要沿 childAt 一路下沉到最深子控件,再沿 parentWidget 跳过穿透控件(与坑 18 同源不同面:18 是点击路由,这里是菜单路由)
26. **QSS 菜单里别用 checkable 行**：勾选列在 QSS 内边距下失控——✓ 吊在左边距、文字多缩进 28px(像素实测 x=47 vs 75);用普通动作+文案随状态翻转(打开弹幕/关闭弹幕)表达,文字与其他行像素级对齐
27. **录播姬 raw 属性里的 JSON 是字符串套字符串**：内层引号写作 \\&quot; 带反斜杠——按 &quot; 字面量搜索永不命中;且合成单测样本若手写漏了反斜杠会**假绿**。用正则容忍可选反斜杠,并拿真实弹幕字节做单测
28. **MSYS2 的 `sed -i` 会把 CRLF 悄悄变成 LF**（Windows 文本模式写回）：改这份文档这类 CRLF 文件时，要么用编辑工具，要么 `sed -i` 之后立刻 `file` 检查换行符再补 CR。源码走 `core.autocrlf=true` 会被归一，无所谓；但工程根的 `.md` **不在 git 里**（仓库是 `src\`），被改成 LF 没人替你归一。

29. **编辑工具会吃掉 UTF-8 BOM**：`Danmaku.cpp` 只改一行，文件头的 BOM 就没了，而 `tools\fix-bom.ps1` 在当前 ExecutionPolicy 下跑不起来。手补：`printf '\xef\xbb\xbf' > t && cat t f > f2 && mv f2 f`；补完用 `git ls-files` 全量比对每个文件的前三个字节与 HEAD blob（本次审计只发现 `.gitignore` 的 CRLF/LF 差异，无 BOM 失配）。

## 7. 已知限制（不是待办，是设计取舍）

1. 大 xml（数万条）解析在 GUI 线程同步执行，可能卡顿数百 ms~秒级（Parse 同步化后引入；本地录播 xml 无感，如需要可再做异步，但要用可靠机制——别回到 QFutureWatcher；注意 Load 的 onFinish 回调里 `&task` 引用，异步化时会悬空）
2. 倍速按钮长按右键的 350ms 阈值/3.0 倍率是拍脑袋值，可按手感调

## 8. 工具脚本（`tools\`，2026-10-25 归档时 13 个）

| 脚本 | 用途 |
| --- | --- |
| `setcfg.py <key> <value>` | 改 Config.txt 任意键（布尔/数字/字符串自动识别） |
| `pwc.ps1` | **PrintWindow(PW_RENDERFULLCONTENT) 抓窗口**——抓视频/底栏用这个 |
| `shot.ps1` | BitBlt 抓屏（D3D/GL 层会黑，仅参考） |
| `perfrun.ps1` | 起播放器采样 CPU（`-Media` 指定本地文件、`-Exe` 指定 exe） |
| `kids.ps1` / `embed-probe.ps1` | 列窗口树 / 窗口树+父窗口+类名 |
| `dense.py` | 解析**指定**录播 xml 求密集窗口（注意：脚本内路径写死，需改源码指路） |
| `fix-bom.ps1` | 源文件 BOM 对齐 git HEAD |
| `verify_ui3.ps1` | **对话框巡览验证**：配合 `BILILOCAL_UI_TEST=both` 钩子，按窗口标题抓 Prefer 五页+Editor（输出双主题截图组） |
| `dumpwins.ps1` / `uia_name.ps1` | 列进程全部窗口（标题转码点列）/ 用 UIA Name 读窗口标题与矩形（GetWindowTextW 对 Qt 标题不可靠） |
| `verify_hover.ps1` / `verify_submenu.ps1` | 悬停冻结与右键子菜单的外部验证：消息投递 + UIA 读项 + CopyFromScreen 截图（QMenu 对 PrintWindow 不透明） |

（历史上几十个 `fs*/popcheck*/stuckslide*` 等一次性排查脚本已删；tools\ 从未入 git、离线备份也已删，**不可找回**。）

**进程内自动化验证钩子**（比外部投递消息可靠——后者会被 Qt 按真实光标位置路由而失准）：
- `BILILOCAL_EMBED_TEST=1`：EWindow 每 300ms 注入鼠标事件（第 30/50 拍双击验证全屏闭环、第 35 拍单击验证暂停），配 `msg.log` 判读
- `BILILOCAL_UI_TEST=prefer/editor/both`：自动打开 Prefer（逐页巡览，标题带页码「设置 - N」）与 Editor——**验证对话框用这个**，配 `tools\verify_ui3.ps1`
- 悬停验证：钩子每 60 拍循环「扫掠→(600,320) 压 14 拍(验证静止不解冻)→第 56 拍右键」，起播后自动跳 10%（片头弹幕密）；旧闭环拍(双击全屏/单击暂停)需另加 `BILILOCAL_EMBED_CLICK=1` 才启用；`msg.log` 找 `hover: freeze/release` 判读

**PS 5.1 铁律**：一律 `-NoProfile -ExecutionPolicy Bypass -File` 调用；脚本存 UTF-8 带 BOM、**不写中文字面量**；`$Pid` 只读、`Move/Cli/lp` 是别名；`Start-Process -RedirectStandardError` 别配 `-Wait`；子进程会继承 stdout 管道（给 `UseShellExecute=$false` 的启动加 stdio 重定向，否则调用方假死）。

## 9. 环境与依赖

- Qt **5.15.2** mingw81_64：`deps\5.15.2\mingw81_64`
- MinGW **8.1.0**：`deps\Tools\mingw810_64\bin`
- VLC **3.0.24**：运行库在 `dist`（升级方法：官网下 win64 安装包静默解包 /S，替换 dist 的 libvlc/libvlccore/plugins（lua\ 与 opengl32sw.dll 本播放器用不到，已实测可去，2026-10-25 决定保留，升级时按原样回填）,**必须把 plugins 里 Qt 的 platforms/imageformats/mediaservice/styles 四目录合并到顶层**，再按「先验证再删」裁掉 gui/stream_out/access_output 等对 libvlc 无用的类别（10-25 那次升级就是漏了验证、把新树放成嵌套 plugins\plugins 才多背 142M：替换后先改名/移动留证，启动播放确认无恙再删））；构建用头文件/导入库在 `deps\thirdparty`（3.0.21 SDK，libvlc API 兼容）
- Python：`C:\Users\23239\.dsh\dsh-runtimes\dsh-next-primary-runtime\dependencies\python\python.exe`
- VC 运行库（VLC3 需要）：系统已装 vcruntime140/140_1/msvcp140——**dist 未自带**,换机器先装 [vc_redist.x64](https://aka.ms/vs/17/release/vc_redist.x64.exe),或把三个 DLL 拷进 dist（支持应用目录部署）
- 网络盘：`Z:` = RaiDrive 挂载 WebDAV，读速 0.1–45 MB/s 抖动；读慢时播放器"未响应"属挂载问题

## 10. 下一步建议

1. 大 xml 异步解析（仅当某天录播 xml 大到可感知卡顿再做；别用 QFutureWatcher）
2. 渲染层（EmbedPrivate/ARender/OpenGL）**已稳定，别动**

> 2026-10-25 裁定并结案两条：**dist 不再瘦身**（`lua\` 1.1M + `opengl32sw.dll` 21M 实测可去，决定保留——升级 VLC 时按原样回填即可）；**Z: 长时段观察不再挂账**（挂载抖动属 RaiDrive 链路问题，日常播到不对劲再查，见 §9）。

## 附：快速回滚

- **源码回滚**：`git checkout v0.5.0-vmem` → 重编（回到最初的 vmem 线）
- ~~旧线运行时~~：VLC 2.2.8 运行库与免编译 vmem exe 已于 2026-10-24 删除。真要跑 vmem 线：从 tag 重编 exe + 自行从 VideoLAN 官方存档（historical releases）下载 VLC 2.2.8 win64 + 配置改 `Output=Window`。注意：vmem 线是已知崩溃/高 CPU 路线，仅作考古用途
- **全历史恢复**：`git clone https://github.com/hanliu660-gif/BiliLocal.git <目录>`（或从本地 `src\` 直接拷贝）
