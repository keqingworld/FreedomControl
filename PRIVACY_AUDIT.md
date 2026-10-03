# Privacy Audit

发布前自动检查通过。

已排除：
- 历史 validation / history 日志与沙箱路径
- build / dist / vcpkg_installed / .git
- DLL / PDB / OBJ / LIB / EXE
- build-settings.json / commonlib.lock.txt
- 旧发布哈希与临时验证输出

扫描结果：
- GitHub Token / PAT：未发现
- AWS Key：未发现
- Private Key：未发现
- 私人邮箱：未发现
- Windows 用户主目录：未发现
- Unix 用户主目录：未发现
- /mnt/data、/tmp、/opt/pyvenv 沙箱路径：未发现

发布脚本只使用 GitHub `users.noreply.github.com` 地址作为提交身份，不暴露私人邮箱。

有意保留的公开身份：
- 莱茵多特
- https://space.bilibili.com/1329692237
