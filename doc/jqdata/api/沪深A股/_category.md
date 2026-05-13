沪深A股接口

试用账号历史范围：前15个月~前3个月 ; 正式账号历史范围：不限制

数据名称	API接口	历史范围	更新时间

股票交易标的列表

将标的代码转化成聚宽标准格式	normalize_code()

获取所有标的信息	get_all_securities()	上市至今	8:00更新

获取单支标的信息	get_security_info()	上市至今	8:00更新

股票标的交易信息

获取股票当日盘前交易信息
get_preopen_infos()	2005年至今	盘前9:15更新

股票ST信息	get_extras()	2005年至今	盘前9:15更新

上市公司状态变动	finance.STK_STATUS_CHANGE	2005年至今	交易日24:00更新

融资融券信息

获取股票的融资融券信息	get_mtss()	2010年至今	下一个交易日9点之前更新

融资标的列表	get_margincash_stocks()	2010年至今	每天21:00更新下一交易日

融券标的列表	get_marginsec_stocks()	2010年至今	每天21:00更新下一交易日

融资融券汇总数据	finance.STK_MT_TOTAL	2010年至今	下一个交易日9点之前更新

交易统计数据

股票资金流向	get_money_flow_pro()	2010年至今	盘后20:00更新

股票龙虎榜数据	get_billboard_list()	2005年至今	盘后20:00和22:00更新

沪深市场每日成交概况	finance.STK_EXCHANGE_TRADE_INFO)	2005年至今	交易日20:30-24:00更新

行业指数日行情及估值数据

行业概念及成分股数据

行业列表	get_industries()	2005年至今	8:00更新

行业成份股	get_industry_stocks()	2005年至今	8:00更新

查询股票所属行业	get_industry()	2005年至今	8:00更新

概念列表	get_concepts()	2005年至今	8:00更新

概念成分股	get_concept_stocks()	2005年至今	8:00更新

股票所属概念板块	get_concept()	2005年至今	8:00更新

沪股通，深股通和港股通(市场通数据）

市场通交易日历	finance.STK_EXCHANGE_LINK_CALENDAR	上市至今	交易日20:30-06:30更新

市场通AH股价格对比	finance.STK_AH_PRICE_COMP	上市至今	交易日20:30-06:30更新

市场通合格证券变动记录	finance.STK_EL_CONST_CHANGE	上市至今	交易日20:30-06:30更新

沪深港通持股数据	finance.STK_HK_HOLD_INFO	上市至今	交易日20:30-06:30更新

市场通十大成交活跃股	finance.STK_EL_TOP_ACTIVATE	上市至今	交易日20:30-06:30更新

市场通成交与额度信息	finance.STK_ML_QUOTA	上市至今	交易日20:30-06:30更新

市场通汇率	finance.STK_EXCHANGE_LINK_RATE	上市至今	交易日20:30-06:30更新

股票历史行情

get_price 移动窗口	get_price()	2005年至今	盘后15:00更新，24:00校对完成入库

get_bars 固定窗口	get_bars()	2005年至今	盘后15:00更新，24:00校对完成入库

获取集合竞价数据	get_call_auction()	2010年至今	盘后15:00更新，24:00校对完成入库

股票tick数据	get_ticks()	2010年至今	盘后15:00更新，24:00校对完成入库

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
