# 发版流程

一次发版从改代码到 Fab 上架。前五步在本地完成；推送 main 之后，打包和 GitHub Release 由 [`.github/workflows/release.yml`](../.github/workflows/release.yml) 自动完成；最后一步 Fab 只能人工操作。

## 步骤

1. **提交修复**。修 issue 的提交说明里写 `Fixes #N`，推送到 main 后 GitHub 会自动关闭该 issue。
   完成标准：每个要关闭的 issue 都至少被一个提交的 `Fixes #N` 引用。

2. **编译全部引擎版本**。运行 `.\BuildAllEngines.ps1`，它会按注册表找到本机安装的 UE 5.1–5.8，逐个执行 `RunUAT BuildPlugin`。只想快速验证时，可以先跑最老和最新的版本：`-Versions 5.1,5.8`。
   完成标准：8 个版本都输出 `OK`，脚本以 0 退出。Fab 会在服务器上重新编译，任何一个版本编不过都会被退回。

3. **升版本号**。在 [`LanguageOne/LanguageOne.uplugin`](../LanguageOne/LanguageOne.uplugin) 里把 `Version`（整数）加 1，并把 `VersionName` 改成新版本号，比如 `"1.7"`。
   完成标准：`Version` 比上一个 tag 的大，`VersionName` 对应的 tag `vX.Y` 还不存在。CI 会检查这两条。

4. **写更新记录**。两份使用说明都要在"版本历史"最上面加一个新小节：
   - [`docs/翻译功能使用说明.md`](翻译功能使用说明.md)：`### vX.Y (当前)`
   - [`docs/TRANSLATION_GUIDE.md`](TRANSLATION_GUIDE.md)：`### vX.Y (Current)`

   同时去掉上一版标题上的 `(当前)` / `(Current)`。这两个小节就是 Release 的发布说明，中文在上、英文在下。
   完成标准：两份文件都有 `### vX.Y ` 开头的标题，且下面有内容。缺任何一份，CI 都会拒绝发布；本地运行 `Package.ps1` 也会给出警告。

5. **推送 main**。推送后 `release.yml` 会读取 `VersionName`、检查第 3、4 步、运行 `Package.ps1` 打出 8 个 zip，然后创建 Release `vX.Y` 并上传。
   完成标准：`gh release view vX.Y` 显示 8 个 `0N_LanguageOne_UE5.N.zip` 附件，且该 Release 是 Latest。

6. **更新 Fab**。在 Fab 后台把 8 个引擎版本的"版本标题"改成 `vX.Y`，填写版本更新说明，然后提交审核。文件链接不用改。
   完成标准：8 个版本都已提交，进入审核或已通过。

## 为什么这样设计

- **zip 文件名不带版本号**（`01_LanguageOne_UE5.1.zip` … `08_LanguageOne_UE5.8.zip`）。Fab 上填的是 `https://github.com/AniBullet/LanguageOne/releases/latest/download/<文件名>`，永远指向标记为 Latest 的 Release。所以改文件名会让 Fab 链接 404，Release 也不能标成 pre-release 或 draft。
- **推送 main 就会发版，但只发一次**。tag 已存在时工作流直接跳过，所以平时推送不会重复发布。要重新发布同一个版本的内容，就升一个新版本号，不要删 tag 重发。
- **Fab 可能在审核时就把 zip 拉走存一份**。所以新的 Release 发出后，还要在 Fab 里提交更新，用户拿到的才是新包。
- **CI 用 `pwsh` 运行 `Package.ps1`**。Windows PowerShell 5.1 会把不带 BOM 的 UTF-8 脚本按系统代码页读取，中文会被读成乱码，导致解析失败。脚本结尾的 `exit 0` 用来覆盖 robocopy 成功时留下的非零退出码。
- **本地编译的输出放在 `%TEMP%\l1b5N`**。UBT 生成的中间文件路径一旦超过 260 个字符就会编译失败，放在路径较深的目录下会碰到这个限制。
