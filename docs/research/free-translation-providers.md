# 免费翻译线路调研

日期：2026-10-09。测的是这台机器：直连出口是上海联通（`curl --noproxy '*'`），海外走本机代理 `127.0.0.1:12809`（出口在美国）。插件用户以国内为主，也有海外用户。目标是免注册、免 API Key 的线路，越容易用、两边网络越能通的越靠前。需要自己申请 Key 的服务单独列出，不进默认链。

后台调研任务中途被额度限制打断，下面是已经落地的探测，加上公开协议来源。没有在本机跑完的项会标明。

## 结论

默认免费链保持 [#7](https://github.com/AniBullet/LanguageOne/issues/7) 已定的顺序：

1. **微软 Bing 网页**（`cn.bing.com/ttranslatev3`）— 国内直连和海外都返回译文，已经替换掉失效的 Edge 授权。
2. **腾讯交互翻译 TranSmart** — 国内直连约 0.3 秒，海外多个节点也能连上，只要一个随机 `client_key`。
3. **Google 网页**（`translate.googleapis.com`，`client=gtx`）— 海外可用；这次国内直连 12 秒超时。
4. **MyMemory** — 两边都能通，匿名每天约 5000 字符，单次 `q` 最多 500 字节，只适合兜底。

可以排在 Google 前面、TranSmart 后面的免 Key 候选（海外连续 25 次都返回了含中文的结果，国内功能这次没有逐条复测）：火山翻译网页、阿里翻译网页、Yandex。B 站 Index-Translate 是 2026-10-04 公布的官方免 Key API，协议最干净，但本机还没打过这个接口，接进默认链之前要先测国内和海外。

付费或要申请 Key 的（百度、Google Cloud、Azure、DeepL API）继续只在用户自己填好凭据时使用，失败不自动降级。

## 已测：能出译文

| 线路 | 国内直连 | 海外/代理 | 要 Key | 备注 |
| --- | --- | --- | --- | --- |
| Bing `cn.bing.com/ttranslatev3` | 200，译出「打开内容浏览器并保存所有资源」 | 200 | 否 | 先 GET `/translator` 取 `params_AbusePreventionHelper` 的 key/token。国内 `www.bing.com` 会 302 到 `cn.bing.com`，插件先访问 `www.bing.com`，从页面的 `<link rel="canonical">` 读出实际落地域名，并带上页面返回的 Cookie；失败时换 `cn.bing.com` 重试一次。不带 Cookie 也能译。token 坏了仍是 HTTP 200，正文是 `{"statusCode":205}`。长文本会截断（约 1670 字符输入只译回约 471 字），要分段。 |
| TranSmart `transmart.qq.com/api/imt` | 200，约 0.3 秒 | check-host 多国 200；代理 25/25 | 否 | JSON：`header.fn=auto_translation`，`source.text_list`，`target.lang`。非官方接口。 |
| Google `translate.googleapis.com/translate_a/single?client=gtx` | 12 秒超时 | 200 | 否 | 国内不可用。 |
| MyMemory `api.mymemory.translated.net/get` | 200 | 200 | 否 | [匿名 5000 字符/天](https://mymemory.translated.net/doc/usagelimits.php)，[单次最多 500 字节](https://mymemory.translated.net/doc/spec.php)。 |

微软旧接口 `https://edge.microsoft.com/translate/auth` 国内、海外都是 404。微软在 2026-07-30 关掉了 Edge 翻译，[其他项目同样中招](https://github.com/YiiGuxing/TranslationPlugin/issues/6451)。

## 已测：海外连续请求能出中文

2026-10-09 经代理连续 25 次短句，HTTP 都是 200，响应里能看到汉字。这只能说明当时没被立刻封，不能当成质量或国内可用性的结论。

| 线路 | 25 次耗时 | 协议要点 | 建议 |
| --- | --- | --- | --- |
| 有道演示 `aidemo.youdao.com/trans` | 1.7 秒 | POST 表单 `q/from/to` | 演示站，不建议进默认链 |
| 火山 `translate.volcengine.com/crx/translate/v1/` | 3.1 秒 | POST JSON `source_language/target_language/text` | 免 Key，可作 TranSmart 之后的候选 |
| 搜狗 `fanyi.sogou.com/api/transpc/text/result` | 4.0 秒 | 要先打开页面再算 sign | [FluentRead 2026-10-05](https://github.com/FluentRead/FluentRead/blob/main/docs/free-translation-apis.md) 记录业务码 `s10`、空译文。汉字可能来自错误信息，先不要用 |
| 阿里 `translate.alibaba.com/api/translate/text` | 6.0 秒 | 先取 `csrftoken`，再 multipart | 免 Key，实现比 Bing 多一步 |
| TranSmart | 6.9 秒 | 见上 | 已在默认链 |
| 彩云 `api.interpreter.caiyunai.com/v1/translator` | 10.9 秒 | 请求头里是公开演示 token `token 3975l6lr5pcbvidl6jl2` | 不算免 Key，token 随时可能作废 |
| Reverso `api.reverso.net/translate/v1/translation` | 12.3 秒 | JSON，`chi`/`eng` | 可用，语言码和现有设置不一致 |
| Yandex `translate.yandex.net/api/v1/tr.json/translate?srv=android` | 12.7 秒 | POST，`id` 用随机 UUID | 免 Key。探测脚本里国内也返回过译文 |
| Bing（脚本每次都重新抓页面） | 32.5 秒 | 见上 | 插件里鉴权缓存约 1 小时，实际不会这么慢 |

check-host.net 在 BR/DE/JP/NL/SG/GB/US 对上述网址做的是 GET。POST 接口因此出现 404、405、410、400，只说明机器能连上，不能说明 GET 失败。TranSmart、Bing 页面、MyMemory、阿里 csrf、搜狗页面在这些节点上是 200。报告在 check-host 的当次链接里，例如 TranSmart：`https://check-host.net/check-report/4fbe23c4k19c`。

## 官方免 Key，本机还没打通

**B 站 Index-Translate。** 2026-10-04 公布的免费 HTTP API，OpenAI 兼容，不需要 Key：`https://index-translate.bilibili.com/v1/chat/completions`，模型 `Index-Translate-35B-A3B`。来源：[bilibili/Index-Translate](https://github.com/bilibili/Index-Translate)、[call_api.py](https://github.com/bilibili/Index-Translate/blob/main/inference/llm/call_api.py)。文本模型覆盖约 150 种语言。额度、限流和能用多久官方没有写死。FluentRead 已把它放进免费池，并去掉发往该域名的 `Origin` 头。插件是编辑器进程，一般没有浏览器扩展的 Origin 问题。接进默认链之前，要用和 Bing 一样的方式测国内直连和海外，并确认译文是纯文本、不会夹带解释。

## 不放进默认链

| 候选 | 原因 |
| --- | --- |
| 百度网页 `/transapi`、`/ait/text/translate` | 要 cookie / 验证码一类页面状态。FluentRead 记过业务码 1022、没有译文 |
| 小牛网页 | 现行协议带验证码 |
| Papago | 页面结构已换，旧签名对不上 |
| DeepL 网页 `w2.deepl.com/jsonrpc` | 要伪造时间戳和 JSON 空格，容易失效；正式 API 要 Key |
| 金山词霸 | 要 AES 解密响应，只覆盖中英 |
| 有道 `dict.youdao.com/jsonapi_s` | 网页签名，只覆盖中英，密钥写在前端 |
| LibreTranslate 官方站、Lingva 公共实例 | 官方要 Key；公共实例这次不可用（403/500） |
| ModernMT 网页 | 官网已说 2026 年底停用，迁到 Lara |
| Azure F0、DeepL API Free、百度/腾讯云/阿里云/火山云正式 API | 都要账号和 Key。免费额度不能当成匿名接口 |

## 和插件的关系

- [#5](https://github.com/AniBullet/LanguageOne/issues/5)：`MicrosoftFree` 已改为 Bing 网页。枚举名不变，旧配置继续有效。UE 5.1–5.8 已编译通过。UE 5.8 里开着代理翻译成功；国内直连在本机用同一接口返回了译文，编辑器里还没关 VPN 复测。
- [#7](https://github.com/AniBullet/LanguageOne/issues/7)：免费失败时按 Bing → TranSmart → Google 网页 → MyMemory 换线，并加默认开启的开关。付费服务失败只报错并建议改设置。火山、阿里、Yandex、B 站是这条链的后续候选，不在这次范围内。
