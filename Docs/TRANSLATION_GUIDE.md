# LanguageOne Complete Guide

## 📖 Features

### Language Switch
Quickly switch between two languages, supports 11 languages.

### Comment Translation
Translate blueprint node comments, supports selected nodes or entire graph.

### Asset Translation
Translate text content in various UE assets.

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

### Translate Asset Content

**Feature:** Batch translate text content in various UE assets using the Asset Translation Tool

**Supported Asset Types:**
- **String Table** - Translate all string entries (original text preserved with zero-width markers)
- **Data Table** - Translate text fields
- **Widget Blueprint** - Translate UI widget text
- **Blueprint** - Translate variable descriptions, function comments, node comments
- **Other Assets** - Translate asset metadata

**How to Use:**
1. Select assets in Content Browser (multi-select supported)
2. Right-click on assets
3. Choose "Asset Translation | 资产翻译"
4. **Asset Translation Tool Window** opens

**Asset Translation Tool Window Features:**

The tool window displays:
- 📦 **Processable Assets**: Number of supported assets
- ⚠ **Unsupported Assets**: Automatically filtered out (e.g., unsupported asset types)
- 💡 **Real-time Tips**: Current operation hints

**Available Operations:**

| Button | Function | Use Case |
|--------|----------|----------|
| **🌐 Translate** | Smart batch translation, auto-completes untranslated parts | Main action button |
| **🔄 Restore** | Remove translation, restore to original | When restoration needed |
| **🗑️ Clear Original** | Permanently delete original text, keep only translation (dangerous) | After confirming translation quality |
| **Close** | Manually close tool window | After completing operations |

**Smart Behavior of Translate Button:**
- ✅ **All Untranslated** → Start translating all assets
- ✅ **Mixed State** → Smart handling: translates only untranslated text, skips or fixes format for translated text
- ✅ **Translated State** → Checks and fixes format

**Progress Display:**
The tool window bottom shows real-time progress:
- ✓ **Success**: Successfully processed assets
- ✗ **Failed**: Failed asset count
- ⚠ **Unsupported**: Unsupported asset count (fixed value)

After completion, shows summary like:
- "✓ Completed! 5 assets processed successfully"

**Technical Details:**
- **Bilingual Format**: Uses `Translation\n---\nOriginal` format, original marked with zero-width characters (`ZWSP+ZWNJ...ZWSP+ZWJ`)
- **Data Table** auto-detects `FText` fields and `FString` fields containing `text`, `desc`, `comment` keywords
- **Widget Blueprint** supports `UTextBlock`, `UEditableText`, `UButton`, `UCheckBox`, etc.
- **Blueprint** uses safe metadata access, avoiding crashes with special blueprint types like ControlRig
- **Real-time Status Detection**: Tool detects asset translation status in real-time, recognizes external modifications correctly

---

### Translate Blueprint Comments

**Shortcut:** `Alt + E`

#### Method 1: Translate Selected Nodes
1. Select nodes in blueprint editor
2. Press `Alt + E`
3. Selected nodes' comments are translated
4. Press `Alt + E` again to restore

#### Method 2: Translate Entire Graph
1. Open blueprint editor
2. **Don't select any nodes**
3. Press `Alt + E`
4. All nodes with comments are translated

#### Restore Function
- Press `Alt + E` again to restore original
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

**Comment Translation:**
| Option | Description | Recommended |
|--------|-------------|:-----------:|
| Translation Service | Choose translation service | Microsoft Edge |
| Translate To | Target language | As needed |
| Translation Above | Translation position (original always preserved) | Personal preference |

**Asset Translation:**
| Option | Description | Recommended |
|--------|-------------|:-----------:|
| Enable Asset Translation | Show translation option in context menu | ✅ |
| Confirm Before Translation | Show confirmation dialog before batch translation | Personal preference |
| Verbose Logging | Show detailed translation logs | ✅ |

**Important Notes:**
- Since v1.4, asset translation preserves original text by default (using zero-width character markers), allowing display toggle or restoration anytime
- To permanently delete original text, use the "Clear Original" button in the tool window
- **Manual save required (Ctrl+S)** after translation, plugin does not auto-save assets
- Operations are locked during processing to prevent state corruption

### Translation Result Examples

**Translation Below (Default):**
```
Original English comment
---
翻译后的中文注释
```

**Translation Above (Setting Enabled):**
```
翻译后的中文注释
---
Original English comment
```

**After Clearing Original (Permanent):**
```
翻译后的中文注释
```

**Technical Notes:**
- Original text marked with zero-width characters (`ZWSP+ZWNJ...ZWSP+ZWJ`), invisible but programmatically detectable
- `---` serves as visible separator for readability
- Can toggle display order anytime using "Translate/Toggle" button
- Use "Restore" button to revert to pure original text
- Use "Clear Original" button to permanently delete original markers

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
Open blueprint → Press `Alt + E` directly → Done

### 2. Translate Specific Nodes
Ctrl + Click to select nodes → `Alt + E`

### 3. Compare with Original
Translation automatically preserves original → View side by side

### 4. Batch Restore Comments
Select translated nodes → `Alt + E` → Restore

### 5. Translate String Table
Select String Table in Content Browser → Right-click → "Asset Translation"

### 6. Batch Translate Multiple Assets
Ctrl + Click to select multiple assets in Content Browser → Right-click → "Asset Translation"

### 7. Translate Sample Projects
Can translate UE official sample assets to help understand examples

### 8. Customize Shortcuts
`Edit > Editor Preferences > Keyboard Shortcuts > Search LanguageOne`

### 9. Smart Tool Window
The Asset Translation Tool intelligently handles mixed selections and provides clear feedback

### 10. View Translation Progress
Batch translation displays beautiful progress window with real-time status

### 11. Smart Mixed State Handling
When selecting both translated and untranslated assets, clicking "Translate/Toggle":
- Tool auto-classifies: untranslated → translate, translated → toggle display
- Shows notification with operation details like "Mixed state: 2 toggled, 3 translated"

### 12. Safe Use of Clear Original
"Clear Original" button permanently deletes original markers:
- ⚠️ **Dangerous Operation**: Irreversible, use with caution
- Confirm translation quality before use
- Suitable for final release cleanup to reduce file size
- Double confirmation dialog appears on click

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

**Q: Can't restore after asset translation?**
- v1.4+ preserves original text by default (using zero-width markers)
- Can restore anytime using "Restore" button in tool window
- Only irreversible after using "Clear Original" function
- Recommend version control for important assets

**Q: Which assets support translation?**
- String Table (string tables)
- Data Table (text fields)
- Widget Blueprint (UI widget text)
- Blueprint (variable descriptions, comments)
- Other asset description info

**Q: Will original text show in-game after translation?**
- No! Original text marked with zero-width characters, invisible to game
- Can control display order with "Translation Above" setting
- Can toggle display anytime with "Translate/Toggle" button
- To completely remove original, use "Clear Original" function

**Q: How are unsupported assets handled?**
- Tool automatically filters out unsupported assets
- Unsupported asset count displayed fixed in progress bar
- Only supported assets processed, progress calculated from supported count
- Unsupported types include: Texture, Material, Sound, and other non-text assets

---

## 📝 Version History

### v1.5 (Current)
- ⌨️ Comment translation shortcut changed from `Ctrl+T` to `Alt+E` to avoid conflicts

### v1.4
- ✨ Brand new Asset Translation Tool window with batch processing
- ✨ Smart "Translate/Toggle" button auto-selects operation based on asset state
- ✨ New "Restore" and "Clear Original" functions for flexible translation management
- 🎨 Integrated progress bar with real-time result display
- 🎨 Optimized interface layout with clearer information display
- 🔧 Improved translation status detection with real-time updates
- 🔧 Optimized mixed state handling (process translated and untranslated assets together)
- 📝 Use zero-width characters to mark original text, more standardized format
- 📝 Enhanced documentation with new usage tips
- ⚠️ **Dropped support for UE 4.x (4.26/4.27) and UE 5.0** due to maintenance complexity. Please use older versions if needed. Only UE 5.1+ is supported.

### v1.3
- 🔧 Fixed API issues
- 🔧 Changed default API

### v1.2
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
