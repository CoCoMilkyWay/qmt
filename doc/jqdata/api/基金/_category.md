基金

数据名称	API接口	更新时间

基金交易标的列表

将标的代码转化成聚宽标准格式	normalize_code()

获取基金交易列表	get_all_securities()	8:00更新

获取单支标的信息	get_security_info()	8:00更新

基金主体信息

基金的主体信息	finance.FUND_MAIN_INFO

基金投资组合

基金持股信息	finance.FUND_PORTFOLIO_STOCK	盘后24:00更新

基金持有的债券信息	finance.FUND_PORTFOLIO_BOND	盘后24:00更新

基金资产组合概况	finance.FUND_PORTFOLIO	盘后24:00更新

基金财务指标表

基金财务指标表	finance.FUND_FIN_INDICATOR	盘后24:00更新

基金分红信息

基金分红、拆分和合并的方案	finance.FUND_DIVIDEND	盘后24:00更新

净值及业绩表现

场内基金份额数据	finance.FUND_SHARE_DAILY	下一个交易日9点20分前更新

基金收益日报信息	finance.FUND_MF_DAILY_PROFIT	盘后17点到下一交易日9点

基金净值信息	finance.FUND_NET_VALUE	盘后17点到下一交易日9点

基金累计净值/基金单位净值/场外基金的复权净值	get_extras()	盘后17点到下一交易日9点

获取etf跟踪指数信息

获取ETF跟踪指数信息	FUND_INVEST_TARGET	24点更新

场内基金行情

get_price 移动窗口	get_price()	盘后15点更新，24点入库

get_bars 固定窗口	get_bars()	盘后15点更新，24点入库

获取集合竞价数据	get_extras()	盘后15点更新，24点入库

基金tick数据	get_ticks()	盘后15点更新，24点入库

基金融资融券信息

获取基金的融资融券信息	get_mtss()	下一个交易日9点之前更新

基金融资标的列表	get_margincash_stocks()	下一个交易日9点之前更新

基金融券标的列表	get_marginsec_stocks()	下一个交易日9点之前更新

normalize_code(code)：将标的代码转化成聚宽标准格式

交易市场	代码后缀	示例代码	证券简称

上海证券交易所	.XSHG	600519.XSHG	贵州茅台

深圳证券交易所	.XSHE	000001.XSHE	平安银行

中金所	.CCFX	IC9999.CCFX	中证500主力合约

大商所	.XDCE	A9999.XDCE	豆一主力合约

上期所	.XSGE	AU9999.XSGE	黄金主力合约

郑商所	.XZCE	CY8888.XZCE	棉纱期货指数

上海国际能源期货交易所	.XINE	SC9999.XINE	原油主力合约

广州期货交易所	.GFEX	SI9999.GFEX	工业硅主力合约

场外基金	.OF	398051.OF	中海环保新能源混合
