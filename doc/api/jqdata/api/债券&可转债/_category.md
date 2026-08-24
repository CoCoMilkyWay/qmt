债券&可转债

数据名称	API接口	历史范围	更新时间

债券基本资料

债券基本信息	bond.BOND_BASIC_INFO	上市至今	每日19：00、22:00更新

债券票面利率	bond.BOND_COUPON	上市至今	每日19：00、22:00更新

债券付息事件	bond.BOND_INTEREST_PAYMENT	上市至今	每日19：00、22:00更新

国债逆回购日行情数据	bond.REPO_DAILY_PRICE	上市至今	每日19：00、22:00更新

可转债交易标的列表

获取所有可转债标的信息	get_all_securities()	上市至今	8:00更新

获取单支可转债标的信息	get_security_info()	上市至今	8:00更新

可转债基本资料

可转债基本资料	bond.CONBOND_BASIC_INFO	上市至今	每日19：00、22:00更新

可转债转股价格调整	bond.CONBOND_CONVERT_PRICE_ADJUST	上市至今	每日19：00、22:00更新

可转债每日转股统计	bond.CONBOND_DAILY_CONVERT	2000/7/12至今	下一交易日 8:30、12：30更新

可转债历史行情

可转债日行情数据（查表）	bond.CONBOND_DAILY_PRICE	2018/9/13至今	每日19：00、22:00更新

可转债集合竞价	get_call_auction	2019年至今	盘后15点更新，24点入库

get_price 移动窗口	get_price()	2019年至今	盘后15点更新，24点入库

get_bars 固定窗口	get_bars()	2019年至今	盘后15点更新，24点入库

获取集合竞价数据	get_call_auction()	2019年至今	盘后15:00更新，24:00校对完成入库

可转债tick数据	get_ticks()	2019年至今	盘后15点更新，24点入库

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
