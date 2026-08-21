Motive: 公众号文章链接 → Markdown; 再往前一步, 临时接管系统代理抓一次微信凭证, 把整个号的历史文章列表拉下来.
抓取本身由本机直连 `mp.weixin.qq.com`; 系统依赖只有 OpenSSL, 交互模式另需 `gsettings` 与 `certutil`.

# 项目结构

```
gzh/
├── run.py                                  # 统一入口: cmake configure (静默) + build + 运行 build/wxmd
│                                           #   未识别的参数原样透传给 wxmd; --build 只编译, --clean 清空 build/
├── CMakeLists.txt                          # 顶层构建: vendored 依赖 + libwxmd.a + wxmd CLI
│                                           #   C++20; 系统依赖仅 OpenSSL (cpp-httplib 的 HTTPS 后端) + pthread
├── include/wxmd/                           # 公共头; 一层一个 header, 与 src/ 同名文件一一对应
│   ├── wxmd.hpp                            # 顶层门面: Article {title/author/account/publish_time/link/markdown}
│   │                                       #   + parse_article (本地 html) / fetch_and_parse (链接) / render_article_html
│   ├── fetch.hpp                           # fetch_article(url) → 原始 html
│   ├── html.hpp                            # ArticleStatus {Success/Deleted/Exception/Error} + validate_html / extract_cgi_script
│   ├── cgi.hpp                             # eval_cgi(script) → nlohmann::json (即 window.cgiDataNew)
│   ├── renderer.hpp                        # render_html(cgi) → 规范化 HTML; extract_title / extract_link
│   ├── markdown.hpp                        # to_markdown(html) → Markdown
│   ├── dom.hpp                             # lexbor RAII 封装 (Document/query/text_content/attr) + MdNode 可变树
│   └── assert.hpp                          # WXMD_ASSERT: Release 下同样生效 — 本项目以「尽早失败」替代错误处理
├── src/                                    # 实现; 五级流水线 fetch → html → cgi → renderer → markdown (见下节)
│   ├── fetch.cpp                           # cpp-httplib + OpenSSL; 伪装微信内置浏览器 UA, Accept-Encoding: identity
│   │                                       #   (不请求压缩 ⇒ 不必链 zlib); 只接受 mp.weixin.qq.com 域名
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
│   ├── dom.cpp                             # lexbor 侧全部实现; build_tree 把 lexbor DOM 抽成 MdNode 轻量树 —
│   │                                       #   turndown 要频繁改写文本与删节点, 在自有树上做远比改 lexbor DOM 简单
│   ├── wxmd.cpp                            # 串流水线; load_cgi 里做状态断言, 不可用文章在此带原因终止
│   └── strutil.hpp                         # 内部字符串工具 (replace_all / trim / strip_whitespace / escape_html)
├── cli/
│   └── main.cpp                            # CLI: <链接> | -f <本地 html> | -o <输出> | --meta 附元信息 | --html 只输出中间态 HTML
├── packages/                               # 依赖全部以源码 vendored, 随本项目一起 cmake 编译, 不装系统包
│   ├── lexbor/                             # HTML5 解析 + CSS 选择器 (3.1.0); 目标 lexbor_static
│   │                                       #   已裁剪: 只留 source/ + cmake 配置, 去掉 examples/test/utils/benchmarks/wasm
│   ├── quickjs/                            # quickjs-ng JS 引擎; 目标 qjs
│   │                                       #   已裁剪到库所需的最小文件集 (quickjs/cutils/dtoa/libregexp/libunicode + 头),
│   │                                       #   CMakeLists.txt 截到库目标结束 (不含 qjs/qjsc CLI 与 test262)
│   ├── httplib/                            # cpp-httplib 单头文件 HTTP(S) 客户端
│   │                                       #   选它而非 cpr: cpr 必须链 libcurl, 本机无 libcurl 开发头且无免密 sudo
│   └── json/                               # nlohmann/json 单头文件 (nlohmann/json.hpp), 承载 cgiDataNew
├── tests/
│   ├── samples/                            # → wechat-article-exporter-master/samples 的软链 (上游真实抓取样本)
│   │   ├── 普通图文/                        # item_show_type=0, 6 篇
│   │   ├── 图片分享/                        # item_show_type=8, 6 篇
│   │   ├── 文本分享/                        # item_show_type=10, 9 篇
│   │   ├── 文章分享/                        # 4 篇
│   │   ├── author/                         # 带 author 字段, 1 篇
│   │   ├── 作者已删除/                     # 6 篇, 预期 status=Deleted
│   │   ├── 内容违规/                       # 2 篇, 预期 status=Exception
│   │   ├── 该内容暂时无法查看/             # 1 篇, 预期 status=Exception
│   │   └── aboutbiz/                       # 7 份账号主页 (非文章), 预期 status=Error
│   ├── turndown_ref.js                     # 参考实现: 真 turndown 7.2.1 + jsdom, 配置与上游 markdown.ts 一致
│   └── compare_turndown.py                 # 对照器: 同一份中间态 HTML 分别喂参考实现与 wxmd, 逐字节 diff (见下节)
└── wechat-article-exporter-master/         # 上游 TS 实现, 移植的参考源 + samples 的实际存放处 (移植完可删)
```

## 五级流水线

`src/wxmd.cpp` 里的 `parse_article` 串起全部五级, 每级一个源文件、一个头文件, 互不反向依赖:

| 级     | 文件           | 输入 → 输出                | 关键点                                                                                                                               |
| ------ | -------------- | -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| 1 抓取 | `fetch.cpp`    | url → 原始 html            | 本机 IP 直连, 无代理. 文章正文页是公开 URL, 不需要登录态; 上游那套公共代理只为解决浏览器 CORS 与集中抓取的风控, 本地单篇抓取都用不上 |
| 2 定位 | `html.cpp`     | html → 状态 + cgi 脚本     | 先判文章是否可用 (已删除/违规/风控), 再取出带 `h5only` 的那段 `<script>`                                                             |
| 3 求值 | `cgi.cpp`      | 脚本 → `cgiDataNew`        | 必须真 JS 引擎: 正文经 `JsDecode` 与 `\xNN` 双层转义, 正则还原不可靠                                                                 |
| 4 渲染 | `renderer.cpp` | `cgiDataNew` → 规范化 HTML | 三种 `item_show_type` 各一套正文组装; 标题/元信息/原文链接统一结构                                                                   |
| 5 转换 | `markdown.cpp` | HTML → Markdown            | turndown 规则集移植; 图片保留为 `![](cdn_url)` 不下载                                                                                |

## 用法

```bash
./run.py "https://mp.weixin.qq.com/s?__biz=..."      # 抓取并输出 Markdown
./run.py -f tests/samples/普通图文/c01.html          # 离线解析本地 HTML
./run.py "<url>" --meta -o out.md                    # 附元信息并写文件
./run.py --html -f <file>                            # 只看中间态 HTML
./run.py --build                                     # 只编译
./run.py --clean                                     # 清空 build/ 后重新编译
```
