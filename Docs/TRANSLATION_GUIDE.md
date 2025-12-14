# LanguageOne Complete Guide

## 📖 Features

### Language Switch
Quickly switch between two languages, supports 11 languages.

### Comment Translation
Translate blueprint node comments, supports selected nodes or entire graph.

---

## 🚀 Basic Usage

### Switch Editor Language

**Shortcut:** `Alt + Q`

**Steps:**
1. Open `Edit > Editor Preferences > Plugins > LanguageOne`
2. Select "Target Language" (e.g., Chinese Simplified)
3. Press `Alt + Q` to switch
4. Press again to switch back

**Supported Languages:**
- Chinese (Simplified/Traditional)
- English, Japanese, Korean
- German, French, Spanish, Russian
- Portuguese, Italian

---

### Translate Blueprint Comments

**Shortcut:** `Ctrl + T`

#### Method 1: Translate Selected Nodes
1. Select nodes in blueprint editor
2. Press `Ctrl + T`
3. Selected nodes' comments are translated
4. Press `Ctrl + T` again to restore

#### Method 2: Translate Entire Graph
1. Open blueprint editor
2. **Don't select any nodes**
3. Press `Ctrl + T`
4. All nodes with comments are translated

#### Restore Function
- Press `Ctrl + T` again to restore original
- Toggle between translation/original repeatedly

---

## ⚙️ Translation Settings

**Open Settings:** `Edit > Editor Preferences > Plugins > LanguageOne`

### Translation Service

**Free Services (Recommended, No Setup):**
- **Microsoft Edge Translator** - Recommended by default, high quality, fast, works in China
- **Google Translate** - Classic service, stable quality (may be limited in China)
- **MyMemory Translator** - Backup option, no API key required

**API Services (Setup Required):**
- **Baidu Translation** - Free 2M chars/month
- **Google API** - Paid, best quality
- **Custom API** - Use your own service

### Translation Options

| Option | Description | Recommended |
|--------|-------------|:-----------:|
| Translation Service | Choose translation service | Microsoft Edge |
| Translate To | Target language | As needed |
| Keep Original | Show both original and translated | ✅ |
| Translation Above | Translation position | Personal preference |

### Translation Result Examples

**Keep Original + Translation Below:**
```
Original English comment
---
翻译后的中文注释
```

**Keep Original + Translation Above:**
```
翻译后的中文注释
---
Original English comment
```

**Translation Only:**
```
翻译后的中文注释
```

---

## 🔧 API Configuration (Optional)

### Baidu Translation (Recommended)

**Advantages:** Large free tier (2M chars/month), good quality, fast

**Steps:**
1. Visit [Baidu Translation Platform](https://fanyi-api.baidu.com/)
2. Register and login
3. Create application (General Text Translation)
4. Copy **APP ID** and **Secret Key**
5. Paste in plugin settings

### Google Translation API

**Advantages:** Best quality

**Steps:**
1. Visit [Google Cloud Console](https://console.cloud.google.com/)
2. Create project and enable Translation API
3. Create API key
4. Paste in plugin settings

### Custom API

**Requirements:**
- POST request
- Request: `{"text": "...", "target_lang": "..."}`
- Response: `{"translated_text": "..."}`
- Optional: `Authorization: Bearer YOUR_KEY`

---

## 💡 Tips & Tricks

### 1. Quick Translate Entire Blueprint
Open blueprint → Press `Ctrl + T` directly → Done

### 2. Translate Specific Nodes
Ctrl + Click to select → `Ctrl + T`

### 3. Compare with Original
Check "Keep Original" in settings → View side by side

### 4. Batch Restore
Select translated nodes → `Ctrl + T` → Restore

### 5. Customize Shortcuts
`Edit > Editor Preferences > Keyboard Shortcuts > Search LanguageOne`

---

## ❓ FAQ

**Q: Shortcuts not working?**
- Restart editor
- Check output log to confirm plugin loaded
- Check for shortcut conflicts

**Q: Not detecting selected nodes?**
- Normal! Without selection, it translates the entire graph
- To translate specific nodes, select them first

**Q: Translation failed?**
- Check network connection
- Switch to another translation service
- Check output log for error details

**Q: Are free services limited?**
- Yes, but enough for normal use
- If limited, switch to another free service
- Or configure Baidu API (free 2M chars/month)

**Q: Translation quality poor?**
- Try different translation services
- Configure Google API (best quality)
- Keep original for reference

---

## 📝 Version History

### v1.2 (Current)
- Blueprint comment translation
- 3 free translation services
- 11 editor language support
- Smart restore function
- Translate entire graph support

### v1.1
- Improved language switch stability
- UE5.7 compatibility fix

### v1.0
- Initial release
- Chinese-English language switch

---

<div align="center">

**Issues:** [GitHub Issues](https://github.com/AniBullet/LanguageOne/issues)

**Open Source · Contributions Welcome · Star ⭐**

</div>
