Motive: 公众号文章链接 → Markdown;

# 项目结构

```
gzh/
├── run.py                                  # 统一入口: cmake configure (静默) + build + 运行 build/wxmd
│                                           #   未识别的参数原样透传给 wxmd; --build 只编译, --clean 清空 build/
├── CMakeLists.txt                          # 顶层构建: vendored 依赖 + libwxmd.a + wxmd CLI
│                                           #   C++20; 系统依赖仅 OpenSSL (cpp-httplib 的 HTTPS 后端 + 代理的 TLS 与签证书)
│                                           #   + zlib (gzip: 正文压完只剩 22%, 占全部下载字节的 86%) + pthread
├── include/wxmd/                           # 公共头; 一层一个 header, 与 src/ 同名文件一一对应
│   ├── model.hpp                           # 三个统一数据结构, 全项目共用:
│   │                                       #   Account {biz/nickname + uin/key/pass_ticket/cookie/source_url/captured_ms}
│   │                                       #     — 一个号: 身份是 __biz, 后半截是抓包得来的凭证 (分钟级过期)
│   │                                       #   Entry {id/datetime/title/link/status/dir/assets}
│   │                                       #     — 一篇文章: index.jsonl 与 pending.jsonl 共用同一套字段, 差别只在 status
│   │                                       #   AssetFile {name/bytes} — 正文里的一张图, 字节攒在内存里等原子提交
│   ├── wxmd.hpp                            # 顶层门面: render_article_markdown(cgi 脚本, 图片钩子) —— 流水线后三级
│   │                                       #   标题/作者/公众号/时间/原文链接由 renderer 直接渲进正文, 不再另开一套结构
│   ├── sync.hpp                            # 主 flow: sync_account(root, account) — 先补齐再发现, 见「流程」一节
│   ├── egress.hpp                          # EgressPolicy: 请求怎么离开这台机器, 全项目唯一的出网描述
│   │                                       #   隧道 (每请求换 IP + 并发) / 直连 (串行), 由 config::kUseTunnel
│   │                                       #   在编译期定死, 全程不变, 不再运行时探测
│   │                                       #   两种方式的频率都只用 qps 描述, throttle() 是唯一的限速点
│   │                                       #   (不叫 proxy 是因为这名字已被 MitmProxy 与 proxy-backup.json 占了)
│   ├── fetch.hpp                           # fetch_raw(url, cookie, route) → 响应体; fetch_asset(url, seq) → AssetFile
│   │                                       #   Route {Tunneled/Direct}: 默认走隧道 (没开就自动退回直连),
│   │                                       #   带凭证的那一路显式 Direct
│   │                                       #   图片在腾讯 CDN 上, 另开一个白名单; 扩展名先看 Content-Type 再退回 wx_fmt
│   ├── html.hpp                            # ArticleStatus {Success/Deleted/Exception/Error} + status_text
│   │                                       #   parse_article(html) → ArticlePage {status/message/cgi_script}:
│   │                                       #   判状态与抠脚本是同一次解析的两个产物 (正文 3.05MB, 不解析两遍)
│   ├── cgi.hpp                             # eval_cgi(script) → nlohmann::json (即 window.cgiDataNew)
│   ├── renderer.hpp                        # render_html(cgi) → 规范化 HTML (标题/元信息/原文链接/正文统一结构)
│   ├── markdown.hpp                        # to_markdown(html) → Markdown
│   ├── assets.hpp                          # AssetHook: img src → 本地相对路径; localize_images
│   ├── dom.hpp                             # lexbor RAII 封装 (Document/query/text_content/attr) + MdNode 可变树
│   ├── profile.hpp                         # probe_credential(account) / fetch_new_entries(account, since, known)
│   │                                       #   / fetch_account_name(article_url)
│   │                                       #   Entry.id = mid_idx (跨轮次稳定的文章身份, 增量靠它对账)
│   │                                       #   翻页早停的水位线 = since + known 回调, 整趟翻完才返回
│   ├── store.hpp                           # AccountStore: 一个号的本地缓存目录 (index/pending + 逐篇原子提交)
│   │                                       #   commit(entry, markdown, assets) — markdown 为空即墓碑; list_accounts
│   ├── proxy.hpp                           # MitmProxy: 只对 mitm_hosts 做中间人, 其余盲转发; Exchange {url/cookie/set_cookies}
│   ├── tlsca.hpp                           # CertAuthority: 自签 CA 落盘复用, 按域名现签叶子证书 (CertPair)
│   ├── capture.hpp                         # parse_exchange: 把 Exchange 认成 Account; Credentials 按 __biz 去重 + 自带落盘
│   │                                       #   构造即读回 ~/.wxmd/credentials.json (0600), 已过期的直接丢掉; save() 显式写回
│   ├── desktop.hpp                         # 桌面集成: 系统代理读写/备份还原 + CA 装进 NSS 库 (全项目唯一知道 GNOME+NSS 的地方)
│   └── assert.hpp                          # WXMD_ASSERT: Release 下同样生效 — 本项目以「尽早失败」替代错误处理
│                                           #   另有 warn(): 非致命但不符预期的情况不 silent, 喊一声
├── src/                                    # 实现; 五级流水线 fetch → html → cgi → renderer → markdown (见下节)
│   ├── egress.cpp                          # 两套参数写死在 src/config.hpp: 隧道 (快代理入口/账号密码 + 10 次/s
│   │                                       #   + 32 并发) 与直连 (2 次/s + 1 并发), 由 config::kUseTunnel 选一个,
│   │                                       #   开局定死不再变 (不再运行时探测)
│   │                                       #   throttle() 是两种方式共用的唯一闸门, 均匀发牌
│   │                                       #     (不攒桶爆发: 隧道对持续超频直接拒 441, 直连突发只会更显眼)
│   │                                       #   额度怎么定的算在注释里, 全是实测: 一篇 4.85 请求 / gzip 后 1.18MB
│   │                                       #     ⇒ qps 顶 2.06 篇/s, 20Mbps (2.5MB/s) 顶 2.12 篇/s — 两者同时吃满, qps 略先到顶
│   │                                       #   并发是必需而非优化: 单流过隧道只有 ~330KB/s, 吃不满 2.5MB/s 的额度;
│   │                                       #     且带宽是订单总额不是单 IP 独享 (6 路并发聚合仍卡在 599KB/s)
│   ├── fetch.cpp                           # cpp-httplib + OpenSSL; 伪装微信内置浏览器 UA
│   │                                       #   刻意不设 Accept-Encoding: httplib 只在请求没带这个头时才自己填
│   │                                       #     gzip 并自动解压, 手写 identity 等于把压缩关掉
│   │                                       #   正文页只认 mp.weixin.qq.com, 图片按域名后缀限定在
│   │                                       #     .qpic.cn/.qlogo.cn/.qq.com (微信换域名比枚举勤)
│   │                                       #   走隧道时 set_proxy + 关 keep-alive (复用连接就换不了出口 IP)
│   │                                       #     + 状态码也进重试: 隧道每请求换 IP ⇒ 重试就是换一个 IP 再来,
│   │                                       #     撞上被微信拉黑的出口 (403) 或隧道自己抖动 (517/超时) 换几次就过;
│   │                                       #     5 次还拿不到 200 就返回空 (由 sync 记成空洞). 直连只在没拿到
│   │                                       #     响应时重试, 且指数退避: 拿到响应后状态码是真实信号
│   │                                       #   fetch_asset 顺带把落盘文件名定下来 (扩展名逻辑也并进了这里)
│   ├── html.cpp                            # 移植 shared/utils/html.ts: 按 #js_article / .weui-msg / .mesg-block
│   │                                       #   判定文章状态; cgi 脚本 = 带 h5only 且含 "window.cgiDataNew = {" 的 <script>
│   │                                       #   一个 dom::Document 走完这两件事, 只有 Success 才去抠脚本
│   ├── cgi.cpp                             # 移植 shared/utils/cgi-sandbox.ts: 脚本内的 JsDecode 与转义正文必须真 JS 引擎才能还原.
│   │                                       #   沙箱只提供 window / console (无网络无文件), 2s 中断 + 256MB 内存上限;
│   │                                       #   在 isolate 内 JSON.stringify 回传 (顺带丢掉函数, 天然容纳超大 content_noencode)
│   ├── renderer.cpp                        # 移植 shared/utils/renderer.ts: item_show_type 0=普通图文 / 8=图片分享 / 10=文本分享
│   │                                       #   + 未购买付费文章的提示块; 图文正文做 data-src→src、去 height;
│   │                                       #   图片分享给「图N」锚点补 href; 不含评论与底部互动栏 (本项目只导出正文)
│   │                                       #   标题/作者/公众号/时间/原文链接统一由 render_meta 渲进 __meta__ 块
│   ├── markdown.cpp                        # turndown 7.2.1 规则集手写移植, 占全部实现的一半篇幅. 四块:
│   │                                       #   collapse_whitespace (DOM 预处理) / escape_markdown (13 条转义)
│   │                                       #   / flanking_whitespace (内联元素首尾空白外移) / apply_rule (各标签规则)
│   │                                       #   配置对齐上游 markdown.ts: atx 标题 / "-" 列表符 / fenced 代码块,
│   │                                       #   并移除 style·script·noscript·link·meta·title 与 __bottom-bar__
│   ├── assets.cpp                          # 在「渲染完 HTML、还没转 Markdown」之间改写 img src ⇒ 不必动 markdown 规则集;
│   │                                       #   同一地址只回调一次; data: 内联图原样留着
│   ├── dom.cpp                             # lexbor 侧全部实现; build_tree 把 lexbor DOM 抽成 MdNode 轻量树 —
│   │                                       #   turndown 要频繁改写文本与删节点, 在自有树上做远比改 lexbor DOM 简单
│   ├── profile.cpp                         # mp/profile_ext?action=getmsg 翻页; general_msg_list 是嵌套 JSON 字符串,
│   │                                       #   一条群发可含多篇 (multi_app_msg_item_list); 按 mid_idx 去重, 整页重复即判到底
│   │                                       #   article_id: 从 content_url 取 mid+idx — 整条链接不能当身份用,
│   │                                       #   chksm/#rd 这些尾巴跨轮次不保证一致, 增量就对不上账了
│   │                                       #   全程 Route::Direct: key 是微信客户端在本机 IP 上拿到的,
│   │                                       #   同一个 key 从一堆轮换 IP 发请求更像异常, 且这一路换 IP 换不出吞吐
│   │                                       #   每页条数与翻页间隔写死在文件顶部
│   ├── store.cpp                           # 本地缓存的全部落盘逻辑 (布局见下节); 每篇 = 临时目录写满 → rename → 追加索引行
│   │                                       #   入库允许乱序、允许留空洞: 某篇抓不到就跳过 (仍在 pending, 下轮补),
│   │                                       #   所以水位线 = 已入库里最新的 (取 max), 而非「最后提交的」
│   │                                       #   index.jsonl 与 pending.jsonl 共用一对 entry_json/parse_entry
│   ├── sync.cpp                            # 主 flow: fetch_article (抓一篇, 不落盘) / drain (排空队列) / discover (增量翻页)
│   │                                       #   / sync_account (先补齐再发现)
│   │                                       #   drain 抓取并发、提交串行但乱序: 哪篇抓完哪篇先入库, 不死等队头 —
│   │                                       #   严格按序 + 队头一篇慢 = 后面抓完的全干等、进度停死; 放开顺序后慢的那篇
│   │                                       #   自己留作空洞 (仍在 pending, 下轮补), 其余照常前进;
│   │                                       #   fetch_article 抓不到正文返回空 (空洞); 任一张图抓不到也算空洞 ——
│   │                                       #   入库要「连图完整」, 宁可整篇重抓, 不在索引里堆缺图的半成品;
│   │                                       #   派发窗口取 workers ⇒ 「在抓的 + 等提交的」合计不超过 workers 篇,
│   │                                       #   否则几十篇的图片字节会一起挂在内存里 (实测一篇连图 660KB)
│   ├── proxy.cpp                           # 裸 socket + OpenSSL 手写 CONNECT 代理: 目标域名解 TLS 抠 url/Cookie/Set-Cookie,
│   │                                       #   其余域名两个 fd 对拷. 只宣告 ALPN http/1.1 (不解 HTTP/2 分帧),
│   │                                       #   把请求改成 Connection: close ⇒ 响应体读到对端关闭为止, 省掉全部分帧逻辑;
│   │                                       #   只监听 127.0.0.1; 原系统代理非空时作为父代理续上, 其它流量走向不变
│   ├── tlsca.cpp                           # OpenSSL 手搓 CA 与叶子证书 (SAN/EKU/AKID 齐全, 叶子 397 天), 叶子共用一把私钥
│   ├── capture.cpp                         # 从 url 的 query 里取 __biz/uin/key/pass_ticket (百分号解码), 四者齐全才算凭证;
│   │                                       #   Cookie 优先取请求头的, 响应 Set-Cookie 只是兜底;
│   │                                       #   Credentials 自带路径, 构造即读回, save() 落盘 0600
│   ├── desktop.cpp                         # gsettings 读写 org.gnome.system.proxy (socks 清空, 否则会被优先选中);
│   │                                       #   接管前把原值存 ~/.wxmd/proxy-backup.json; certutil 把 CA 写进 ~/.pki/nssdb
│   ├── wxmd.cpp                            # 串流水线后三级: eval_cgi → render_html → [localize_images] → to_markdown
│   ├── config.hpp                          # 全项目唯一的可调参数集 (与 strutil/fsutil 一样是内部头, 不进 include/):
│   │                                       #   kUseTunnel 选出网方式, 隧道入口/账号密码/qps/并发, 直连 qps,
│   │                                       #   翻页每页条数与间隔, UA, 目标主机, store 目录, dump 开关
│   ├── strutil.hpp                         # 内部字符串工具 (replace_all / trim / lowered / percent_decode / query_param …)
│   └── fsutil.hpp                          # 内部落盘工具: write_atomic (临时文件→fsync→rename→fsync 父目录) /
│                                           #   append_line (追加+fsync, 索引的提交点) / fsync_path / mkdirs
├── cli/
│   └── main.cpp                            # CLI 只有两个入口: 无参数进增量同步 (补齐→发现→加号),
│                                           #   --uninstall 卸载. 单篇 / -f 离线解析 / --html / --store / --add
│                                           #   全部删掉了: 同步这条 flow 已经覆盖, 多留的入口只会各自腐烂;
│                                           #   认不出的参数当场断言, 不静默忽略
│                                           #   出网走哪条路由 config::kUseTunnel 在编译期定死, 开局打印一声
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
└── store/                                  # 本地缓存 (gitignore, 路径写死在 src/config.hpp); 一个子目录一个公众号
    └── <__biz>/
        ├── account.json                     # {biz, nickname} — 目录名经过清洗, 反推不回 __biz, 原值记在这里
        ├── index.jsonl                      # 已入库文章, 一行一篇, 按提交顺序追加 (允许乱序、允许空洞)
        ├── pending.jsonl                    # 已发现未入库的队列, 按发布时间升序 (空洞也留在这里)
        ├── 2016-09-07_一个高手的交易之路_2247483651_1/
        │   ├── article.md                   # 标题/作者/公众号/时间/原文链接由 renderer 渲进正文开头; 图片链接指向 assets/
        │   └── assets/001.jpg …             # 序号即正文里的出现顺序, 扩展名由 Content-Type 定
        └── .staging/                        # 正在下的那一篇; 开局无条件清掉
```

```
main(argv):
    if argv 给了 --uninstall: 还原系统代理 + 摘 CA + 删 ~/.wxmd; return
    if argv 还有别的: 断言当场炸                                 # 不静默忽略认不出的参数
    run_sync("store")                                             # 出网路由 config::kUseTunnel 编译期定死  (egress.cpp)

run_sync(root):
    creds = Credentials(~/.wxmd/credentials.json)     # 构造即读回, 过期的直接丢掉  (capture.cpp)
    known = list_accounts(root)                        # 扫 store/ 下每个子目录的 account.json  (store.cpp)
    for account in known:                               # 段一: 先补齐已有的号
        creds.fill(account)                             # 表里有就把 key/pass_ticket 填进去
        sync_account(root, account)                     (sync.cpp)

    for account in capture_session(creds):              # 段二: 抓包收新凭证 (内部已落盘), 见下
        sync_account(root, account)                     # 新号走同一条 flow

sync_account(root, account):
    store = AccountStore(root, account)   # 开/建目录, 读回 index.jsonl + pending.jsonl, 清掉 .staging/  (store.cpp)
    drain(store)                          # 先把上次没下完的补齐: 不需要凭证, 永远能做
    if account 没有可用凭证 or not probe_credential(account):   # 过期是常态, 不是 bug  (profile.cpp)
        return                            # 跳过发现, 等下一轮再有凭证时接着补
    discover(store, account)
    drain(store)                          # 新发现的立刻接着落盘

drain(store):                             # 抓取并发, 提交串行但乱序 (不死等队头)
    queue = store.pending()               # 按发布时间升序
    workers = egress().workers            # 直连 1, 隧道 32                      (egress.cpp)

    并发 workers 个:                      # 只领 下标 < 已提交 + workers 的活 —— 这个窗口
        entry = queue[下一个下标]          #   同时框住 reorder buffer 与内存占用
        done = fetch_article(entry)       # 抓完不落盘, 字节先攒在内存里
                                          # 没有篇间等待: 频率由 throttle() 逐请求发牌
        if done 为空: 记成空洞 (仍在 pending, 下轮补)

    主线程 while 还有未处理:             # ready 里有谁就提交谁, 挑最小下标让入库尽量贴近发布序
        store.commit(...)                 # commit 逼出串行: .staging 只有一个目录
                                          # 不再断言 datetime >= 水位线: 乱序入库, 水位线 = max(已入库)

fetch_article(entry):                     # 可并发: 网络层无共享状态, quickjs 每次自建 runtime
    html = fetch_raw(entry.link)          # Route::Tunneled — 没开隧道时它自动走直连  (fetch.cpp)
    if html 为空: return 空                # 抓不到正文 = 空洞, 留在 pending 下轮补
    page = parse_article(html)            # 流水线 1+2 级: 一次解析, 出状态 + cgi 脚本  (html.cpp)
    if page.status != Success:            # 已删除/违规/风控, 永远抓不回来
        return 墓碑(entry, status)         # markdown 为空: 不建目录, 索引照样推进,
                                          #   不然流水线卡死在这一篇
    assets = []
    markdown = render_article_markdown(page.cgi_script,          (wxmd.cpp 门面)
        on_asset = url -> { 抓这张图; 抓不到就把整篇作空洞丢掉 })      (assets.cpp)
    # render_article_markdown 内部: eval_cgi 得 cgiDataNew (3求值, 真 JS 引擎, JsDecode+\xNN 双层转义)
    #                             -> render_html 拼标题/元信息/原文链接/正文 (4渲染, 按 item_show_type 分支)
    #                             -> localize_images 改写 <img src> (图片落到本地的插入点)
    #                             -> to_markdown (5转换, turndown 规则集移植)
    return (entry, markdown, assets)

discover(store, account):
    since = store.watermark()             # 已入库里最新的发布时间 (取 max), 空库为 0
    found = fetch_new_entries(account, since,                  (profile.cpp)
        known = id -> store.has(id))
    # fetch_new_entries 内部: 从最新一页往回翻 (mp/profile_ext, 带 key/pass_ticket)
    #   历史消息严格按群发时间倒序 -> 一出现 datetime < since 就整体早停
    #   但同一次群发的多篇共享同一个 datetime, 单看时间会把同批里还没入库的几篇一起挡掉,
    #   所以逐条再问一句 known(id): 已入库的跳过, 没入库的收进 found
    #   Entry.id 用 mid_idx (群发 id + 该次第几篇), 不用整条链接: chksm/#rd 尾巴跨轮次不稳定
    store.set_pending(found)              # 只有整趟翻完才写队列: 中途打断的话「已发现」和水位线
                                           # 之间会留一段没翻到的, 既不在 index 也不在 pending,
                                           # 下一轮早停又恰好停在水位线上, 永远不回头补;
                                           # set_pending 是合并而非覆盖, 上一轮 drain 留下的空洞不会被冲掉

capture_session(creds):
    检查 gsettings / certutil, 缺 certutil 就报装法并等确认        (desktop.cpp)
    proxy = MitmProxy(端口0交内核分配, 只监听回环, 原系统代理接为父代理)   (proxy.cpp)
    CA = 自签证书装进 ~/.pki/nssdb (只写当前用户)                   (tlsca.cpp)
    备份当前系统代理到 ~/.wxmd/proxy-backup.json, 接管系统代理        (desktop.cpp)
    注册 atexit + SIGINT/TERM/HUP/QUIT/ABRT/SEGV -> 还原系统代理并停代理   (cli/main.cpp)

    等待用户在微信里逐个点开想同步的号 (只对 mp.weixin.qq.com 解 TLS, 其余盲转发):
        for 每次 HTTP 往返 exchange:
            if account = parse_exchange(exchange):     # 认出 __biz/uin/key/pass_ticket  (capture.cpp)
                if creds.offer(account):                # 按 __biz 去重, 同号留最新一次
                    account.nickname = fetch_account_name(article_url)  # 回抓文章页解析号名  (profile.cpp)

    用户回车:
        还原系统代理, 停代理                              # 立刻还原, 之后都是本机直连
        return creds 里这一轮新收下的号
    用户按 q: return []

# 本地缓存原子性 (store.cpp), commit() 内部保证的不变式:
AccountStore.commit(entry, markdown, assets):        # markdown 为空即墓碑
    assert !store.has(entry.id)                        # 不重复入库
    assert entry.status 非空                             # 入库必带状态
    # 不再断言 datetime >= 水位线: 入库允许乱序, 水位线 = max(已入库 datetime)
    if markdown 非空:
        把 markdown + assets 全写进 .staging/ -> fsync
        rename .staging/ -> <日期>_<标题>_<id>/ (同目录 rename, 原子)  -> fsync 父目录
    append_line(index.jsonl, entry_json(entry))          # 这一行落盘才算入库, 是提交的分界点
    # 被打断: .staging/ 天然不在索引里, 下次启动无条件清掉, 不留半成品
    # 空洞: 某篇抓不到正文就不 commit, 它仍留在 pending, 下一轮 drain 再抓

# 不变式: index.jsonl 里每篇都原子落盘, 但篇间允许乱序、允许留空洞;
#         水位线 = 已入库里最新的发布时间, 空洞落在水位线之下、由 pending 保留, 下轮补。
```

## 用法

```bash
./run.py                                             # 增量同步 store/, 结束后起代理等着加新号
./run.py --uninstall                                 # 还原系统代理、移除 CA 与 ~/.wxmd (不动 store/)
./run.py --build                                     # 只编译
./run.py --clean                                     # 清空 build/ 后重新编译
```
