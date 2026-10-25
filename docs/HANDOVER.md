# BiliLocal 64 位自建版 · 交接文档

> **更新时间**：2026-10-26（**第二十三版：Qt6 迁移结案（tag `v0.7.0-qt6`）**——Phase 3 复验通过后完成收尾：dist 对调（`dist\` 现为 Qt 6.8.3 线，`dist-qt5\` 原样归档作回滚位）、Qt5 杂项裁撤（MinGW 三件套/d3dcompiler_47/旧 qt.qm，pefile 全插件导入表实证无依赖，换 Qt6 自带 `qt_zh_CN/zh_TW.qm`）、死树清空（只余 `build-cmake6\`）、hook-gate 复验 PASS。**迁移全程：Phase 1 MSVC 工具链 → Phase 2 Qt 6.8.3 → Phase 3 用户验收（功能全过 + 坑 55 性能修复）→ 结案，7 个会话日内完成**。剩余挂账见 §10 尾部））
>
> **这份文档写给谁**：写给"下一会话接手这个工程的人/AI"。先读 §0 约束，再看 §2 当前状态，动手前看 §6 坑。
>
> **一句话**：把开源播放器 BiliLocal 0.4.2（GPL-3.0，2016 年停更）改造成 64 位私人 Windows 播放器——**VLC 3 + d3d11 硬解直出（Embed 唯一输出）+ 单底栏 UI**，日常播服务器录播 + 弹幕，CPU 从 ~210% 降到 ~11%。

**路径基准**：本文是工程级文档，**正本在工程根 `C:\DSH\APP\BiliLocal\交接文档.md`**；git 仓库（在 `src\`，即 GitHub 仓库根）里的 `src\docs\HANDOVER.md` 是它的同步备份副本——**更新完正本后 `cp 交接文档.md src/docs/HANDOVER.md`，随仓库一起提交推送**，两份必须一致。`build\ dist\ deps\ tools\ test\` 都在仓库外、不入 git。

**三份文档的分工**（写新内容前先对号，判据一句话：**这句话换台机器/换个人还成立吗？**）

| 文档 | 装什么 |
| --- | --- |
| `src\README.md`（英文） | 成立：项目是什么、有什么功能、怎么构建、许可 |
| 本文 | 不成立：本机路径、用户约束、当前状态哈希、验证钩子、坑、已知限制、回滚 |
| 工程根 `README.md` | 只做路由，不承载内容 |

---

## 0. 用户约束（动手前必须看）

- **改代码/改行为前先问一句**（他明确要求过）。批准了再动手，改完给实测证据。
- **不要抢焦点**：不移动鼠标、不抢前台。验证靠「投递消息 / 截图 / 读日志 / 改配置重启」，不用真实光标。
- **要实测证据**（截图、日志原文、UIA 读数、SHA256），不要"应该可以"。
- **功能/交互的验收测试交给用户本人做**：自动化只负责日志取证与时序验证，别为 UI 行为搭模拟点击链路。
- **路径别写死**：工程会被搬，脚本里用相对路径或参数。
- **说明文档的位置与其中的绝对路径，改之前先问**：`交接文档.md`、`src\docs\HANDOVER.md`、`APP\AGENTS.md` 这类文档放在哪个目录、叫什么名字、里面写的绝对路径，都属"先问再动"。内容修订可以直接做。
- **进度落盘**：重要状态写进这份文档，别只留在会话里（他按上下文付费）。

---

## 1. 目录与关键路径

| 路径 | 内容 |
| --- | --- |
| `src\` | 源码 git 仓库（全部工作在 `master`）；tag `v0.5.0-vmem` = 早期 vmem 线回滚点 |
| `src\docs\HANDOVER.md` | 本文的仓库备份副本（正本在工程根，更新后同步过来） |
| `build\` | ~~qmake 影子构建~~ **已删（Phase 2）**；`build-test*` 六个 qmake 测试树同批删除 |
| `build-cmake\` / `build-cmake-msvc\` | ~~CMake 死线（MinGW/Qt5 与 MSVC/Qt5）~~ **已删（迁移结案清理，回收 578M）**；回滚重编走 tag + deps 的 Qt5 工具链 |
| `build-cmake6\` | **CMake 影子构建（现役唯一）**：Qt 6.8.3 msvc2022_64 + VS 生成器 → `Release\BiliLocal.exe` + 5 个测试 exe + `zh_CN.qm`/`zh_TW.qm`；构建命令见 §4 |
| `dist-qt5\` | **Qt5/MinGW 线整树归档（回滚位）**：最后成品 956,928 B / `6C55F692…`；源码锚 `v0.6.0-qt5-final`。要回 Qt5：把现 dist 改名让位、本树改回 dist\ 即用 |
| `src\test\*.cpp` | 5 个测试目标 41 例（parse 9 / config 7 / shield 9 / load 12 / list 4），构建与运行全走 `build-cmake6` + ctest（`tools\run-tests.ps1` 一键；负向对照与测试约束的说明在 §2/§8） |
| `deps\archive\` | 依赖归档：`vlc-sdk-3.0.24-win64.zip`（= `deps\thirdparty` 现快照，SHA256 `7BD12CD7…`；只有头文件+再生 MinGW 库，**无官方 .lib**）+ 旧 `vlc-sdk-3.0.21-win64.zip`（SHA256 `857CE77B…`）；来源与再生途径见 §9 |
| `dist\` | **可直接运行的成品（日常在用，Qt6 线）**：Qt 6.8.3 exe + VLC 3.0.24（FFmpeg 8.1.2）+ 插件 + 翻译。旧 Qt5 线在 `dist-qt5\` 归档 |
| `deps\` | 工具链：Qt 5.15.2（mingw81_64 + msvc2019_64，遗留）、**Qt 6.8.3（msvc2022_64，现役）**、MinGW 8.1.0（遗留）、VLC SDK 头文件/导入库（在 `deps\thirdparty`）。VLC 运行库只存在于 dist；`deps\dl` 仅剩 `vlc-2.2.8-win64.zip`（vmem 考古用） |
| `tools\` | 调试/测试脚本（16 个，见 §8） |
| `test\` | 已清空（2026-10-25 清点：`embed.xml` 与 `rec.*` 符号链接均已不存在）。hook-gate 的弹幕 fixture 与本地视频由 `tools\gen-media.py` 按需生成到 `tools\.media\`（不入 git，见 §8） |
| `%LOCALAPPDATA%\BiliLocal\Config.txt` | 运行时配置（JSON） |
| `%LOCALAPPDATA%\BiliLocal\cache\msg.log` | 日志（超 10MB 自动轮转成 `.old`） |

> **`dumps\`（WER 转储）会自己长回来**：2026-10-24 清过一次 4.9G，10-25 又清两次（3 个 1.9G、2 个 1021M），**每崩一次写一份 ~0.5G**。排查完立刻删；目录存在不代表当前 exe 有毛病（那些 dump 都来自部署新 exe 之前的旧进程）。
>
> 2026-10-24 还删了：`dist-vlc2\`（旧 vmem 线）、`build-dbg/fp\`（旧构建树）、`embed-wt\` worktree、`BiliLocal-64bit.patch`、离线 bundle（`backup\`）。**回滚材料只在 git 历史 + GitHub 远端里，别再找这些目录。**

---

## 2. 当前状态（2026-10-26 实测）

**迁移状态（✅ 2026-10-26 结案，tag `v0.7.0-qt6`）**：

- **源码**：`master` = 最新提交（文档 23 版）；最后代码提交 = `8f15d86`（坑 55 修复）← `d5b6383`（文档清扫）← `8bb173f`（**Phase 2**：Qt 6.8.3）← `599dc95`（Phase 1 结案）← `f81e6ee`（Phase 1：MSVC 工具链）← `74a7569`（文档 19 版）。远端已同步。
- **现役成品**：`dist\BiliLocal.exe` **978,944 B / SHA256 `f6b73df1…`**（Qt 6.8.3 msvc2022_64，构建树 `build-cmake6\`）——**这就是日常在用的 dist**。对调时裁掉 Qt5 杂项：MinGW 三件套（libgcc/libstdc++/libwinpthread）、`d3dcompiler_47.dll`、Qt5 时代 `qt.qm`——pefile 扫 libvlc/libvlccore/全部插件的导入表实证零依赖；换上 Qt6 自带 `qt_zh_CN.qm`/`qt_zh_TW.qm`（QFileDialog 等 Qt 内建文案的翻译）。hook-gate 对调后复验 PASS。
- **回滚位**：`dist-qt5\`（Qt5/MinGW 线最后成品整树归档，956,928 B / `6C55F692…`）；源码锚 tag `v0.6.0-qt5-final`；deps 的 Qt5/MinGW 工具链（约 1.4G）按"默认倾向留"保留，要裁再说一声。
- **验收记录**：Phase 3 功能清单全过；性能回归（反应变慢 + 关闭长卡顿）定位与修复见坑 55；千夏复验通过。
- **构建树收敛**：只余 `build-cmake6\`（现役）。qmake 的 `build\` + 6 个测试树（Phase 2 删）、CMake 死线 `build-cmake\`（382M）/`build-cmake-msvc\`（196M）（本次删）全部清空。
- **CI**：单 job `unit` = Qt 6.8.3 msvc2022_64 + qt5compat（缓存 key `qt-6.8.3-msvc2022-v1`）；`hook-gate.ps1`/`perfrun.ps1` 的默认 exe 路径已随对调自动指向 Qt6 版（都用 `dist\`）。




- **exe**：现行 `dist\BiliLocal.exe` = **978,944 B / `f6b73df1…`**（见上）；归档线 `dist-qt5\BiliLocal.exe` = 956,928 B / `6C55F692…`。**构建可复现性结论**（Qt5 线 2026-10-25 三组实测，原则仍适用，细节见坑 42）：尺寸可复现、字节不完全可复现——**判断"是不是同一份源码"只能看 git 提交号，别比哈希**。
- **上游与远端**：上游 `9d1cb05`；回滚 tag `v0.5.0-vmem` = `46b68b9`；远端 `github` = `github.com/hanliu660-gif/BiliLocal`。完整沿革见 §2.1。
- **单测**：五个目标共 **41 例全绿**（`build-cmake6` + ctest，CI 同配方）：`tst_parse` **9**、`tst_config` **7**、`tst_shield` **9**、`tst_load` **12**、`tst_list` **4**。**负向对照做过三个**（证明用例不是空跑）：①去掉 `tst_config` 的 `QWriteLocker` → `concurrentWriters` SIGSEGV；②去掉 `Load.cpp` 嗅探的 `.trimmed()` → `blankLineBeforeXml` 变红；③把 List 去重短路成 `if (false)` → `tst_list` 双红。GUI 钩子剧本见 `tools\hook-gate.ps1`。Danmaku 本体仍因依赖整套服务定位器暂不独立成测（§10 尾部挂账）。
- **编译警告**：**MSVC 现役基线 0**（Phase 1 起实测始终 0）。Qt5/MinGW 时代 42→3（3 条刻意保留：Running.cpp fallthrough、VPlayer 的 libvlc deprecation ×2），该基线随 dist-qt5 归档；换 VLC 4 时那 2 条会变硬错误（§10-4）。
- **i18n**：源码全英文串，中文走 `locale\zh_CN\bililocal.qm`（187/187 全译，含繁体）+ Qt 内建文案 `qt_zh_CN.qm`/`qt_zh_TW.qm`（Qt6 官方，QFileDialog 等）。改 UI 文案后：lupdate 补 `.ts` → 重新构建（CMake 的 `translations ALL` 目标自动 lrelease 出 `build-cmake6\zh_CN.qm`/`zh_TW.qm`）→ 拷进 dist 树 `locale\<lang>\bililocal.qm`。
- **运行库**：VLC **3.0.24**（d3d11 直出）；`dist\plugins` 里混住着 Qt 的**三个**插件目录（`platforms\` `imageformats\` `styles\`；mediaservice 随 Qt6 部署取消），整目录替换 VLC 时必须把它们合并到顶层，否则 Qt 起不来（流程见 §9）。dist 只带 exe 实际导入的 **8 个 Qt6 DLL**（Concurrent/Core/Core5Compat/Gui/Network/OpenGL/OpenGLWidgets/Widgets）。
- **配置**：`Output=Embed` 已是唯一输出（键保留但代码不再读）；`Immediate=True`；`HardwareDecode` 已无消费者（d3d11 固定开启，`Arguments` 手写 `--avcodec-hw=`/`--vout=` 仍可覆盖）。
- **备份**：git 历史 = 本地 `src\.git` + GitHub 双保险；锚点 tag：`v0.7.0-qt6`（迁移结案）/ `v0.6.0-qt5-final`（Qt5 末态）/ `v0.5.0-vmem`（考古，重编需自备 VLC 2.2.8）。
- **未决事项**：**Qt6 迁移全部结案（2026-10-26，千夏验收通过）**。挂账清单见 §10 尾部（大 xml 异步解析、Danmaku 本体成测、编辑器上色/底栏按钮、渲染重构、VLC 4）。
- **CI**：`.github/workflows/ci.yml`——push 到 master 触发，**单 job（Phase 2 起）**：runner 上 aqt 装 Qt 6.8.3 msvc2022_64 + qt5compat（不钉 `-G`，坑 51；装完落盘验 `Qt6Core.dll`，坑 52），`BILILOCAL_BUILD_APP=OFF` 只编测试目标并 ctest（41 例）；Qt 走 actions/cache（key `qt-6.8.3-msvc2022-v1`）。查状态：认证 API 或徽章 SVG。**每次 push 都会跑，红了不许推**。
- **验证分工**：agent 工具几乎全能自验——日志取证（`hover: freeze/release`、`Load: processed … pool=N`、`Program Link Failed` 计数）、进程内钩子注入（双击全屏/单击暂停/悬停扫掠/拍点右键）、时序验证、对话框巡览截图；**用户本人只做"真实右键点到弹幕"这一个动作**（物理点击过不了三层窗口栈的焦点/显示盲区，合成事件替代不了，见 `a4b5acf`）与主观体验类验收。

### 2.1 版本沿革（根因与证据都在 git 提交信息里）

| 提交 | 主题 |
| --- | --- |
| `46b68b9` | vmem 线回滚点 tag `v0.5.0-vmem`（VLC 2.2.8，考古用） |
| `5c33397`…`82a3d13` | Embed 直出重写（d3d11 原生子窗 + GL 覆盖层路由输入）+ 现代快捷键 |
| `4702ab1` | 单底栏 UI；死在线功能移除 |
| `49c173c` / `d7847e8` | 死代码大清理 + 单平台化（vmem/跨平台/在线路径全删） |
| `c8679e6` | 现代化：主题/图标/面板/QtTest；顺手修上游 ASS 颜色 bug |
| `cbc0b42` / `c834050` | 启动续播 + 目录记忆；enum class 槽全限定修复 |
| `3155020` / `3f86f74` | 单击暂停 + 输入路由修复；VLC 3.0.24（130+ 安全修复） |
| `32c6597`…`473ba62` | Win11 设计语言轮（双主题 QSS、Prefer 重排 6→5 页、`BILILOCAL_UI_TEST` 钩子） |
| `0c4f7b7`…`7f8685a` | 瘦身轮（dist 358M→176M、插件机制+网络栈移除、Load 本地化、List 扫描合并） |
| `44fbb3e` | 底栏打开接入 `/Interface/OpenDir`（`cbc0b42` 只接了 Menu 面板，底栏重启即失忆） |
| `54e718c`…`8d9c8ec` | 交互轮：悬停冻结单条弹幕（静止不解冻）/右键弹幕数据块（全文、脱敏用户名+user_hash、时间、位置、真实同源计数）/屏蔽发送者按 hash 生效；录播姬毫秒时间戳（2038 bug）；测试钩子循环化 |
| （交互轮打磨） | 屏蔽页双列重构；弹幕专属右键菜单（点弹幕只留 信息/复制/屏蔽）；菜单快捷键列按 sizeHint 加宽；开关行文案翻转（打开/关闭弹幕）；勾选态以 Shield 为真值源；音频菜单去掉提高/降低音量 |
| `a4b5acf` | **放弃底栏弹幕右键菜单**（用户决定，勿重做）：进程内事件合成全通，真实点击始终不弹出——三层窗口焦点/显示盲区，见 §6-23~25 |
| （设置审计轮） | 全库配置键交叉审计：删死项（版本信息复选框、无框窗口+拖窗残留）、补漏（播放页「播放行为」卡、弹幕外观加轨道间距 `/Danmaku/Grating`）、Shield 类型 tooltip、关于页构建信息卡、文件面板「清空」。「防挡字幕」= `/Danmaku/Protect`，已有入口故未重复 |
| `a5f95fe` | 警告 42→4：`Config.h` 的 `QVariant` 特化补 `[[maybe_unused]]`（清 29 条 defined-but-unused）、`endl`→`Qt::endl` ×3、`globalPos()`→`globalPosition()` ×3 |
| `81aab8f` | `Danmaku` 的 `BackgroundRole` 不再穿透到 `ModeRole`（普通弹幕曾把 mode 整数当背景刷返回；Qt 不认 int→QBrush，所以无可见变化，属契约修复），警告 4→3 |
| `d98327b` | UI/DPI：①`Local.cpp` 补 `PassThrough` 取整策略（150% 屏实测 dpr 2.00→1.50、逻辑尺寸 1280×720→1707×960）并开 `AA_UseHighDpiPixmaps`；②播放/暂停改 `QPainter` 按 dpr 1/1.5/2/3 逐档绘制（原 16×16 png + fromTheme 在 Windows 永远 fallback）；③两份 65 行 QSS 搬到 `res/Style/*.qss` 随 qrc 打包；④`Res.qrc` 用 alias 避免 `/Style/Style/…`。exe 943,616→927,232 |
| `cd3b0fe` | 审计第 1 批：①`Config.h` 的 `getValue/setValue` 读-改-写整段进锁（旧实现无锁读快照再持写锁整体覆盖，两线程改不同键会丢更新）；②`Config.cpp` 解析失败先把原文件拷成 `Config.txt.broken-yyyyMMdd-HHmmss`（旧行为：静默空对象，退出时用出厂值把用户文件覆盖），`save()` 锁内取快照再写；③`List.cpp` 空表 `rc <= 0` 直接 return（`(i+rc-1) % rc` 整数除零，首次启动按 PgUp 必崩）、Surmise 头节点搜索不再负行后解引用、找不到回落同名文件；④新增 `test/tst_config.*` 7 例。exe 927,232→928,768 |
| `2f8a1d1` | 审计第 2 批：①`Home::setWindowFlags()` 恢复 `/Interface/Top` 三态（旧代码并成 `>=2`，"只在播放时置顶"等于 0）并改用 `SetWindowPos(TOPMOST/NOTOPMOST)`（`QWidget::setWindowFlags()` 会重建原生窗口）；原路径 emit 的 `Interface::flagsChanged` **全仓零接收者**；②删掉该信号；③`Prefer` 快捷键列表改按 objectName 非空筛选（旧 `QRegularExpression(".{4}")` 是子串匹配，3 字符的 `Top` 永不出现在列表里、改不了键），`exec()` 看返回值（Esc 不再写盘），非法正则当场标红+tooltip；④`Shield` 载入 `/Shield/Regexp` 跳过空串（空正则匹配一切 ⇒ 全屏弹幕消失）与非法正则；⑤`Running` 两处 `clear()+waitForDone()` 屏障后 `d->wait = 0`（泄漏几次后密度上限恒真 ⇒ 新弹幕不再出现）；⑥`AA_UseOpenGLES` 从"构造之后（空操作）"挪到构造前，且只有 `BILILOCAL_GL=angle` 才开。exe 928,768→929,280 |
| `1b10dc0` | 审计第 3 批（**只修健壮性、不改使用逻辑**——用户裁定"不改使用逻辑"，交互类一条都没做）：①`Load.cpp` 类型嗅探先 `trimmed()`（首字节是空行的 xml 以前整份交给 AcFun JSON ⇒ **0 条弹幕**；A/B 实测新=1 条、旧=0 条）；②`Parse.cpp` `comment.date` 改 `toLongLong()` + 毫秒归一（13 位毫秒 `toInt` 溢出成 0）、ASS `Dialogue` 行加列数校验、`strtod` 溢出不再 UB；③`Danmaku.cpp` 追加同源第二份 xml 时先 `reserve` 再取地址（去重集存的是元素地址，旧顺序全悬空）；④`Shield.cpp` `<= 2` 改 `< 2`（`t=` 残留规则以前删不掉）；⑤`VPlayer.cpp` 换片前 `libvlc_event_detach` 五路事件并在 `timeMutex` 内 release；⑥`Local.cpp` 单实例 socket 循环 `waitForReadyRead` 读干净。exe 929,280→929,792 |
| `9edac7c` | 审计第 4 批（交互一致性，用户"做就做呗"授权）：①`Info::eventFilter` 只认左键（右键/中键松手不再 `setTime`）；②滚轮在进度条上被吞掉（原来走 `valueChanged`、`isSliderDown()` 为假 ⇒ 滚一下跳 5 秒或 1/10 时长）；③拖到最右钳到 `duration-500`（跳满会被判成片尾直接 `stop()`）；④新增 `Info::timeAtX()`，`Home::seekTo` 与滑块共用同一套 x→时间映射（原来整窗宽 vs 滑块宽，左端差 slx）；⑤新增 `Info::barMinWidth()`，`Home::resizeEvent` 据此抬最小宽度（窄窗进度条压住右侧按钮） |
| `600b369` | 审计第 5 批（渲染层，批准后解冻）：①`OpenGLAtlasMgr::squeeze()` 不再当场 `delete`（由 `timeChanged` 直连进来、paintGL 之外没有当前上下文），改挪进 `graveyard`、由 `collect()` 在 `OpenGLRender::draw()` 里真删；②`timeChanged/reach` 两个 lambda 加 `!d->resource.isNull()` 守卫；③`~OpenGLEmbedRenderPrivate` 补齐拆卸（disconnect 两路 begin、removeEventFilter+delete、`delete surface`）；④延迟 `nudge` 改用 `QSharedPointer<bool> alive` 失效标记 |
| `30466b0` | 弹幕白方块（用户报"弹幕变成白色方块"）：六个 shader 首行的 `precision lowp float;` 被 Qt 的 `#define lowp` 展开成 `precision  float;` ⇒ 桌面 GLSL 语法错误、程序没链接上，代码却照旧 `glDrawArrays` ⇒ 每条弹幕被固定管线画成纯白矩形。删掉六行后 `syntax error\|Link Failed` 归零、弹幕恢复白色描边文字 |
| `7bf39d2` | 同一视频弹幕重复（用户报"同样的弹幕会出现两次"）：List 里同时存在模板 `%{Path}/%{File}.xml`（正斜杠）与"命令行带 xml 打开"登记的绝对路径（反斜杠），`Load` 按 source 字符串全等去重失效 ⇒ 加载两次。`case Records:` 改按 `QFileInfo::canonicalFilePath()`（取不到退回 `cleanPath(fromNativeSeparators())`）本轮去重；实测 `Load: processed` 两条→一条 |
| `0c74654` | 单测第 3 轮（模型层补覆盖）：`tst_shield` 9 例（空/坏正则跳过、m/u/t 往返、isBlocked 全语义、aboutToSave 落盘往返——顺带坐实了 Shield 构造连接无上下文对象这个隐性约束）+ `tst_load` 12 例（真文件真嗅探：BOM/空行两大历史回归、四格式分支、AcFun 兜底、缺文件报错、队列顺序、延迟平移）；负向对照 = 去 `.trimmed()` 后 `blankLineBeforeXml` 变红。fixture 的两个教训见 §6-43/44 |
| `0f25480` | 钩子注释纠偏：`EmbedPrivate.cpp` 的"正式构建不编译"改为"无条件编进所有构建"（§8 的 ⚠️ 项结案）；纯注释改动，exe 尺寸不变重新部署 |
| `5ecd888` | `tst_parse.cpp` 头注纠偏：删掉"Danmaku/Shield 覆盖不到"的过时说法（Shield 已由 `tst_shield` 覆盖）与"人眼弹幕"旧表述（见本版验证分工修订）；纯注释 |
| `dd8efc3` | tst_list：`case Records` 去重端到端——模板子项（itemFromFile 目录扫描自动建）+ 命令行反斜杠绝对路径复刻 `7bf39d2` 现场 → 池=1；ghost 文件变体验证 fallback 去重 → `errorOccured`=1；负向对照两红。测试结构教训：List 的信号连接同样无上下文对象，实例必须全程一份；**刻意不注册 Config**（掐断 touch→自动保存链，测试不落盘） |
| `15b2667` | CMake 构建：CMakeLists 与 .pro 逐项等价（单平台三件套 DEFINES + 验证钩子、PCH、AUTOMOC/AUTORCC、图标 RC、翻译、5 测试目标），41 例 + hook-gate 双验证后 **dist 切换为 CMake 产物**；`.gitignore` 的 `*.rc` 加例外；tst_config 补 Local 桩（AUTOMOC 会 moc include 链里的头，坑 47） |
| `395bb09`…`2136fe5` | CI 落地：GitHub Actions push 触发，runner 只编测试目标并 ctest（41 例，run #4/#5 两连绿 + 缓存命中验证）；aqt 的 tools_mingw810 元数据已废 → MinGW 8.1 改 SourceForge mingw-builds 官方存档 + 7zr.exe 解（坑 48）；`BILILOCAL_BUILD_APP=OFF` 让 CI 免 VLC SDK |
| `74a7569` | 文档 19 版：迁移裁定解除，任务书进 §10（回滚锚 tag `v0.6.0-qt5-final`） |
| `f81e6ee` / `25e3751` | **Phase 1**：MSVC 工具链（官方 .lib 入 thirdparty/lib/msvc、全局 /utf-8、fixture 反斜杠真修——坑 49/50）；CI 加 msvc job、不钉生成器（坑 51） |
| `599dc95` | Phase 1 结案：千夏开关 `dist-msvc` 验收通过 |
| `8bb173f` / `f630a70` | **Phase 2**：Qt 6.8.3 全面落地（shader 330 core、Core5Compat、事件坐标/desktop/setCodec/AA_* 全清）+ qmake 遗留删除 + run-tests 改 ctest + CI 换 Qt6 单 job（坑 52–54）；文档 21 版 |
| `d5b6383` | 文档清扫：README 构建/测试段换 Qt6、沿革补全、全部失效句清理 |
| `8f15d86` | **坑 55 修复**：atlas 退出零 GL 调用 + collect 批量删（关闭 2.0–8.1s → 2.0–2.1s；4 份 WER 转储取证法入档） |

---

## 3. 怎么用（用户视角）

产品功能、快捷键、断点续播、自动续播、目录记忆等说明在 `src\README.md` 的 **Usage** 一节（那是"换台机器也成立"的产品事实，本节不再重复）。

> **验证注意**（2026-10-25 按用户澄清收窄）：弹幕层是 QOpenGLWindow，对 BitBlt/PrintWindow 抓屏**不可见**——截图全黑不代表画面黑（视频/底栏用 `tools\pwc.ps1` 能抓到）。**截图不能当弹幕证据，但弹幕并不因此靠人眼**：行为面（事件路由、悬停冻结、加载计数、shader 链接、时序）全走日志 + 进程内钩子，agent 可自验（见 §8）；**只有"真实右键点到弹幕"这一个物理动作必须用户本人**——合成事件全通、真实鼠标点击过不了三层窗口栈的焦点/显示盲区（`a4b5acf`/§6-23~25）。

---

## 4. 怎么重编 / 部署

**CMake（现役唯一构建路径：Qt 6.8.3 msvc2022_64 + VS 生成器）**（cmake 来自 pip 包：`...\dsh-runtimes\dsh-next-primary-runtime\dependencies\python\Scripts\cmake.exe`）：

```powershell
# 不需要手动配 cl 环境：VS 生成器经 vswhere 自动定位 BuildTools（cl 14.44）
cmake -G "Visual Studio 17 2022" -A x64 -B C:\DSH\APP\BiliLocal\build-cmake6 -S C:\DSH\APP\BiliLocal\src `
      -DQt6_DIR=C:\DSH\APP\BiliLocal\deps\6.8.3\msvc2022_64\lib\cmake\Qt6
cmake --build C:\DSH\APP\BiliLocal\build-cmake6 --config Release -j8
# 产物在 build-cmake6\Release\BiliLocal.exe；部署目标 = dist\（对调后 dist 即 Qt6 树）
#（Qt6 DLL：Core/Gui/Widgets/Network/Concurrent/OpenGL/OpenGLWidgets/Core5Compat；
#  插件只带 platforms\qwindows、imageformats\{gif,ico,jpeg}、styles\qmodernwindowsstyle）
# 部署（先停进程）
Stop-Process -Name BiliLocal -Force -ErrorAction SilentlyContinue
Copy-Item C:\DSH\APP\BiliLocal\build-cmake6\Release\BiliLocal.exe C:\DSH\APP\BiliLocal\dist\ -Force
# 回滚到 Qt5 线：把 dist 改名 dist（现 Qt6 树）→ 备用名，dist-qt5 改回 dist 即可
```

改动 CMakeLists 后重跑 configure。测试：`tools\run-tests.ps1` 一键（build + ctest），或手动
`ctest --test-dir build-cmake6 -C Release`（跑前 PATH 带 Qt6 bin）。

- 源码里有中文 `QStringLiteral`/`tr()` 的文件必须保持 **UTF-8 BOM**；编辑工具会吃掉 BOM（见 §6-29）。
- 历史小抄：qmake 路径（`.pro`×6、`build\`、6 个 build-test* 树）已在 Phase 2 删除，命令别再找；
  MinGW/Qt5 的 CMake 双线命令同样作废（`build-cmake`/`build-cmake-msvc` 两棵死树留着等 Phase 3 清理）。
- 回滚点：`git checkout v0.6.0-qt5-final` + Qt5 工具链 = 回到迁移前（deps 的 Qt5/MinGW 树都还在）。

---

## 5. 代码结构速览（写给要改代码的你）

- **架构**：`Local` 服务定位器，`lApp->findObject<T>()` 取 9 个模块（Config / Shield / Danmaku / Running / Interface / APlayer / ARender / Load / List）。
- **播放**：`VPlayer`（libvlc，唯一后端）。视频经 `set_hwnd` 直出到原生子窗口 `ESurface`（d3d11va 硬解 + direct3d11 vout，参数在构造时固定注入，可被 `Arguments` 手写覆盖）。
- **弹幕**：`Load`（仅本地文件）→ `Parse::parseComment`（**同步**，直接返回结果）→ `Danmaku` 模型 → `Running`（池线程布局防碰撞）→ `EWindow`（透明 GL 覆盖层，兼作输入面）绘制；文字精灵走 `ASprite`/`SyncTextureSprite`/`OpenGLAtlas` 图集。
- **UI**：`Home`（主窗，快捷键/全屏/seek 横带）+ `Info`（单底栏）+ `Menu`（文件与弹幕悬浮面板）+ `Editor`（弹幕池编辑）+ `Prefer`（设置，5 页：播放/界面/屏蔽/快捷/关于）。
- **主题**：`src\res\Style\{dark,light}.qss` 随 qrc 打包（改配色不必重编译，但要重新 rcc）。
- 每层的关键决策都写在 git 提交说明里：`git log --oneline` + `git show <hash>`。

---

## 6. 修过的坑（现象 → 根因 → 修法）

1. 中文路径乱码 → `CommandLineToArgvW` 取 argv
2. 录播姬 xml 带 BOM 解析 0 条 → 剥 `EF BB BF`
3. 死成员指针崩溃（WER dump 定位）→ 删 + 置空
4. 底栏拉不回来（动画状态脱节）→ `isShown()=isVisible()` + 先 stop 动画
5. 不动鼠标不收起 → 400ms 常驻轮询
6. 全屏白块 → 三层几何同步 + 延时重排
7. 退出全屏丢最大化 → wasMaximized + DWM 禁过渡
8. 进度条不灵敏 → eventFilter 接管 + 整条横带
9. 设置存不住 → `%LOCALAPPDATA%` + 去抖 + 原子写
10. VLC3+vmem 崩 / VLC2 无法硬解喂 vmem → **架构性互斥，最终走 Embed 直出路线**
11. MinGW `::time` 撞名 C 库 → 改名 `timeMutex`
12. MinGW 链式赋值 QFlags 报错（MSVC 容忍）→ 拆开写
13. lambda 按引用捕获 ctor 局部变量 → 悬空 UB → 改静态
14. **DWM MPO 启用时 BitBlt/PrintWindow 抓屏黑屏缺层** → 用 `pwc.ps1`（PW_RENDERFULLCONTENT）；弹幕 GL 层任何抓屏都不可见（截图只能作反证不能作正证），但行为验证走日志/钩子即可——真正必须本人的只有"真实右键点到弹幕"这一个动作（§3、`a4b5acf`）
15. Parse 异步 ResultDelegate（QFutureWatcher 链）在在线模块移除后不触发 → **同步化 VectorRecord**
16. **enum class 槽参数必须全限定**：moc 记录声明原文（`Event`），跨线程 `invokeMethod` 的 `Q_ARG(APlayer::Event,…)` 给的是全限定名 ⇒ 不匹配时**静默失败**，症状是 state 永远 Stop（拖不动/无时长/标题不变/List 存 Time=-1）。另：**图像分析读 UI 数字不可靠**（会迎合 prompt 里的示例数字），断言要用行为证据
17. （教训）`invokeMethod` 返回值从不检查——关键投递点要么查返回值，要么改信号转发
18. **`childAt` 无视 `WA_TransparentForMouseEvents`**：`EWindow::route` 命中透明 ESurface ⇒ 事件进黑洞（视频区单击/双击/拖动全失灵）。转发前沿 `parentWidget` 跳过穿透控件
19. **`QWidget::mouseDoubleClickEvent` 默认转发 `mousePressEvent`**：重写后调基类 = 双击又触发一遍单击逻辑。（历史路线 Win32 子类化转发鼠标消息已废弃：投递不达 vout proc + VLC 重建窗口后钩子悬空崩溃）
20. **`invokeMethod` 按名调用要求目标是槽**：普通私有方法会静默失败（只有 "No such method" 进日志才看得见）；`Load::processNext` 即例，修法 `private slots:`
21. **`Danmaku::append` 按 source 路径去重**：同一 xml 传两遍造不出 pool=2——测 Editor 自动弹出要用两个不同路径的文件
22. **队列消费的 dequeue 时机**：任务在处理函数开头取出；尾部残留 dequeue 会吞掉下一个任务（靠 `Load: processed` 日志定位）
23. **包 if 时检查栈对象寿命是否覆盖 exec**：弹幕专属菜单把四个子菜单的栈 `QMenu` 关进 `if` 作用域、`exec` 在作用域外 ⇒ 子菜单先析构、菜单项凭空消失。声明提到函数作用域，更深层子菜单用堆分配
24. **覆盖层吞 `WM_CONTEXTMENU`，且 QWindow 系的 Leave/ContextMenu 都走 `event()`**：右键事件落在最上层 `EWindow` 头上，不转发则底层按钮的 `customContextMenuRequested` 永不触发；`leaveEvent`/`contextMenuEvent` 这些 QWidget 名字在 QOpenGLWindow 上不存在，必须在 `event()` 里按类型分发
25. **`childAt` 不保证递归到叶子**：右键转发只命中 `Info` 容器本体而非内部按钮——要沿 `childAt` 一路下沉到最深子控件，再沿 `parentWidget` 跳过穿透控件（与坑 18 同源不同面）
26. **QSS 菜单里别用 checkable 行**：勾选列在内边距下失控（✓ 吊在左边距、文字多缩进 28px，实测 x=47 vs 75）；用普通动作 + 文案随状态翻转表达
27. **录播姬 raw 属性里的 JSON 是字符串套字符串**：内层引号写作 `\\&quot;` 带反斜杠，按 `&quot;` 搜永不命中；合成单测样本漏了反斜杠会**假绿**。用正则容忍可选反斜杠，并拿真实弹幕字节做单测
28. **MSYS2 的 `sed -i` 会把 CRLF 悄悄变成 LF**：改这份文档（工程根 `.md` 不在 git 里）要么用编辑工具，要么 `sed -i` 之后 `file` 检查换行符再补 CR。源码走 `core.autocrlf=true` 会被归一，无所谓
29. **编辑工具会吃掉 UTF-8 BOM**：`Danmaku.cpp` 只改一行 BOM 就没了，而 `tools\fix-bom.ps1` 在当前 ExecutionPolicy 下跑不起来。手补 `printf '\xef\xbb\xbf' > t && cat t f > f2 && mv f2 f`；补完用 `git ls-files` 全量比对前三个字节与 HEAD blob
30. **qrc 的 `prefix` 会和 `file` 路径拼起来**：`<qresource prefix="/Style"><file>Style/dark.qss</file>` 的资源名是 `:/Style/Style/dark.qss`，代码按 `:/Style/dark.qss` 读会 `exists()==false` ⇒ QSS 静默变空、编译期零提示。要么放平文件，要么写 `<file alias="dark.qss">`。校验：把 `build\release\qrc_Res.o` 链进 20 行探针读出来比对字节数
31. **exe 本来就带 `-Wl,-s`（已 strip）**：别以为"加个 `-s` 还能省 100–200K"。真正让 exe 变小的是把 130 行 QSS 字面量从文本段挪进压缩资源：943,616 → 927,232 → … → 931,840
32. **别按行号从 `git show HEAD:file` 切片回滚**：blob 行号与工作树不一定对齐，拼回去编译报 `error: 'lock' was not declared in this scope`。按内容定位，或在"已修好的副本"上做 `sed 's/旧/新/'` 生成变体
33. **`QRegularExpression` 是子串匹配，不是整串匹配**：`findChildren<QAction *>(QRegularExpression(".{4}"))` 想写"名字至少 4 字节"，实际是"含一处 4 字符子串" ⇒ `Top`(3) 被漏、`Fowd2`(5) 反而命中。要么锚定 `^.{4}$`，要么按 `objectName().isEmpty()` 筛
34. **运行期别用 `QWidget::setWindowFlags()` 改置顶**：它会重建原生窗口（闪、可能折腾焦点）。置顶只是 Z 序，直接 `SetWindowPos(hwnd, HWND_TOPMOST/NOTOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)`
35. **弹幕 xml/ass 的类型靠"开头几字节"猜**：`Load::processNext` 只看 `data.left(512)`，xml 前面有一个空行 `startsWith("<?xml")` 就失配、整份被当 JSON ⇒ 0 条弹幕且无报错。嗅探前必须 `trimmed()`，BOM 要先剥
36. **GLSL 1.20（`varying`/`texture2D`）在 core profile 上下文里编译不过**：本机虚拟显示适配器上实测 `syntax error, unexpected IDENTIFIER`，弹幕层不画。这不是代码改动引入的（日志里早有 8 次同样失败）；真机 NVIDIA 走兼容 profile 正常。`30466b0` 之后本机已正常（真正的拦路虎是 §6-40）
37. **新写的单测要做一次负向对照**：`tst_config` 修好版稳定 7/7；把两处 `QWriteLocker` 注释掉重建后 `concurrentWriters` 直接 SIGSEGV。没这个对照就无法证明用例不是空跑
38. **`QSlider` 的滚轮不走 `sliderMoved`**：滚轮改值触发 `valueChanged`，那时 `isSliderDown()` 为假 ⇒ "只响应拖动"拦不住。要拦就在 `eventFilter` 里吞 `QEvent::Wheel`
39. **`glDeleteTextures` 必须在上下文当前时调**：`OpenGLAtlasMgr::squeeze()` 由 `timeChanged` 直连（paintGL 之外），在那里 `delete` 图集 ⇒ 删除打空、纹理不释放。改法是只标记进 `graveyard`，在 `draw()` 里 `collect()` 真删
40. **桌面 GL 的 shader 里不要写 `precision lowp float;`**：Qt 为桌面 GL 预置 `#define lowp`/`mediump`/`highp`（展开为空），这行变成 `precision  float;` ⇒ 桌面 GLSL 语法错误，表现为 `syntax error, unexpected IDENTIFIER` + `Program Link Failed`；程序没链接上而代码照旧 `glDrawArrays`，于是每条弹幕被固定管线画成**纯白方块**。排查入口：`msg.log` 里 `Program Link Failed` 的条数（本机曾累积 35550 条，每帧 `bind()` 都重试）
41. **同一视频在 List 里可能挂多条指向同一份弹幕的规则**：模板展开成正斜杠、"命令行带 xml 打开"登记的绝对路径是反斜杠，`Load` 按 source 字符串全等去重失效 ⇒ 同一份 xml 加载两次、弹幕整屏重复。`List` 的 `case Records:` 现按 `QFileInfo::canonicalFilePath()`（取不到退回 `cleanPath(fromNativeSeparators())`）本轮去重；用户 List 里那两条规则原样保留
42. **别用 exe 的 SHA256 判断"是不是同一份源码编的"**：同一构建树重复 clean 重建只差 4 字节（PE 时间戳/校验和），但把同样的源码导到另一个目录重建，尺寸一模一样（931,840 B）却有 2,692 字节不同（代码/数据区 2,688），成因未定。要证明源码同源，认 git 提交号。
43. **Load 的 xml 嗅探只认 `<packet>`/`<i>`/`<c>` 三种头**：裸 `<l i=` 内容（AcFunLocalizer 真文件必有 `<c>` 包裹）三个分支全不命中 ⇒ 默认构造的 `ResultDelegate` ⇒ **静默 0 条弹幕**。给 `tst_load` 写 fixture 时踩到；"能被认出"本身就是格式契约的一部分
44. **QtTest 的 `QVERIFY`/`QFAIL` 宏展开成 `return;`，只能用在 void 函数**：放进返回 QString/bool 的辅助函数直接编译错（`qtestcase.h:66`）。测试辅助函数一律 void + 出参
45. **弹幕 lane 自顶向下填充，稀疏弹幕永远到不了屏幕中部**：Running 的"最空 lane"算法按 y=0,10,20… 顺序占位，(600,320) 这类中部锚点只在并发弹幕多到把 lane 填过 320（约 130 条在屏）时才有图形可冻结——探针实锤：稀疏 fixture 的矩形范围 y[0..275]→y[0..75]，"悬停冻结"验证静默不报错。**hook-gate 的 fixture 必须 0.05s 间隔**；据此推断真机日常录播（高密度）才会触发悬停冻结，低密度直播场景该功能可能"看似没反应"
46. **build 树的 exe 直接跑不起来**：加载器不认 PATH 兜底（裸跑退出码 `0xC0000135`，或静默卡在加载器错误框上进程活着但零日志）。要在 build 树调试：`cp dist/*.dll build/release/` + `mklink /J build\release\plugins dist\plugins`，诊断完 `rm *.dll` + `rmdir plugins`（**junction 只能 rmdir，rm -rf 会穿透删掉 dist\plugins**）。hook-gate 已内置 `QT_QPA_PLATFORM_PLUGIN_PATH`/`VLC_PLUGIN_PATH`/PATH 三个环境变量，但实测仍需 DLL 在 exe 旁——**现脚本对非 dist 的 exe 自动 stage**（见 §8）
47. **AUTOMOC 会 moc include 链里所有含 Q_OBJECT 的头**：qmake 的 moc 面由 HEADERS 列表决定，CMake 的 AUTOMOC 扫源码 include——`tst_config.cpp` 没 HEADERS 列 Local.h 也被 moc 出来（`mocs_compilation.cpp` 引用 ctor/dtor/槽符号）。CMake 化时按链接错误补桩，别按 qmake 的 HEADERS 直觉；**两套构建并存时反过来也成立**——补了 ctor/dtor 桩的 TU 会发射 vtable，qmake 侧 HEADERS 必须补列同一个头（`tst_config.pro` 加 Local.h），否则 vtable/metaObject 无着色
48. **aqt 的 `tools_mingw810` 元数据已废，且 mingw-builds 的 7z 是 BCJ2 滤波器**：CI 首跑与本机均复现 `tools_mingw810/Updates.xml` checksum 拉取失败（qt.io 侧问题，非代理）。MinGW 8.1 改从 SourceForge 的 mingw-builds 官方存档下（与 Qt 配套同源：x86_64-posix-seh 8.1.0）；py7zr 遇 BCJ2 直接 `UnsupportedCompressionMethodError`，用独立 `7zr.exe`（7-zip.org，600KB）解。CI 完整流程见 §8 的 CI 段
49. **MSVC 在中文 Windows 上默认按 GBK 读无 BOM 源码**：仓库测试文件是无 BOM UTF-8 + 中文注释，MinGW 默认 UTF-8 所以从没暴露；MSVC 把 UTF-8 字节当 GBK 解，某个多字节序列可以吞掉紧随的声明字符，表现为成片的 `C2334 "unexpected token before {"` + 辅助函数 `C3861 找不到标识符`，看起来像代码坏了其实一个字符都没错。修法 = CMake 全局 `add_compile_options(/utf-8)`（带 BOM 的文件以 BOM 为准，互不干扰）。主程序目标先加了 /utf-8 所以一次通过，测试目标没加就全挂——**这坑只在"部分目标加了"时最迷惑**
50. **`\&` 是未知转义：GCC 静默解码成 `&`，MSVC C4129 才报警**：tst_parse 的录播姬 fixture 写的 `{\&quot;user_hash\&quot;…}`，`\&quot;` 实际解码 = `&quot;`——**反斜杠丢了**，恰好是坑 27 自己警告的"合成样本缺反斜杠 = 假绿"形状（真实录播姬字节里内层 JSON 引号写作 `\&quot;`，带反斜杠）。修法是把 fixture 改成 `\\&quot;`（C++ 层双反斜杠 ⇒ xml 层真反斜杠），`hashRe` 本就容忍可选反斜杠，41 例不变、反斜杠分支从此有覆盖。教训：**换编译器的价值之一就是它替你抓GCC 沉默的老问题**
51. **windows-latest 镜像不再保证装 VS2022**：CI 钉 `-G "Visual Studio 17 2022"` 报 `could not find any instance of Visual Studio`（2026-10-26 实测，runner 镜像的 VS 随年代演进）。修法 = CI 不指定生成器、只给 `-A x64`，让 CMake 选镜像实际安装的默认 VS；本机 BuildTools 是 17.x，显式 17 2022 不受影响。**教训：CI 配方里凡是"镜像预装了 X"的假设都会过期，能不钉就不钉**
52. **aqt 的两个 6.x 陷阱**：①Qt 6 没有 `qttools` 模块——lrelease/lupdate 在 base 包里，`aqt install-qt … -m qttools` 报 `The packages ['qttools'] were not found`；②**报这个错时 aqt 仍然 exit 0**（本机实测），后台任务/脚本看着是绿的其实什么都没装。修法 = 只 `-m qt5compat`，装完落盘验 `bin\Qt6Core.dll`（CI 已内置）。**教训：外部工具的 exit code 不可全信，关键步骤要有产物存在性断言**
53. **Qt6 的 `QtConcurrent::blockingMappedReduced` sequence 版是转发引用**：签名 `<ResultType, Sequence, MapFunctor, ReduceFunctor>(Sequence &&sequence, …)`——Qt5 时代显式把 `Sequence` 钉成值类型 `QVector<T>`，形参就是右值引用，左值序列绑不上，编译器只剩迭代器重载可候选（报"没有重载可以转换所有参数类型"，且候选注记把 4 个模板实参错位展示，极难读）。修法 = 模板实参写 `QVector<T> &`（或传 `std::move`）。**教训：Qt6 把 QtConcurrent 的映射版重载整个重做过，显式模板实参要对 6.8 的头文件核对，别背 Qt5 的**
54. **迁移影响清单按"类型名"扫描会漏调用点**：§11 清单写了 `QDesktopWidget` 0 命中，但 `QApplication::desktop()->screenGeometry()` 这类**调用点**（类型名不出现）漏网两处（Home.cpp/WidgetInterfacePrivate.cpp），到 Qt6 编译才炸。修法 = `screen()->geometry()`（QWidget::screen 自 5.14 起有）。**教训：查 API 存废要连方法名一起 grep（`::desktop(`、`->desktop(`），只查类名必漏**
55. **Qt6 下退出时的 GL 删除会挂住/崩溃（图集析构链，Phase 3 用户实测暴露）**：关闭窗口时 GUI 线程在 `~OpenGLAtlasMgr`/`~OpenGLAtlas` 的 `render->glDeleteFramebuffers/glDeleteTextures` 上被驱动拖住数秒（弹幕池越大越久：0 条 2s、1 万条 2~8s），同一析构链还会空指针崩溃。**取证路径值得记**：WER LocalDumps 落的 4 份转储里，**挂起报告的"异常地址"就是卡死线程的当前 IP**——用 pip 的 `minidump`+`pefile` 解析（本机无 cdb）：异常 IP 对 Qt6Gui.dll 导出表取最近符号 = `glDeleteFramebuffers+0xc`，两份挂起同址独立复现，另两份同链 ACCESS_VIOLATION。修法 = ①退出路径零 GL 调用（对象随 context 销毁回收，退出补删本来就是无效功）；②播放中 `collect()` 批量删除（一次 `glDeleteTextures(n, ids)` 顶逐页 N 次驱动往返——每次往返都可能被驱动拖住，播放中的小顿挫同源）。实测：关闭 2.0–2.1s×3（修复前 2.0–8.1s，Qt5 基线 2.0s）。**教训：①AppHang 的 dump 与崩溃 dump 走同一个 LocalDumps 目录，别只当崩溃用；②Qt6 改了窗口/上下文销毁时序，"退出时补删 GL 对象"的代码全部要重审；③析构链里持有兄弟对象指针（atlas→render）在销毁序变化下随时变悬空**

---

## 7. 已知限制（不是待办，是设计取舍）

1. 大 xml（数万条）解析在 GUI 线程同步执行，可能卡数百 ms~秒级（Parse 同步化后引入；本地录播 xml 无感）。真要异步必须用可靠机制——别回到 `QFutureWatcher`；注意 `Load` 的 `onFinish` 里有 `&task` 引用捕获，异步化会悬空。
2. 倍速按钮长按右键的 350ms 阈值 / 3.0 倍率是拍脑袋值，可按手感调。

---

## 8. 工具脚本（`tools\`，16 个）与验证钩子

| 脚本 | 用途 |
| --- | --- |
| `run-tests.ps1` | **一键门禁**（pre-push 调它）：`cmake --build build-cmake6` + `ctest -C Release`（5 目标 41 例），任一失败退出码 1；路径相对脚本位置，要求 build-cmake6 已 configure |
| `hook-gate.ps1` | **GUI 钩子门禁**：以 `BILILOCAL_EMBED_TEST=1` 起播放器播本地 fixture，断言 `msg.log` 三个标记（`seeked to 10%` / `hover: freeze` / `hover: release`），全中才退出码 0；`-Media` 可换 Z: 真实录播做对照；`-Dist` 指定运行时根（并行树如 `dist-msvc` 必传，否则 staging 会把 dist 的 MinGW DLL 盖进 MSVC 树）；**非运行时根的 exe 会自动 stage DLL+junction**（坑 46） |
| `gen-media.py` | 生成 hook-gate 的本地 fixture 到 `tools\.media\`：`sample-60s.y4m`（裸 YUV4MPEG2 流，libvlc 原生可播，本机无转码器也能造视频）+ 同名 xml（**0.05s 间隔高密度弹幕**，原因见 §6-45） |
| `setcfg.py <key> <value>` | 改 `Config.txt` 任意键（布尔/数字/字符串自动识别） |
| `pwc.ps1` | **PrintWindow(PW_RENDERFULLCONTENT) 抓窗口**——抓视频/底栏用这个 |
| `shot.ps1` | BitBlt 抓屏（D3D/GL 层会黑，仅参考） |
| `perfrun.ps1` | 起播放器采样 CPU（`-Media` 指定本地文件、`-Exe` 指定 exe） |
| `kids.ps1` / `embed-probe.ps1` | 列窗口树 / 窗口树 + 父窗口 + 类名 |
| `dense.py` | 解析指定录播 xml 求密集窗口（脚本内路径写死，需改源码指路） |
| `fix-bom.ps1` | 源文件 BOM 对齐 git HEAD（当前 ExecutionPolicy 下跑不起来，见 §6-29） |
| `verify_ui3.ps1` | 对话框巡览验证：配 `BILILOCAL_UI_TEST=both`，按窗口标题抓 Prefer 五页 + Editor（双主题截图组） |
| `dumpwins.ps1` / `uia_name.ps1` | 列进程全部窗口 / 用 UIA Name 读标题与矩形（`GetWindowTextW` 对 Qt 标题不可靠） |
| `verify_hover.ps1` / `verify_submenu.ps1` | 悬停冻结与右键子菜单的外部验证（QMenu 对 PrintWindow 不透明，用 `CopyFromScreen`） |

（历史上几十个 `fs*/popcheck*/stuckslide*` 一次性脚本已删；`tools\` 从未入 git，**不可找回**。）

**进程内自动化验证钩子**（比外部投递消息可靠——后者会被 Qt 按真实光标位置路由而失准）：

- `BILILOCAL_EMBED_TEST=1`：`EWindow` 每 300ms 注入鼠标事件（第 30/50 拍双击验证全屏闭环、第 35 拍单击验证暂停），配 `msg.log` 判读。
- `BILILOCAL_UI_TEST=prefer/editor/both`：自动打开 Prefer（逐页巡览，标题带页码「设置 - N」）与 Editor——**验证对话框用这个**，配 `tools\verify_ui3.ps1`。
- 悬停验证：每 60 拍循环「扫掠→(600,320) 压 14 拍（验证静止不解冻）→第 56 拍右键」，起播后自动跳 10%；旧闭环（双击全屏/单击暂停）需另加 `BILILOCAL_EMBED_CLICK=1`。`msg.log` 找 `hover: freeze/release`。
- `BILILOCAL_EMBED_TEST` **无条件编进发布 exe**（`BiliLocal.pro` 的 DEFINES），靠环境变量触发；源码注释已与事实对齐（`0f25480` 纠掉"正式构建不编译"的旧说法），之所以不改成 release 剔除，是因为悬停/子菜单等外部验证脚本都依赖发布 exe 里的钩子。

**GitHub Actions CI（`.github/workflows/ci.yml`；2026-10-25 MinGW 首绿，2026-10-26 起单 job 换 Qt 6.8）**：push 到 master 触发。runner 上 aqt 装 Qt 6.8.3 win64_msvc2022_64 + qt5compat（`qttools` 不是 6.x 模块名，lrelease 在 base 包里——坑 52；装完落盘验 `bin\Qt6Core.dll`），不指定 `-G`（坑 51），`BILILOCAL_BUILD_APP=OFF` 只编 5 个测试目标并 `ctest`（41 例）；Qt 走 actions/cache（key `qt-6.8.3-msvc2022-v1`，命中后秒级）。主程序编译与 hook-gate 留本机（CI 虚机 D3D11/GL 不可信）。查运行状态：认证 API 或 Actions 徽章 SVG（`actions/workflows/ci.yml/badge.svg`）。

**PS 5.1 铁律**：一律 `-NoProfile -ExecutionPolicy Bypass -File`；脚本存 UTF-8 带 BOM、**不写中文字面量**；`$Pid` 只读、`Move/Cli/lp` 是别名；`Start-Process -RedirectStandardError` 别配 `-Wait`；子进程会继承 stdout 管道（给 `UseShellExecute=$false` 的启动加 stdio 重定向，否则调用方假死）。

---

## 9. 环境与依赖

- Qt **6.8.3** msvc2022_64（**现役**，Phase 2）：`deps\6.8.3\msvc2022_64`；aqt 装：`aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qt5compat -O deps`。**qttools 不是 Qt6 的模块名**——lrelease/lupdate 已在 base 包里（坑 52）；**aqt 模块名错也可能 exit 0**，装完必须落盘验 `bin\Qt6Core.dll`
- Qt **5.15.2** mingw81_64 / msvc2019_64：`deps\5.15.2\`（**遗留**，v0.6.0-qt5-final 回滚重编用；Phase 3 清理时再裁）
- VS BuildTools 2022（Phase 1 实测本机已有，**无需新装**）：cl 14.44.35207（v143），Windows SDK 10.0.26100；CMake VS 生成器经 vswhere 自动定位
- MinGW **8.1.0**：`deps\Tools\mingw810_64\bin`
- VLC **3.0.24**：运行库在 `dist`。升级方法：官网下 win64 安装包静默解包（`/S`），替换 `dist` 的 `libvlc`/`libvlccore`/`plugins`，**必须把 plugins 里 Qt 的 `platforms`/`imageformats`/`mediaservice`/`styles` 四目录合并到顶层**；`lua\` 与 `opengl32sw.dll` 本播放器用不到（实测可去，**10-25 决定保留**，升级时按原样回填）；再按「先验证再删」裁掉 gui/stream_out/access_output 等对 libvlc 无用的类别（10-25 那次升级就是漏了验证、把新树放成嵌套 `plugins\plugins` 才多背 142M：替换后先改名/移动留证，启动播放确认无恙再删）。构建用头文件/导入库在 `deps\thirdparty`（3.0.21 SDK，libvlc API 兼容）。
- Python：`C:\Users\23239\.dsh\dsh-runtimes\dsh-next-primary-runtime\dependencies\python\python.exe`（系统 `python` 是 Store 存根，跑了等于没跑）
- VC 运行库（VLC3 需要）：**dist 已自带**（2026-10-25 从系统 System32 拷入 vcruntime140 123,472 B / vcruntime140_1 47,264 B / msvcp140 553,552 B，v14.50.35719.0，拷入后启动冒烟通过）。换机器无需再装；如需更新再取 [vc_redist.x64](https://aka.ms/vs/17/release/vc_redist.x64.exe)
- **deps 来源与归档（2026-10-25 补）**：
  - `deps\thirdparty`（**VLC 3.0.24 SDK**，同日从 3.0.21 刷新）→ 归档 `deps\archive\vlc-sdk-3.0.24-win64.zip`（SHA256 `7BD12CD7…`）。**注意**：官方 7z 的 lib 只有 MSVC `.lib`——现行 `lib/amd64` 里的 `.def`+`.dll.a` 是 `gendef dist\libvlc.dll` 提取导出表、`dlltool -m i386:x86-64 --as-flags=--64` 从**出货运行库本体**再生成的，`include/` 为官方 3.0.24 头；链接验证 = 41 例 + hook-gate 全绿、警告仍是那 2 条已知项。**Phase 1 增补（2026-10-26）**：`lib\msvc\` 里放了官方 `libvlc.lib`/`libvlccore.lib`（267,386 / 609,922 B）+ `vlc.lib`/`vlccore.lib` 转发桩（18/22 B），从 `deps\dl\vlc-3.0.24-win64.7z` 的 `sdk\lib\` 用 `7zr.exe e` 再取（MSVC 线直接链它，不再走 gendef/dlltool）
  - `deps\dl\vlc-3.0.24-win64.7z`：SHA256 `1ED59C09…`（download.videolan.org，`sdk/` 子树 112 条目）
  - `deps\archive\vlc-sdk-3.0.21-win64.zip`：旧 SDK 存档保留（SHA256 `857CE77B…`）
  - `deps\5.15.2\mingw81_64`（501M）：Qt 官方存档 5.15.2 的 mingw81_64 组件（download.qt.io/archive/qt/5.15/5.15.2/）
  - `deps\Tools\mingw810_64`（465M）：Qt 5.15.2 配套的 MinGW 8.1.0（download.qt.io 存档 Tools 分类）
  - `deps\dl\vlc-2.2.8-win64.zip`：SHA256 `DD2AD3B5…`（vmem 考古用，勿删）
  - Qt 6.8.3 msvc2022_64（`deps\6.8.3\`）：官方存档可随时 aqt 再下（命令见上），**不做本地压缩归档**；qt5compat 模块已含在内
  - Qt 5.15.2 mingw81_64（501M）+ msvc2019_64、MinGW 8.1.0（465M）：**遗留但暂留**（`v0.6.0-qt5-final` 回滚重编用）；官方存档仍可下载，Phase 3 定稿时裁决去留
- 网络盘：`Z:` = RaiDrive 挂载 WebDAV，读速 0.1–45 MB/s 抖动；读慢时播放器"未响应"属挂载问题

---

## 10. 迁移任务书（执行中——**Phase 1/2 已落地，剩 Phase 3 用户验收与定稿**）

> 2026-10-25 晚裁定：解除"先不迁移"，按本节执行。开工前**先读 §0 约束和 §11 影响清单**；每个 Phase 完成后给实测证据（41 例 + hook-gate + 该阶段验收）再进下一段；**渲染正确性的人眼清单必须用户本人过**（§2 验证分工）；门禁（pre-push + CI）红了不许推。
>
> **回滚锚点**：tag `v0.6.0-qt5-final`（打在 CI 全绿的 2136fe5 上）；`deps\Tools\mingw810_64` 与 `deps\5.15.2` 原封不动，任何阶段可整体回退重编。

**Phase 1 · 换编译器（保持 Qt 5.15，风险最低）**
- 装 VS BuildTools（VC + Win SDK）；Qt 用 aqt 换 `win64_msvc2019_64`（同一条命令换 target，清单已验证存在）。
- CMake 侧加 MSVC 配置（generator `Visual Studio 17 2022` 或 Ninja+cl，`-A x64`）；导入库**直接用官方 3.0.24 SDK 的 `libvlc.lib`**——gendef/dlltool 流程是 MinGW 专属，MSVC 不需要（`deps\thirdparty\lib` 里官方 `.lib` 已随 3.0.24 sdk 解包放在 `deps\dl\vlc-3.0.24-extracted`，若已清理则从归档 zip 再取）。
- 预期工作：一轮 MSVC 警告清理（历史纪录 42→3，值得信任）；坑 11/12 的 MinGW 特调天然合法不用动。
- 验收：41 例 + hook-gate + 用户开关一次播放器；CI 加一条 msvc job（runner 预装 VS，aqt 装 msvc 版 Qt 即可）。

> **Phase 1 实况（2026-10-26，✅ 机器侧全过、⏳ 只差用户开关验收）**：
> VS BuildTools 本机已有（cl 14.44 / SDK 10.0.26100，零安装）；Qt msvc2019_64 aqt 装入 `deps\5.15.2\msvc2019_64`；
> 官方 `.lib` **不是**从归档 zip 再取（那是 thirdparty 快照，只有再生 `.dll.a`），而是从 `deps\dl\vlc-3.0.24-win64.7z`
> 的 `sdk\lib\` 用 7zr 再取入 `deps\thirdparty\lib\msvc\`。CMakeLists 加 MSVC 分支（全局 `/utf-8`、NOMINMAX、
> 链接分流），顺带修了坑 49/50——其中坑 50 是 fixture 反斜杠缺失的真修。证据：MSVC **0 警告**、ctest **41/41**、
> hook-gate 对 `dist-msvc` PASS、pre-push MinGW 41 例绿、CI 双 job **绿**（`25e3751`；首跑 `f81e6ee` 挂在
> runner 无 VS2022，坑 51 修掉）。
> 产物 `dist-msvc\BiliLocal.exe` 878,080 B / `b4e39ad1…`；**dist 本体未动**（`6c55f692…`，MinGW 线照常可用）。
> 用户开关验收过后才进 Phase 2。**✅ 2026-10-26 千夏开关验收通过（"没问题"），Phase 1 结案。**

**Phase 2 · Qt 5.15 → 6.8 LTS（核心会话）**
- `Parse.cpp:9-48` 的 `QTextCodec` → Core5Compat 的 QTextCodec（**`QStringConverter` 只覆盖 UTF 系，GBK 老弹幕文件必须靠 Core5Compat**——漏了的表现是"某些 xml 静默 0 条弹幕"，坑 35 家族）。
- `Local.cpp:188` 的 `AA_UseHighDpiPixmaps`（Qt 6 恒开，删行）；`Local.cpp:194` 的 `AA_UseOpenGLES` 语义重验。
- **六个 shader GLSL 1.20 → 330 core**（`varying`→`in/out`、`texture2D`→`texture`；坑 36/40 的失败形状 = "链接不上但照画"，排查入口是 `msg.log` 的 `Program Link Failed`）。
- DPI：`d98327b` 的 PassThrough 取整在 Qt 6 恒开高 DPI 下重测（150% 屏）。
- CI 配方换成 6.x + MSVC；**qmake 遗留在本阶段删除**（`.pro`×5、`build\` 与 4+1 个 qmake 测试树、run-tests.ps1 改用 ctest）。

> **Phase 2 实况（2026-10-26，✅ 机器侧全绿、⏳ 等 Phase 3 人眼清单）**：Qt **6.8.3**（aqt，`-m qt5compat`；
> qttools 不是 6.x 模块名，lrelease 在 base 包里——坑 52）。清单全落地：QTextCodec 显式 include 走
> Core5Compat（GBK 老文件保解码）；`AA_EnableHighDpiScaling`/`AA_UseHighDpiPixmaps` 删行、`AA_UseOpenGLES`
> 保留（env 门控，Qt6 官方包已无 ANGLE，注记存档）；六 shader → 330 core（`Program Link Failed`=0）；
> DPI 探针 **dpr=1.50 / 1707×960** 无回退。清单外的实收：事件坐标 10 处（`position()`/`globalPosition()`）、
> `QTextStream::setCodec`→`setEncoding`、`QApplication::desktop()`×2→`screen()`（坑 54）、
> QtConcurrent 模板实参按 6.8 签名重写（坑 53）、`OpenGLRenderPrivate.h` 补 `<QtOpenGL>`、
> CMake 换 Qt6 组件 + **qm 生成首次真正挂上 ALL 目标**（此前 CMake 树从没编出过 qm，坑记在 CMakeLists 注）。
> qmake 遗留删净（.pro×6 + 树×7），run-tests.ps1 改 ctest 实测走通，CI 换 Qt6 单 job。
> 证据：**0 警告 / ctest 41/41 / hook-gate PASS / DPI 1.50**，提交 `8bb173f`。

**Phase 3 · 用户验收（人眼清单）**
弹幕可见/颜色/描边/滚动速度、悬停冻结（高密度段，`hook-gate.ps1` 先跑）、**右键弹幕数据块（用户专属动作）**、全屏进出、断点续播、双主题、Prefer 五页、屏蔽规则生效。全过后 dist 定稿、清理旧构建树。

> **Phase 3 实况（2026-10-26）**：首轮验收完成——**功能清单全过**（千夏："基本功能没问题"）；性能两项不达标
> （反应变慢、关闭长卡顿），经他自己会话产生的 4 份 WER 转储定位为图集析构链的退出 GL 删除（坑 55，含两次
> 真崩溃），已修复实测（关闭 2.0–2.1s、播放 CPU 7.1%/核）。**待办 = 千夏复验这两项 → 全过后的收尾清单**：
> dist 对调定稿（Qt6 转正当班）→ 清死树（`build-cmake\`、`build-cmake-msvc\`）→ deps 裁决（Qt5×2 + MinGW
> 共 1.4G 去留，默认倾向留）→ Qt5 杂项对账（`qt.qm`、`d3dcompiler_47`、MinGW 三件套）。

**Phase 4 · VLC 4（独立会话，等上游 4.0 final）**
与 Qt 解耦。必改点已知：`libvlc_video_set_subtitle_file`（现存的 2 条警告在 4.x 头文件里会变硬错误）。可先做纸面差异清单，把考古变成核对。

**每阶段的纪律**：独立提交（Phase 名进提交信息）→ 跑全门禁 → 证据进文档 → 再进下一段。任何一段失败：`git reset`/`revert` + 从 tag 重编即回滚，`deps\` 的 Qt5/MinGW 树不受影响。

**迁移完成后回到的账**（现在不做）：
1. 大 xml 异步解析：仅当卡顿可感知（别用 `QFutureWatcher`，见 §7-1）。
2. Danmaku 本体独立成测：服务定位器换代（可并入迁移收尾）后桩面消失，自然解锁。
3. 待拍板的加功能：编辑器按弹幕类型上色（模式 4/5/6 淡色底）、底栏静音/上一集/下一集/截图按钮。
4. 渲染/输入路由重构：仍按 §10-2 旧规则——没有现象和证据就不动。

> 2026-10-25 裁定并结案：**dist 不再瘦身**（`lua\` 1.1M + `opengl32sw.dll` 21M 实测可去，决定保留）；**Z: 长时段观察不再挂账**（RaiDrive 链路抖动，日常播到不对劲再查）。

---

## 11. Qt 6 迁移影响清单（**已全部落地（Phase 1+2，2026-10-26）**——本节保留作技术底账）

- **工具链耦合**：Qt 6 Windows 官方包只有 MSVC——MinGW 路线意味着同时换编译器（新 mingw-w64）。~~qmake → CMake~~ **已完成**（`15b2667`）；~~编译器换代~~ **已完成**（Phase 1 `f81e6ee`：MSVC v143 + 官方 .lib）。
- **全库命中清单**（除下列各项外，QRegExp / QDesktopWidget / qrand / toTime_t / `QString::SkipEmptyParts` / `toList()`/`toSet()` / `QFontMetrics::width` 均 **0 命中**）：

| 项 | 位置 | Qt 6 处置 |
| --- | --- | --- |
| `QTextCodec`（唯一硬移除项） | `Parse.cpp:9-48`（弹幕文件编码嗅探） | 换 `QStringDecoder` 或引 Qt6::Core5Compat；全库仅此一处 |
| `AA_UseHighDpiPixmaps` | `Local.cpp:188` | Qt 6 恒开该行为，删行即可 |
| `AA_UseOpenGLES` | `Local.cpp:194` | 语义要重验（Qt 6 的 GL/ANGLE 策略变了） |
| `QOpenGLWindow`/`QOpenGLFunctions` | `Render/OpenGL` 5 个文件 | 类仍在；上下文/GLSL 策略重验——§6-36/40 的坑要重新走一遍 |
| `QtConcurrent::blockingMappedReduced` | `Parse.cpp` | Qt 6 同名（Qt6::Concurrent），签名未变 |
| `Qt::SkipEmptyParts` | `Parse.cpp:379` | 已是 Qt 6 兼容写法 ✓ |

- **高风险区**：`d98327b` 的 DPI 工作（150% 屏 PassThrough 取整、逐档 dpr 绘制）建立在 Qt 5 的开关式高 DPI 之上，Qt 6 恒开高 DPI 后必须重测。
- **不受影响**：libvlc 侧（VPlayer/Embed 直出）与 Qt 版本无关；§6-18/23/24/25 的输入路由坑是窗口系统层面，大半仍成立。
- **量级预估（已销账）**：CMake 化（`15b2667`）→ 编译器切换（Phase 1）→ QTextCodec/Core5Compat 改写 + shader 330 移植 + 渲染层/DPI 重验（Phase 2，探针实证 1.50）全部落地；渲染正确性的人眼确认归 Phase 3 清单。

---

## 附：快速回滚

- **源码回滚**：`git checkout v0.6.0-qt5-final` + deps 的 Qt5/MinGW 工具链 = 回到 Qt6 迁移前（dist 里的 Qt5 线 exe 就是这条线的产物）；`git checkout v0.5.0-vmem` → 自备 VLC 2.2.8 重编 = 最初的 vmem 线。
- **旧线运行时**：VLC 2.2.8 运行库与免编译 vmem exe 已于 2026-10-24 删除。真要跑 vmem 线：从 tag 重编 exe + 自行从 VideoLAN 官方存档下载 VLC 2.2.8 win64 + 配置改 `Output=Window`。vmem 线是已知崩溃/高 CPU 路线，仅作考古。
- **单轮回滚**：任意一轮都是独立提交，`git revert <hash>` 即可；`30466b0`（shader）与 `7bf39d2`（去重）互不依赖。
- **全历史恢复**：`git clone https://github.com/hanliu660-gif/BiliLocal.git <目录>`（或从本地 `src\` 直接拷贝）。
