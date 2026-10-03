# VMARGS18：参数类型推导修复

构建标识：`FC-0.5.5-VMARGS18-20261002-N`。

## 对应这次日志

COMPILE17 解决了上一轮的 GetObject 宏和 kPlayer 问题，但新日志表明继续编译到
`SexLab16.cpp:135` 时，在 `FunctionArguments.h:82` 实例化失败。
模板参数中的前五个类型都是 `RE::Actor*&`，不是 Papyrus 所需的 `RE::Actor*` 值。

原因：`actors[i]` 是一个左值；通用转发引用把它的类型推导为引用，接着传给一个
只在 `is_return_convertible<Args>...` 全部满足时才有定义的模板。
`is_parameter_convertible` 又明确排除了引用，因此只有前置声明，没有可实例化的类。
这不是 VS2026 安装、代理、vcpkg 下载或 CommonLib.lib 编译问题。

## 修复

新增 `include/fc/VMArguments18.hpp`。参数先进入按值参数包（复制调用者的指针／字符串），
再作为这些副本的右值传递给 `RE::MakeFunctionArguments<Args...>`。
此处不是 `std::move(actors[i])`，不会移动调用者存储的对象；typed nullptr 保持 Actor* 类型。
两个调用入口均使用该适配器。空参数调用仍走 CommonLib 的 ZeroFunctionArguments 特化。

新增 `src/VMArgumentsContract18.cpp`，构建真实 DLL 时强制编译这两种调用的函数体。
保留 `ApiContract17.cpp` 和全部前次修复；没有通过注释掉调用或关闭功能绕过编译。

## 为什么上一轮主机测试没有发现

上一轮模拟接口是一个无类型约束的 `MakeFunctionArguments(T&&...)`，只返回空指针，
因此把真实 CommonLib 会拒绝的引用类型错误放过去了。
这轮将它换为明确拒绝引用的测试替身，使用上一版完整源文件复现失败；
修正版完整源文件、同一适配器和参数保存测试通过。测试替身不等于真实引擎。

## 未改动的部分

Engine.cpp（含暂停和音频实现）、PCH.h、Overlay.cpp、跟随／军团／任务代码、
FreedomControlRuntime.esp、默认 INI、vcpkg.json 均与 COMPILE17 基底字节一致。
生物目录只有 build 标识更新，条目不变。构建脚本仅更新版本文字／安装包名，
增加必要新文件校验，保留 VS2026、缓存和已有配置处理。

## 核对来源

公开 CommonLibSSE-NG 源码文档：
- https://ng.commonlib.dev/_function_arguments_8h_source.html
- https://ng.commonlib.dev/_type_traits_8h_source.html

用户日志标明本机 checkout 为 a898f469851c464d05137bb74b069dd234897643。
当前容器未能下载该固定提交／Windows SDK，因此没有声称完整原生编译通过。
源码头部约束与用户日志中的推导失败一致；最终真实头文件编译由本机 BUILD.bat 执行。
