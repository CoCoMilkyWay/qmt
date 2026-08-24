Motive: 公众号文章链接 → Markdown; 再往前一步, 临时接管系统代理抓一次微信凭证, 把整个号的历史文章连正文带图片同步到本地, 之后每次运行只做增量.
抓取本身由本机直连 `mp.weixin.qq.com` 与腾讯图片 CDN; 系统依赖只有 OpenSSL, 抓凭证时另需 `gsettings` 与 `certutil`.

# 项目结构

```
gzh/
├── run.py                                  # 统一入口: cmake configure (静默) + build + 运行 build/wxmd
│                                           #   未识别的参数原样透传给 wxmd; --build 只编译, --clean 清空 build/
├── CMakeLists.txt                          # 顶层构建: vendored 依赖 + libwxmd.a + wxmd CLI
│                                           #   C++20; 系统依赖仅 OpenSSL (cpp-httplib 的 HTTPS 后端 + 代理的 TLS 与签证书) + pthread
├── include/wxmd/                           # 公共头; 一层一个 header, 与 src/ 同名文件一一对应
│   ├── wxmd.hpp                            # 顶层门面: Article {title/author/account/publish_time/link/markdown}
│   │                                       #   + parse_article (本地 html, 可带图片钩子) / fetch_and_parse (链接) / render_article_html
│   ├── fetch.hpp                           # fetch_raw(url, cookie) → 响应体; fetch_article(url) → 原始 html
│   │                                       #   fetch_asset(url) → Asset {body/content_type}: 图片在腾讯 CDN 上, 另开一个白名单
│   ├── html.hpp                            # ArticleStatus {Success/Deleted/Exception/Error} + status_text / validate_html / extract_cgi_script
│   ├── cgi.hpp                             # eval_cgi(script) → nlohmann::json (即 window.cgiDataNew)
│   ├── renderer.hpp                        # render_html(cgi) → 规范化 HTML; extract_title / extract_link
│   ├── markdown.hpp                        # to_markdown(html) → Markdown
│   ├── assets.hpp                          # AssetHook: img src → 本地相对路径; localize_images / asset_extension
│   ├── dom.hpp                             # lexbor RAII 封装 (Document/query/text_content/attr) + MdNode 可变树
│   ├── profile.hpp                         # Credential {biz/uin/key/pass_ticket/cookie} + ProfileEntry/Page/List
│   │                                       #   ProfileEntry.id = mid_idx (跨轮次稳定的文章身份, 增量靠它对账)
│   │                                       #   Watermark {since/known}: 翻页早停的水位线
│   │                                       #   fetch_profile_page/list / probe_credential / fetch_account_name / load_credential
│   ├── store.hpp                           # AccountStore: 一个号的本地缓存目录 (index/pending + 逐篇原子提交)
│   │                                       #   StoredArticle / AccountInfo / list_accounts
│   ├── proxy.hpp                           # MitmProxy: 只对 mitm_hosts 做中间人, 其余盲转发; Exchange {url/cookie/set_cookies}
│   ├── tlsca.hpp                           # CertAuthority: 自签 CA 落盘复用, 按域名现签叶子证书 (CertPair)
│   ├── capture.hpp                         # 把 Exchange 认成 CapturedAccount; CredentialStore 按 __biz 去重
│   │                                       #   save/load_credentials: 凭证落盘 0600, 读回时丢掉已过期的
│   ├── desktop.hpp                         # 桌面集成: 系统代理读写/备份还原 + CA 装进 NSS 库 (全项目唯一知道 GNOME+NSS 的地方)
│   └── assert.hpp                          # WXMD_ASSERT: Release 下同样生效 — 本项目以「尽早失败」替代错误处理
│                                           #   另有 warn(): 非致命但不符预期的情况不 silent, 喊一声
├── src/                                    # 实现; 五级流水线 fetch → html → cgi → renderer → markdown (见下节)
│   ├── fetch.cpp                           # cpp-httplib + OpenSSL; 伪装微信内置浏览器 UA, Accept-Encoding: identity
│   │                                       #   (不请求压缩 ⇒ 不必链 zlib); 正文页只认 mp.weixin.qq.com,
│   │                                       #   图片按域名后缀限定在 .qpic.cn/.qlogo.cn/.qq.com (微信换域名比枚举勤)
│   ├── html.cpp                            # 移植 shared/utils/html.ts: 按 #js_article / .weui-msg / .mesg-block
│   │                                       #   判定文章状态; cgi 脚本 = 带 h5only 且含 "window.cgiDataNew = {" 的 <script>
│   ├── cgi.cpp                             # 移植 shared/utils/cgi-sandbox.ts: 脚本内的 JsDecode 与转义正文必须真 JS 引擎才能还原.
│   │                                       #   沙箱只提供 window / console (无网络无文件), 2s 中断 + 256MB 内存上限;
│   │                                       #   在 isolate 内 JSON.stringify 回传 (顺带丢掉函数, 天然容纳超大 content_noencode)
│   ├── renderer.cpp                        # 移植 shared/utils/renderer.ts: item_show_type 0=普通图文 / 8=图片分享 / 10=文本分享
│   │                                       #   + 未购买付费文章的提示块; 图文正文做 data-src→src、去 height;
│   │                                       #   图片分享给「图N」锚点补 href; 不含评论与底部互动栏 (本项目只导出正文)
│   ├── markdown.cpp                        # turndown 7.2.1 规则集手写移植, 占全部实现的一半篇幅. 四块:
│   │                                       #   collapse_whitespace (DOM 预处理) / escape_markdown (13 条转义)
│   │                                       #   / flanking_whitespace (内联元素首尾空白外移) / apply_rule (各标签规则)
│   │                                       #   配置对齐上游 markdown.ts: atx 标题 / "-" 列表符 / fenced 代码块,
│   │                                       #   并移除 style·script·noscript·link·meta·title 与 __bottom-bar__
│   ├── assets.cpp                          # 在「渲染完 HTML、还没转 Markdown」之间改写 img src ⇒ 不必动 markdown 规则集;
│   │                                       #   同一地址只回调一次; data: 内联图原样留着;
│   │                                       #   扩展名先看 Content-Type, 再退回 URL 的 wx_fmt, 都认不出就断言
│   ├── dom.cpp                             # lexbor 侧全部实现; build_tree 把 lexbor DOM 抽成 MdNode 轻量树 —
│   │                                       #   turndown 要频繁改写文本与删节点, 在自有树上做远比改 lexbor DOM 简单
│   ├── profile.cpp                         # mp/profile_ext?action=getmsg 翻页; general_msg_list 是嵌套 JSON 字符串,
│   │                                       #   一条群发可含多篇 (multi_app_msg_item_list); 按 mid_idx 去重, 整页重复即判到底
│   │                                       #   article_id: 从 content_url 取 mid+idx — 整条链接不能当身份用,
│   │                                       #   chksm/#rd 这些尾巴跨轮次不保证一致, 增量就对不上账了
│   ├── store.cpp                           # 本地缓存的全部落盘逻辑 (布局见下节); 每篇 = 临时目录写满 → rename → 追加索引行
│   │                                       #   索引严格按发布时间升序; commit 断言 datetime >= 水位线, 谁乱序入库当场炸
│   ├── proxy.cpp                           # 裸 socket + OpenSSL 手写 CONNECT 代理: 目标域名解 TLS 抠 url/Cookie/Set-Cookie,
│   │                                       #   其余域名两个 fd 对拷. 只宣告 ALPN http/1.1 (不解 HTTP/2 分帧),
│   │                                       #   把请求改成 Connection: close ⇒ 响应体读到对端关闭为止, 省掉全部分帧逻辑;
│   │                                       #   只监听 127.0.0.1; 原系统代理非空时作为父代理续上, 其它流量走向不变
│   ├── tlsca.cpp                           # OpenSSL 手搓 CA 与叶子证书 (SAN/EKU/AKID 齐全, 叶子 397 天), 叶子共用一把私钥
│   ├── capture.cpp                         # 从 url 的 query 里取 __biz/uin/key/pass_ticket (百分号解码), 四者齐全才算凭证;
│   │                                       #   Cookie 优先取请求头的, 响应 Set-Cookie 只是兜底
│   ├── desktop.cpp                         # gsettings 读写 org.gnome.system.proxy (socks 清空, 否则会被优先选中);
│   │                                       #   接管前把原值存 ~/.wxmd/proxy-backup.json; certutil 把 CA 写进 ~/.pki/nssdb
│   ├── wxmd.cpp                            # 串流水线; load_cgi 里做状态断言, 不可用文章在此带原因终止
│   ├── strutil.hpp                         # 内部字符串工具 (replace_all / trim / lowered / percent_decode / query_param …)
│   └── fsutil.hpp                          # 内部落盘工具: write_atomic (临时文件→fsync→rename→fsync 父目录) /
│                                           #   append_line (追加+fsync, 索引的提交点) / fsync_path / mkdirs
├── cli/
│   └── main.cpp                            # CLI: 无参数进增量同步 (补齐→发现→可选加号); <链接> | -f <本地 html> 走单篇
│                                           #   --store <目录> 缓存根 (默认 ./store) | --add 直接进抓包环节
│                                           #   -o <输出> | --meta 附元信息 | --html 只输出中间态 HTML | --uninstall 卸载
│                                           #   接管系统代理后所有退出路径 (atexit / 信号 / 断言 abort) 都汇到 restore_now
├── packages/                               # 依赖全部以源码 vendored, 随本项目一起 cmake 编译, 不装系统包
│   ├── lexbor/                             # HTML5 解析 + CSS 选择器 (3.1.0); 目标 lexbor_static
│   │                                       #   已裁剪: 只留 source/ + cmake 配置, 去掉 examples/test/utils/benchmarks/wasm
│   ├── quickjs/                            # quickjs-ng JS 引擎; 目标 qjs
│   │                                       #   已裁剪到库所需的最小文件集 (quickjs/cutils/dtoa/libregexp/libunicode + 头),
│   │                                       #   CMakeLists.txt 截到库目标结束 (不含 qjs/qjsc CLI 与 test262)
│   ├── httplib/                            # cpp-httplib 单头文件 HTTP(S) 客户端
│   │                                       #   选它而非 cpr: cpr 必须链 libcurl, 本机无 libcurl 开发头且无免密 sudo
│   └── json/                               # nlohmann/json 单头文件 (nlohmann/json.hpp), 承载 cgiDataNew
├── store/                                  # 本地缓存 (gitignore); 一个子目录一个公众号, 布局见「本地缓存」一节
└── tmp/                                    # 不进版本库: 移植参考与离线回归素材
    ├── example_app/                        # 上游 TS 实现 (wechat-article-exporter), 移植的参考源
    └── tests/
        ├── samples/                        # → 上游 samples 的软链 (真实抓取样本)
        │   ├── 普通图文/                    # item_show_type=0, 6 篇
        │   ├── 图片分享/                    # item_show_type=8, 6 篇
        │   ├── 文本分享/                    # item_show_type=10, 9 篇
        │   ├── 文章分享/                    # 4 篇
        │   ├── author/                     # 带 author 字段, 1 篇
        │   ├── 作者已删除/                 # 6 篇, 预期 status=Deleted
        │   ├── 内容违规/                   # 2 篇, 预期 status=Exception
        │   ├── 该内容暂时无法查看/         # 1 篇, 预期 status=Exception
        │   └── aboutbiz/                   # 7 份账号主页 (非文章), 预期 status=Error
        ├── turndown_ref.js                 # 参考实现: 真 turndown 7.2.1 + jsdom, 配置与上游 markdown.ts 一致
        └── compare_turndown.py             # 对照器: 同一份中间态 HTML 分别喂参考实现与 wxmd, 逐字节 diff (见下节)
```

## 五级流水线

`src/wxmd.cpp` 里的 `parse_article` 串起全部五级, 每级一个源文件、一个头文件, 互不反向依赖.
`assets.cpp` 是唯一的可选插入点 (只有同步模式会传图片钩子), 所以不占级号:

| 级     | 文件           | 输入 → 输出                | 关键点                                                                                                                                                                                   |
| ------ | -------------- | -------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1 抓取 | `fetch.cpp`    | url → 原始 html            | 本机 IP 直连, 无代理. 文章正文页是公开 URL, 不需要登录态; 上游那套公共代理只为解决浏览器 CORS 与集中抓取的风控, 本地单篇抓取都用不上. 底层 `fetch_raw` 也供 `profile.cpp` 带 cookie 复用 |
| ─      | `assets.cpp`   | 规范化 HTML → 同一份 HTML  | 只在同步模式插一脚: 把 `img` 的 src 交给钩子换成 `assets/NNN.ext`. 放在渲染与转换之间, `markdown.cpp` 一行都不用改                                                                       |
| 2 定位 | `html.cpp`     | html → 状态 + cgi 脚本     | 先判文章是否可用 (已删除/违规/风控), 再取出带 `h5only` 的那段 `<script>`                                                                                                                 |
| 3 求值 | `cgi.cpp`      | 脚本 → `cgiDataNew`        | 必须真 JS 引擎: 正文经 `JsDecode` 与 `\xNN` 双层转义, 正则还原不可靠                                                                                                                     |
| 4 渲染 | `renderer.cpp` | `cgiDataNew` → 规范化 HTML | 三种 `item_show_type` 各一套正文组装; 标题/元信息/原文链接统一结构                                                                                                                       |
| 5 转换 | `markdown.cpp` | HTML → Markdown            | turndown 规则集移植; 图片直接由 src 生成 `![](…)` — 单篇模式下就是 cdn 地址, 同步模式下已被上一步换成本地相对路径                                                                        |

## 一条约束决定了整个流程的形状

单篇文章页是公开 URL, 直连即可; 但**历史消息列表** (`mp/profile_ext`) 必须带微信客户端侧的
`key` / `pass_ticket`. 这两个值只在微信内打开文章时才下发, **按 `__biz` 绑定**、分钟级过期
(按上游取 25 分钟), 程序无法自行续签 —— 所以「已知公众号 id 就全自动发现新文章」做不到零交互,
每个号每轮都要在微信里点开一篇.

但**正文页不需要凭证**. 这道缝把整件事切成约束完全不同的两段, 也就是同步流程的骨架:

| 段         | 需要凭证? | 代价                       | 能不能中断续跑                             |
| ---------- | --------- | -------------------------- | ------------------------------------------ |
| 发现新文章 | 要        | 增量下通常一页就撞上水位线 | 不能: 中断会留空洞, 只有整趟翻完才写队列   |
| 落盘正文   | 不要      | 一篇一请求, 外加 N 张图    | 能: 逐篇原子提交, 任意时刻掐掉都不留半成品 |

所以无参数运行 `wxmd` 是这个顺序 —— **先补齐, 再发现, 最后才问要不要加新号**:

| 步  | 做什么                                                                       | 在哪                          |
| --- | ---------------------------------------------------------------------------- | ----------------------------- |
| 1   | 扫 `store/` 下已有的号; 读 `~/.wxmd/credentials.json`, 已过期的直接丢掉      | `store.cpp` + `capture.cpp`   |
| 2   | 逐个号把 `pending.jsonl` 里剩下的文章下完. 不用凭证, 这一步永远能做          | `cli/main.cpp`                |
| 3   | 凭证还在的号: `probe_credential` 探一条确认可用, 再增量翻页发现新文章        | `profile.cpp`                 |
| 4   | 新发现的排进队列, 立刻接着走第 2 步                                          | `store.cpp` + `cli/main.cpp`  |
| 5   | 都走完了才问: 要不要起代理加新号 / 刷新失效的凭证 (`--add` 跳过这句直接进)   | `cli/main.cpp`                |

补齐排在发现之前不是随意排的: 补齐会把水位线往前推, 紧接着的发现就能更早停下来.

### 抓凭证那一趟

| 步  | 做什么                                                                                  | 在哪                           |
| --- | --------------------------------------------------------------------------------------- | ------------------------------ |
| 1   | 检查 `gsettings` / `certutil`, 缺 certutil 就报出本机发行版对应的安装命令并等确认       | `cli/main.cpp` + `desktop.cpp` |
| 2   | 起 MITM 代理: 端口传 0 交内核分配, 只监听回环; 原有系统代理接为父代理, 其它流量走向不变 | `proxy.cpp`                    |
| 3   | 自签 CA 装进 `~/.pki/nssdb` (只写当前用户, 不动系统证书), 微信 webview 从那里取信任     | `tlsca.cpp` + `desktop.cpp`    |
| 4   | 接管系统代理, 等你在微信里逐个点开想同步的号; 只对 `mp.weixin.qq.com` 解 TLS, 其余盲转发 | `desktop.cpp` + `proxy.cpp`    |
| 5   | 从 URL query 里认出 `__biz/uin/key/pass_ticket`, 按 `__biz` 去重, 再回抓文章页解析号名  | `capture.cpp` + `profile.cpp`  |
| 6   | 回车一次性收下全部, **立刻还原系统代理并停代理**, 之后翻页与抓正文都是我们自己直连      | `cli/main.cpp`                 |
| 7   | 凭证落盘 `~/.wxmd/credentials.json` (0600), 然后对这批号走一遍上面的补齐 + 发现         | `capture.cpp` + `cli/main.cpp` |

一趟收多个号是刻意的: 凭证按号绑定, 但 25 分钟的窗口是共享的 —— 在微信里连点几下,
之后所有号的增量都在同一个窗口里做完, 不必反复接管系统代理.
凭证落盘则让 25 分钟内的重跑完全不用碰微信 (超时之后, 「补齐」这一段照样零交互).

环境改动只有两处, 且都可回退:

- 系统代理: 接管前把原值存 `~/.wxmd/proxy-backup.json`. 正常收尾、`atexit`、
  `SIGINT/TERM/HUP/QUIT/ABRT/SEGV` 都汇到同一个还原入口; 被 `kill -9` 也没关系 ——
  下次启动无条件读这份备份自愈. (本项目断言遍地, 只挂 `SIGINT` 兜不住 `abort()`.)
- CA 信任: 只在用户 NSS 库里加一条 `wxmd local CA`. `--uninstall` 一次性还原代理、
  摘掉这条信任并删掉 `~/.wxmd` (连带那份凭证).

每页条数、请求间隔这些参数写死在 `cli/main.cpp` 顶部的常量里, 没做成命令行开关:
日常用不到, 改了要重编译, 正好逼着想清楚再改.

## 本地缓存: 增量与原子性

缓存根目录默认 `./store` (`--store` 可改), 一个子目录一个公众号:

```
store/<__biz>/
├── account.json                                  # {biz, nickname} — 目录名经过清洗, 反推不回 __biz, 原值记在这里
├── index.jsonl                                   # 已入库文章, 一行一篇, 严格按发布时间升序追加
├── pending.jsonl                                 # 已发现未入库的队列, 同样按时间升序
├── 2016-09-07_一个高手的交易之路_2247483651_1/
│   ├── article.md                                # 带元信息头; 图片链接指向 assets/
│   └── assets/001.jpg …                          # 序号即正文里的出现顺序, 扩展名由 Content-Type 定
└── .staging/                                     # 正在下的那一篇; 开局无条件清掉
```

**不变式: `index.jsonl` 覆盖 [最早一篇, 水位线] 之间的全部文章, 中间没有空洞.**
水位线 = 索引最后一行的发布时间. 围着这条不变式有四个设计:

- **文章身份是 `mid_idx`**, 不是整条链接. `content_url` 的 `chksm` / `#rd` 这类尾巴跨轮次不保证
  一致, 拿整条链接当键, 增量第一轮就对不上账. `mid` 是群发消息 id, `idx` 是该次群发里的第几篇.
- **翻页早停**: 历史消息严格按群发时间倒序, 所以本页一出现早于水位线的文章就停
  (`Watermark::since`). 但一次群发的多篇共享同一个发布时间, 单看时间会把同批还没缓存的几篇
  一起挡掉, 所以再用 `Watermark::known` 逐条问一句「这篇是不是已经入库了」.
- **只有整趟翻完才写队列**: 翻页是从新往旧走的, 中途被打断的话「已发现」和水位线之间会留一段
  空洞, 那时入库会让水位线跳过没抓到的文章 —— 空洞一旦被跳过, 以后的早停永远不会再回头补.
- **逐篇原子提交, 从早到晚**: 正文和图片先全写进 `.staging/` → fsync → 整目录 rename 到位 →
  才往 `index.jsonl` 追加一行. 所以「索引里有这一行」等价于「这篇连图片都完整落盘」,
  被打断最多丢掉正在下的那一篇. `commit` 里还断言 `datetime >= 水位线`, 谁乱序入库当场炸.

已删除 / 违规的文章只在索引里留一行墓碑 (`status` 记 `Deleted` / `Exception`, 不建目录).
不这么做的话, 一篇永远打不开的文章会把整条流水线永久卡在同一个位置 —— 历史久的号踩到删除件是常态.

## 用法

```bash
./run.py                                             # 增量同步 store/, 结束后问要不要加新号
./run.py --add                                       # 跳过询问, 直接进抓包环节加号 / 刷新凭证
./run.py --store ~/gzh-data                          # 换一个缓存根目录
./run.py "https://mp.weixin.qq.com/s?__biz=..."      # 抓取单篇并输出 Markdown (不入缓存)
./run.py -f tmp/tests/samples/普通图文/c01.html      # 离线解析本地 HTML
./run.py "<url>" --meta -o out.md                    # 附元信息并写文件
./run.py --html -f <file>                            # 只看中间态 HTML
./run.py --uninstall                                 # 还原系统代理、移除 CA 与 ~/.wxmd (不动 store/)
./run.py --build                                     # 只编译
./run.py --clean                                     # 清空 build/ 后重新编译
```

`-o` / `--meta` / `--html` 只对单篇有效, 同步模式下会喊一声然后忽略: 同步的产物就是缓存目录本身.

抓不到凭证就重启一次微信: 它只在启动时读代理和证书.
