# BiliLocal 64 位自建版 · 交接文档

> **更新时间**：2026-10-25（**第十七版：模型层单测**——tst_shield/tst_load 共 21 例入库+负向对照、一键测试门禁 run-tests.ps1、钩子注释纠偏、VC 运行库入 dist、deps 归档与来源记录、Qt 6 纸面预案（仅备案未启动）；代码事实按 2026-10-25 20:09 前后实测。同日修订：验证分工表述按用户澄清收窄——"只能人眼验证"实为"仅真实右键到弹幕这一个动作需要本人"，见 §2/§3/§6-14，源码头注同步纠偏 `5ecd888`；GUI 钩子门禁 hook-gate + 本地 fixture + pre-push 门禁落地（见 §8，坑 45/46））
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
| `build\` | 影子构建（qmake + mingw32-make），产物 `build\release\BiliLocal.exe` |
| `build-test\` | 单测构建树（`src\test\tst_parse.pro`，只链 Parse 与 Utils 两个 TU）→ `release\tst_parse.exe`（9/9）。跑前把 `deps\5.15.2\mingw81_64\bin` 与 `deps\Tools\mingw810_64\bin` 放进 PATH |
| `build-test-config\` | 单测构建树（`src\test\tst_config.pro`，只链 `Config.cpp`）→ `release\tst_config.exe`（7/7）。`qmake ../src/test/tst_config.pro -spec win32-g++ "CONFIG+=release"` 后 make，同样要设 PATH |
| `src\test\tst_config.*` | Config 的 7 例单测：往返、缺键写回不冲掉别的键、2 线程 × 500 次并发写、坏配置留备份、存档是合法 JSON |
| `build-test-shield\` | 单测构建树（`src\test\tst_shield.pro`，链 `Config.cpp`+`Shield.cpp`）→ `release\tst_shield.exe`（9/9）。qmake/make 流程同上 |
| `src\test\tst_shield.*` | Shield 的 9 例单测：配置加载跳过空/坏正则、m/u/t 往返、isBlocked 模式/颜色/嘉宾全语义、aboutToSave 落盘往返。**测试里重建 Shield 前必须先断开 aboutToSave**（连接无上下文对象，见文件头注） |
| `build-test-load\` | 单测构建树（`src\test\tst_load.pro`，真链 Load/Parse/Config/Utils）→ `release\tst_load.exe`（12/12）。同上 |
| `src\test\tst_load.*` | Load 端到端 12 例：真文件真嗅探真解析，假 Danmaku 收结果；覆盖 BOM/前置空行两大历史回归 + 四格式分支 + AcFun 兜底 + 缺文件报错 + 队列顺序 + 延迟平移 |
| `deps\archive\` | 依赖归档：`vlc-sdk-3.0.21-win64.zip`（= `deps\thirdparty` 的快照，110,162 B，SHA256 `857CE77B…`；来源与再生途径见 §9） |
| `dist\` | **可直接运行的成品**：exe + Qt5 + VLC 3.0.24（FFmpeg 8.1.2）+ 插件 + 翻译 |
| `deps\` | 工具链：Qt 5.15.2 (mingw81_64)、MinGW 8.1.0、VLC SDK 头文件/导入库（在 `deps\thirdparty`）。VLC 运行库只存在于 dist；`deps\dl` 仅剩 `vlc-2.2.8-win64.zip`（vmem 考古用） |
| `tools\` | 调试/测试脚本（14 个，见 §8） |
| `test\` | 已清空（2026-10-25 清点：`embed.xml` 与 `rec.*` 符号链接均已不存在）。hook-gate 的弹幕 fixture 与本地视频由 `tools\gen-media.py` 按需生成到 `tools\.media\`（不入 git，见 §8） |
| `%LOCALAPPDATA%\BiliLocal\Config.txt` | 运行时配置（JSON） |
| `%LOCALAPPDATA%\BiliLocal\cache\msg.log` | 日志（超 10MB 自动轮转成 `.old`） |

> **`dumps\`（WER 转储）会自己长回来**：2026-10-24 清过一次 4.9G，10-25 又清两次（3 个 1.9G、2 个 1021M），**每崩一次写一份 ~0.5G**。排查完立刻删；目录存在不代表当前 exe 有毛病（那些 dump 都来自部署新 exe 之前的旧进程）。
>
> 2026-10-24 还删了：`dist-vlc2\`（旧 vmem 线）、`build-dbg/fp\`（旧构建树）、`embed-wt\` worktree、`BiliLocal-64bit.patch`、离线 bundle（`backup\`）。**回滚材料只在 git 历史 + GitHub 远端里，别再找这些目录。**

---

## 2. 当前状态（2026-10-25 18:46 实测）

- **exe**：`dist\BiliLocal.exe` = **931,840 字节**，SHA256 **`2C0005D3…`**（构建自 `0f25480`，20:00 前后部署；该提交只改注释，exe 与 `build\release\BiliLocal.exe` 逐字节一致）。**构建可复现性（2026-10-25 18:46 实测三组）**：①`dist` 与 `build\release` 逐字节相同（每次部署都 sha256 校验）；②同一构建树连续两次全量 clean 重建：931,840 B，只差 **4 字节**（offset 137/138、217/218 = PE 时间戳/校验和）；③把 `git archive HEAD` 导到**另一个目录**重建：尺寸同为 931,840 B，但差 **2,692 字节**（4 字节 PE 头 + 2,688 字节在代码/数据区），成因未定。⇒ **尺寸可复现、字节不完全可复现：判断"是不是同一份源码"只能看 git 提交号，别比哈希。**
- **源码**：`master` = 最新提交（**从 05:30 起改为不写死文档哈希**——写死的值在提交瞬间就过期，当前值用 `git log -1 --oneline` 取）；**最后一个代码提交 = `5ecd888`**（tst_parse 头注纠偏，纯注释）← `0f25480`（钩子注释纠偏，纯注释）← `0c74654`（tst_shield/tst_load 新单测）← `7bf39d2`（同一视频只加载一份弹幕）← `30466b0`（弹幕 shader 修好、白方块消失）← `600b369`（审计第 5 批）← `9edac7c`（审计第 4 批）← `1b10dc0`（审计第 3 批）← `2f8a1d1`（审计第 2 批）← `cd3b0fe`（审计第 1 批）← `d98327b`（UI/DPI）← `81aab8f` ← `a5f95fe` ← `a9fbcb3` ← `63f9a4b` ← `8d9c8ec` ← `44fbb3e` ← `7f8685a` ← `473ba62` ← `3f86f74` ← `3155020` ← `c834050` ← `cbc0b42` ← `c8679e6` ← `d7847e8` ← `49c173c` ← `4702ab1`。上游 `9d1cb05`；回滚 tag `v0.5.0-vmem` = `46b68b9`；远端 `github` = `github.com/hanliu660-gif/BiliLocal`。
- **单测**：四个目标共 **37 例全绿**（2026-10-25，`tools\run-tests.ps1` 一键复跑）：`tst_parse` **9/9**、`tst_config` **7/7**、`tst_shield` **9/9**、`tst_load` **12/12**。**负向对照做了两个**：①把 `tst_config` 两处 `QWriteLocker locker(&lock);` 注释掉重建，`concurrentWriters` 直接 SIGSEGV（`rc=139`）；②把 `Load.cpp` 嗅探的 `.trimmed()` 去掉重建，`blankLineBeforeXml` 变红（0 条弹幕）、其余 11 例不受影响 ⇒ 用例不是空跑。GUI 钩子剧本另有 `tools\hook-gate.ps1` 断言三个日志标记（见 §8）。List 与 Danmaku 本体因依赖整套服务定位器 + APlayer（会拖进 VPlayer/libvlc）暂不独立成测（见 §10-4）。
- **编译警告**：**42 → 3**（连清两轮：`a5f95fe` 清 6 个位置，`81aab8f` 清最后 1 个；`0c74654` 新增的两个测试目标构建 **0 警告**）。剩下 3 条是**刻意留的**：`-Wimplicit-fallthrough` 1 条（`src\src\Model\Running.cpp:283` 的 `case 1` 落到 `case 0`，入队本来就该两模式同路）、`libvlc_video_set_subtitle_file` 2 条（`src\src\Player\VPlayer.cpp:476`，VLC 3 标弃用但照常可用，换 VLC 4 才必须改）。
- **i18n**：源码全英文串，中文走 `dist\locale\zh_CN\bililocal.qm`（187/187 全译，含繁体）。改 UI 文案后：lupdate → 补译 → lrelease → 部署 qm。
- **运行库**：VLC **3.0.24**（d3d11 直出）；`dist\plugins` 里**混住着 Qt 的四个插件目录**（`platforms\` `imageformats\` `mediaservice\` `styles\`），整目录替换 VLC 时必须把它们合并到顶层，否则 Qt 起不来（流程见 §9）。dist 只带 exe 实际导入的 5 个 Qt DLL（Concurrent/Core/Gui/Network/Widgets）。
- **配置**：`Output=Embed` 已是唯一输出（键保留但代码不再读）；`Immediate=True`；`HardwareDecode` 已无消费者（d3d11 固定开启，`Arguments` 手写 `--avcodec-hw=`/`--vout=` 仍可覆盖）。
- **备份**：git 历史 = 本地 `src\.git` + GitHub 双保险；vmem 旧线源码完整在 tag `v0.5.0-vmem`（重编需自备 VLC 2.2.8）。
- **未决事项**：无（10-25 裁定：dist 不再瘦身、Z: 长时段观察结案）。
- **验证分工**（2026-10-25 用户澄清，收窄旧表述）：agent 工具几乎全能自验——日志取证（`hover: freeze/release`、`Load: processed … pool=N`、`Program Link Failed` 计数）、进程内钩子注入（双击全屏/单击暂停/悬停扫掠/拍点右键）、时序验证、对话框巡览截图；**用户本人只做"真实右键点到弹幕"这一个动作**（物理点击过不了三层窗口栈的焦点/显示盲区，合成事件替代不了，见 `a4b5acf`）与主观体验类验收。

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

---

## 3. 怎么用（用户视角）

产品功能、快捷键、断点续播、自动续播、目录记忆等说明在 `src\README.md` 的 **Usage** 一节（那是"换台机器也成立"的产品事实，本节不再重复）。

> **验证注意**（2026-10-25 按用户澄清收窄）：弹幕层是 QOpenGLWindow，对 BitBlt/PrintWindow 抓屏**不可见**——截图全黑不代表画面黑（视频/底栏用 `tools\pwc.ps1` 能抓到）。**截图不能当弹幕证据，但弹幕并不因此靠人眼**：行为面（事件路由、悬停冻结、加载计数、shader 链接、时序）全走日志 + 进程内钩子，agent 可自验（见 §8）；**只有"真实右键点到弹幕"这一个物理动作必须用户本人**——合成事件全通、真实鼠标点击过不了三层窗口栈的焦点/显示盲区（`a4b5acf`/§6-23~25）。

---

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

# 部署（必须先停进程，否则 cp 报 Device or resource busy）
Stop-Process -Name BiliLocal -Force -ErrorAction SilentlyContinue
Copy-Item build\release\BiliLocal.exe ..\dist\ -Force
```

- 源码里有中文 `QStringLiteral`/`tr()` 的文件必须保持 **UTF-8 BOM**；编辑工具会吃掉 BOM（见 §6-29）。
- 改了 `.pro` 或公共头文件后报 `PCH files were found, but they were invalid`：删 `build\release\Common.h.gch` 再 make（**它是目录**，`rm -f` 会报 `Is a directory`）。
- 全量干净重建：`mingw32-make clean && rm -rf release/Common.h.gch`。
- 回滚点：`git checkout v0.5.0-vmem` + 重编 = 回到最初的 vmem 线（运行库已删，见文末「快速回滚」）。

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
46. **build 树的 exe 直接跑不起来**：加载器不认 PATH 兜底（裸跑退出码 `0xC0000135`，或静默卡在加载器错误框上进程活着但零日志）。要在 build 树调试：`cp dist/*.dll build/release/` + `mklink /J build\release\plugins dist\plugins`，诊断完 `rm *.dll` + `rmdir plugins`（**junction 只能 rmdir，rm -rf 会穿透删掉 dist\plugins**）。hook-gate 已内置 `QT_QPA_PLATFORM_PLUGIN_PATH`/`VLC_PLUGIN_PATH`/PATH 三个环境变量，但实测仍需 DLL 在 exe 旁

---

## 7. 已知限制（不是待办，是设计取舍）

1. 大 xml（数万条）解析在 GUI 线程同步执行，可能卡数百 ms~秒级（Parse 同步化后引入；本地录播 xml 无感）。真要异步必须用可靠机制——别回到 `QFutureWatcher`；注意 `Load` 的 `onFinish` 里有 `&task` 引用捕获，异步化会悬空。
2. 倍速按钮长按右键的 350ms 阈值 / 3.0 倍率是拍脑袋值，可按手感调。

---

## 8. 工具脚本（`tools\`，16 个）与验证钩子

| 脚本 | 用途 |
| --- | --- |
| `run-tests.ps1` | **一键门禁**：4 个单测目标（parse/config/shield/load）逐个 qmake+make+运行，输出各自 Totals，任一失败退出码 1（`-NoBuild` 只跑不编） |
| `hook-gate.ps1` | **GUI 钩子门禁**：以 `BILILOCAL_EMBED_TEST=1` 起播放器播本地 fixture，断言 `msg.log` 三个标记（`seeked to 10%` / `hover: freeze` / `hover: release`），全中才退出码 0；`-Media` 可换 Z: 真实录播做对照 |
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

**PS 5.1 铁律**：一律 `-NoProfile -ExecutionPolicy Bypass -File`；脚本存 UTF-8 带 BOM、**不写中文字面量**；`$Pid` 只读、`Move/Cli/lp` 是别名；`Start-Process -RedirectStandardError` 别配 `-Wait`；子进程会继承 stdout 管道（给 `UseShellExecute=$false` 的启动加 stdio 重定向，否则调用方假死）。

---

## 9. 环境与依赖

- Qt **5.15.2** mingw81_64：`deps\5.15.2\mingw81_64`
- MinGW **8.1.0**：`deps\Tools\mingw810_64\bin`
- VLC **3.0.24**：运行库在 `dist`。升级方法：官网下 win64 安装包静默解包（`/S`），替换 `dist` 的 `libvlc`/`libvlccore`/`plugins`，**必须把 plugins 里 Qt 的 `platforms`/`imageformats`/`mediaservice`/`styles` 四目录合并到顶层**；`lua\` 与 `opengl32sw.dll` 本播放器用不到（实测可去，**10-25 决定保留**，升级时按原样回填）；再按「先验证再删」裁掉 gui/stream_out/access_output 等对 libvlc 无用的类别（10-25 那次升级就是漏了验证、把新树放成嵌套 `plugins\plugins` 才多背 142M：替换后先改名/移动留证，启动播放确认无恙再删）。构建用头文件/导入库在 `deps\thirdparty`（3.0.21 SDK，libvlc API 兼容）。
- Python：`C:\Users\23239\.dsh\dsh-runtimes\dsh-next-primary-runtime\dependencies\python\python.exe`（系统 `python` 是 Store 存根，跑了等于没跑）
- VC 运行库（VLC3 需要）：**dist 已自带**（2026-10-25 从系统 System32 拷入 vcruntime140 123,472 B / vcruntime140_1 47,264 B / msvcp140 553,552 B，v14.50.35719.0，拷入后启动冒烟通过）。换机器无需再装；如需更新再取 [vc_redist.x64](https://aka.ms/vs/17/release/vc_redist.x64.exe)
- **deps 来源与归档（2026-10-25 补）**：
  - `deps\thirdparty`（VLC 3.0.21 SDK，1.4M）→ 已归档 `deps\archive\vlc-sdk-3.0.21-win64.zip`（SHA256 `857CE77B…`，18 条目 = `include/vlc/*.h` + `lib/amd64`）。再生途径：VideoLAN 官方存档 libvlc 3.0.21 win64 7z 里的 `sdk\`
  - `deps\5.15.2\mingw81_64`（501M）：Qt 官方存档 5.15.2 的 mingw81_64 组件（download.qt.io/archive/qt/5.15/5.15.2/）
  - `deps\Tools\mingw810_64`（465M）：Qt 5.15.2 配套的 MinGW 8.1.0（download.qt.io 存档 Tools 分类）
  - `deps\dl\vlc-2.2.8-win64.zip`：SHA256 `DD2AD3B5…`（vmem 考古用，勿删）
  - Qt/MinGW 近 1G **不做本地压缩归档**：官方存档仍可下载，本地快照的维护成本大于收益；真正难再生的是 thirdparty，已归档解决
- 网络盘：`Z:` = RaiDrive 挂载 WebDAV，读速 0.1–45 MB/s 抖动；读慢时播放器"未响应"属挂载问题

---

## 10. 下一步建议

1. 大 xml 异步解析：**仅当**哪天录播 xml 大到可感知卡顿时再做（别用 `QFutureWatcher`，见 §7-1）。
2. 渲染层（`EmbedPrivate`/`ARender`/`OpenGL*`）：已在 `600b369` 修过一轮（图集纹理在 paintGL 里删、析构补齐 surface/过滤器/连接、延迟 nudge 加存活标记），**再动要先讲清现象并给证据**。
3. 待用户拍板的**加功能**项（没批准前不要动）：编辑器按弹幕类型上色（模式 4/5/6 淡色底）、底栏静音/上一集/下一集/截图按钮。
4. 模型层剩余覆盖（List/Danmaku 本体）：独立成测需要更大的桩面（APlayer 会拖进 VPlayer/libvlc），暂缓；现有 37 例已盖住嗅探与屏蔽语义这两个 bug 高发区。

> 2026-10-25 裁定并结案：**dist 不再瘦身**（`lua\` 1.1M + `opengl32sw.dll` 21M 实测可去，决定保留）；**Z: 长时段观察不再挂账**（RaiDrive 链路抖动，日常播到不对劲再查）。

---

## 11. Qt 6 迁移纸面预案（未启动，仅备案）

> 原则：**先不动**（2026-10-25 用户裁定）。触发条件：安全事件、Qt 5.15 官方二进制不可得、或新硬件逼着换工具链。以下为 2026-10-25 全库 grep 的实测排查，真要迁移时按单核对。

- **工具链耦合**：Qt 6 Windows 官方包只有 MSVC——MinGW 路线意味着同时换编译器（新 mingw-w64）和构建系统（qmake → CMake）。想解耦风险，第一步只换编译器（保持 Qt 5.15），框架另算。
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
- **量级预估**：CMake 化 + QTextCodec 改写 + 渲染层重验 ≈ 2–4 个会话；动手前先重读本文件 §6。

---

## 附：快速回滚

- **源码回滚**：`git checkout v0.5.0-vmem` → 重编（回到最初的 vmem 线）。
- **旧线运行时**：VLC 2.2.8 运行库与免编译 vmem exe 已于 2026-10-24 删除。真要跑 vmem 线：从 tag 重编 exe + 自行从 VideoLAN 官方存档下载 VLC 2.2.8 win64 + 配置改 `Output=Window`。vmem 线是已知崩溃/高 CPU 路线，仅作考古。
- **单轮回滚**：任意一轮都是独立提交，`git revert <hash>` 即可；`30466b0`（shader）与 `7bf39d2`（去重）互不依赖。
- **全历史恢复**：`git clone https://github.com/hanliu660-gif/BiliLocal.git <目录>`（或从本地 `src\` 直接拷贝）。
