指数

数据名称	API接口	历史范围	更新时间

指数交易标的列表

获取指数交易列表	get_all_securities()	上市至今	8:00更新

指数估值

指数估值	 get_index_valuation	2005年至今	9点更新总股本、流通股本数据等股本数据，盘后 17:00 更新剩余字段

指数成分股及权重

指数成分股数据	get_index_stocks()	2005年至今	8:00更新

指数成分股权重(月度)	get_index_weights()	2005年至今	8:00更新

指数行情

get_price 移动窗口	get_price()	2005年至今	盘后15点更新，24点入库

get_bars 固定窗口	get_bars()	2005年至今	盘后15点更新，24点入库

获取集合竞价数据	get_call_acution()	2017年至今	盘后15点更新，24点入库

指数tick数据	get_ticks()	2017年至今	盘后15点更新，24点入库

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
