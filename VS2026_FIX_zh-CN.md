# VS2026 构建

本包继续使用 Buildfix8 的构建路径：检测 VS2022/2026、CMake 生成器能力，使用 x64 原生工具集，保留 vcpkg/CommonLib 缓存。

本版新增 src/Legion.cpp；请运行 BUILD.bat 编译，不要只打包旧 DLL。源码内容指纹会阻止“旧文件日期让增量构建误以为不用更新”的旧包流入新 ZIP。

需要定向修复时使用保留的 REPAIR_BUILD8.bat，不要删除 CommonLib 或全部 build。成功 ZIP 名称见根目录 README。
