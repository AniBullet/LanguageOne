<a id="readme-top"></a>

<div align="center">

# LanguageOne

**虚幻引擎编辑器语言切换与资产翻译工具**<br>
Unreal Engine Editor Language & Asset Translation Tool

[![Release](https://img.shields.io/github/v/release/AniBullet/LanguageOne?label=release&logo=github)](https://github.com/AniBullet/LanguageOne/releases/latest)
[![Fab](https://img.shields.io/badge/Fab-download-0078F2?logo=epicgames)](https://fab.com/s/dc840febb323)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.1%2B-313131?logo=unrealengine)](https://www.unrealengine.com/)
[![License](https://img.shields.io/github/license/AniBullet/LanguageOne)](#许可与声明)

[**Fab 下载**](https://fab.com/s/dc840febb323) | [**GitHub 下载**](https://github.com/AniBullet/LanguageOne/releases/latest) | [**使用指南**](docs/翻译功能使用说明.md) | [**更新日志**](https://github.com/AniBullet/LanguageOne/releases) | [English](#english)

</div>

一键切换 UE 编辑器语言，免费翻译蓝图注释和资产文本。

## 功能

| 功能 | 说明 | 快捷键 |
|:---|:---|:---:|
| 语言切换 | 11 种语言双向切换 | <kbd>Alt</kbd> + <kbd>Q</kbd> |
| 注释翻译 | 免费翻译蓝图注释 | <kbd>Alt</kbd> + <kbd>E</kbd> |
| 资产翻译 | 翻译资产中的文本内容 | 右键菜单 |

- **支持语言**：中文（简/繁）、英语、日语、韩语、德语、法语、西班牙语、俄语、葡萄牙语、意大利语
- **翻译服务**：微软 Edge（推荐）· 谷歌翻译 · MyMemory · 百度 API · Google API

## 安装

从 [Fab](https://fab.com/s/dc840febb323) 或 [GitHub Releases](https://github.com/AniBullet/LanguageOne/releases/latest) 下载，启用插件后重启编辑器。

## 使用

- <kbd>Alt</kbd> + <kbd>Q</kbd>：在 **语言 A** 和 **语言 B** 之间来回切换（在设置中配置）
- <kbd>Alt</kbd> + <kbd>E</kbd>：翻译 / 还原蓝图节点注释（有选中则只处理选中节点，未选中则处理整张图）
- **右键资产**：批量翻译资产（String Table、Data Table、Widget、Blueprint 等）
  - **翻译**：智能批量翻译，自动补全未翻译部分，跳过已翻译部分
  - **还原**：清除翻译内容，恢复到原文状态
  - **清除原文**：只保留译文（慎用）

**设置入口**：`编辑 > 编辑器偏好设置 > 插件 > LanguageOne`

| 选项 | 说明 |
|:---|:---|
| 语言 A (Source) | 默认语言，如英文 |
| 语言 B (Target) | 目标语言，如中文 |
| 译文位置 | 译文显示在原文上方或下方 |

完整说明见 [翻译功能使用说明](docs/翻译功能使用说明.md)。

## 截图

<table>
<tr>
<td align="center" width="50%"><img src="Preview/1启用.png" width="400" alt="启用插件"><br><sub>启用插件</sub></td>
<td align="center" width="50%"><img src="Preview/2按钮.png" width="400" alt="翻译按钮"><br><sub>翻译按钮</sub></td>
</tr>
</table>

<details>
<summary>更多截图</summary>
<br>
<table>
<tr>
<td align="center" width="33%"><img src="Preview/3蓝图翻译.png" width="260" alt="蓝图翻译"><br><sub>蓝图翻译</sub></td>
<td align="center" width="33%"><img src="Preview/4批量翻译.png" width="260" alt="批量翻译"><br><sub>批量翻译</sub></td>
<td align="center" width="33%"><img src="Preview/5设置界面.png" width="260" alt="设置界面"><br><sub>设置界面</sub></td>
</tr>
</table>
</details>

## 更新日志

**v1.5**

- **快捷键调整**：注释翻译快捷键从 <kbd>Ctrl</kbd> + <kbd>T</kbd> 改为 <kbd>Alt</kbd> + <kbd>E</kbd>，避免冲突
- **问题修复**：修复蓝图中仅选中部分节点时仍翻译整张图表的问题

完整记录见 [GitHub Releases](https://github.com/AniBullet/LanguageOne/releases)。

## 参与贡献

- 问题与建议：提交 [GitHub Issues](https://github.com/AniBullet/LanguageOne/issues)
- 欢迎 Fork 本仓库并发起 Pull Request

## English

Switch the Unreal Editor language in one keystroke, and translate blueprint comments and asset text for free.

| Feature | Description | Shortcut |
|:---|:---|:---:|
| Language Switch | Toggle between 11 languages | <kbd>Alt</kbd> + <kbd>Q</kbd> |
| Comment Translation | Free blueprint comment translation | <kbd>Alt</kbd> + <kbd>E</kbd> |
| Asset Translation | Translate text content in assets | Context menu |

- **Languages**: Chinese (Simplified / Traditional), English, Japanese, Korean, German, French, Spanish, Russian, Portuguese, Italian
- **Translation services**: Microsoft Edge (recommended) · Google Translate · MyMemory · Baidu API · Google API

**Install**: download from [Fab](https://fab.com/s/dc840febb323) or [GitHub Releases](https://github.com/AniBullet/LanguageOne/releases/latest), enable the plugin and restart the editor.

**Usage**

- <kbd>Alt</kbd> + <kbd>Q</kbd>: toggle between **Language A** and **Language B** (configured in settings)
- <kbd>Alt</kbd> + <kbd>E</kbd>: translate / restore blueprint node comments (selected nodes only, or the whole graph if nothing is selected)
- **Right-click assets**: batch translate String Table, Data Table, Widget, Blueprint, etc.
  - **Translate**: smart batch translation, fills in untranslated parts and skips translated ones
  - **Restore**: removes translations and restores the original text
  - **Clear Original**: keeps the translation only (use with caution)

**Settings**: `Edit > Editor Preferences > Plugins > LanguageOne`

- **Language A (Source)**: default language, e.g. English
- **Language B (Target)**: target language, e.g. Chinese
- **Position**: show the translation above or below the original

**Latest v1.5**

- **Shortcut update**: comment translation shortcut changed from <kbd>Ctrl</kbd> + <kbd>T</kbd> to <kbd>Alt</kbd> + <kbd>E</kbd> to avoid conflicts
- **Bug fix**: translating selected blueprint nodes no longer translates the entire graph

See the [Full Guide](docs/TRANSLATION_GUIDE.md) and [GitHub Releases](https://github.com/AniBullet/LanguageOne/releases) for details.

## 许可与声明

- 代码以 [MIT License](LICENSE) 授权
- 插件使用 AI 辅助编写，主要用于个人学习和交流
- This plugin is AI-assisted and created for personal learning purposes.

---

<sub>© Bullet.S · [X / Twitter](https://x.com/aniBulletCom) · [回到顶部](#readme-top)</sub>
