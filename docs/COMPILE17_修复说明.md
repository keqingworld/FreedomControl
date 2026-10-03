# COMPILE17 编译错误修复

构建标识：FC-0.5.5-COMPILE17-20261002-M。
来源包 SHA256：88d16a333756fd92c0bebf9bb6ea2dcb1133a7dddf7c6ee5a39773147a3f185f。
来源：FreedomControl-0.5.5-SEXFAST16-FULL-20261002-L.zip。

## 从本次日志能确认什么

依赖安装、CommonLibSSE.lib 构建成功；失败发生在 FreedomControl 自身 C++ 编译阶段。
第一处是 Engine.cpp:370：GetObjectA 不是 RE::BGSDefaultObjectManager 的成员。
slot 未初始化和无法取消引用是它的后续错误，不是另外两个根因。
第二处是 SexLab16.cpp:105/106：kPlayer 未声明。
InputPolicy13.hpp:69 触发的 C4244 是警告，不是本次编译停止的根因。

## 修正

1. PCH 原先先包含 CommonLib，再包含 Windows.h。Windows GDI 的 GetObject 宏会把后续同名 C++ 调用展开为 GetObjectA/GetObjectW。新增可重复包含的 Win32MacroCleanup17.hpp，在 CommonLib 入口之前和 Windows.h 之后清除该宏；没有把调用改成错误的 GetObjectA，也没有移除音频逻辑。
2. kPlayer 仅存在于 Engine.cpp 的匿名命名空间，不能由 SexLab16.cpp 跨翻译单元引用。后者已经检查玩家指针，现在直接使用 player->GetFormID()。没有往测试替身中偷偷加入全局 kPlayer 来掩盖错误。
3. WheelStream 使用 float 零值，没有禁用编译器警告。
4. ApiContract17.cpp 加入实际 DLL 目标，编译时检查 SDK 宏没有泄漏、CommonLib 音量类别槽位和玩家 ID 的类型相符，不添加运行时 Hook。

## 新增验证的证据边界

compile17_regression_tests.py 能对来源 ZIP 的原始 PCH、原始音频函数，以及完整 SexLab16.cpp 分别执行负面对照。旧文件必须出现预期 GetObjectA/GetObjectW/kPlayer 编译错误，新文件必须通过。音频函数保留原实现，并在模拟类别上检查开关、重复打开、原先静音、缺失类别等分支。

这组检查使用真实 GCC/Clang 编译器、真实 fmt 头文件，但 Windows/RE/Papyrus 是显式测试替身。它能验证本次宏与作用域错误，不能证明游戏 ABI、真实音频设备、动画系统或整个 DLL 已编译运行成功。

## 未改动的范围

未更改快速启动的行为、暂停/声音处理逻辑、军团/生物目录条目、任务/世界规则、ESP 记录、共存存档格式。目录仅更新构建标识。未升级 CommonLib、vcpkg 基线或 VS 工具集。

音频仍沿用 0.5.5 的主类别静音方案；不能把这次编译修正说成底层音频播放游标冻结。

## 外部核对

微软 GetObjectA 文档说明 wingdi.h 使用 UNICODE 条件别名：
https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getobjecta

上游 ng 分支的 BGSDefaultObjectManager.h 区分 DefaultObjectID 的 T** 返回值和其他重载：
https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/ng/include/RE/B/BGSDefaultObjectManager.h

上游 ng 分支不是用户固定提交的替代；本机 API 编译检查仍由实际 CommonLib 头文件决定。
