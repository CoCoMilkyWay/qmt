get_symbol_infos - 查询标的基本信息
获取指定(范围)交易标的基本信息，与时间无关.

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

get_symbol_infos(sec_type1, sec_type2=None, exchanges=None, symbols=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
sec_type1	int	证券品种大类	Y	无	指定一种证券大类，只能输入一个. 证券大类 sec_type1 清单 1010: 股票， 1020: 基金， 1030: 债券 ， 1040: 期货， 1050: 期权， 1060: 指数，1070：板块.
sec_type2	int	证券品种细类	N	None	指定一种证券细类，只能输入一个. 默认None表示不区分细类，即证券大类下所有细类. 证券细类见 sec_type2 清单 - 股票 101001:A 股，101002:B 股，101003:存托凭证 - 基金 102001:ETF，102002:LOF，102005:FOF，102009:基础设施REITs - 债券 103001:可转债，103008:回购 - 期货 104001:股指期货，104003:商品期货，104006:国债期货 - 期权 105001:股票期权，105002:指数期权，105003:商品期权 - 指数 106001:股票指数，106002:基金指数，106003:债券指数，106004:期货指数 - 板块：107001:概念板块
exchanges	str or list	交易所代码	N	None	输入交易所代码，可输入多个. 采用 str 格式时，多个交易所代码必须用英文逗号分割，如：'SHSE,SZSE' 采用 list 格式时，多个交易所代码示例：['SHSE', 'SZSE'] 默认None表示所有交易所. 交易所代码清单 SHSE:上海证券交易所，SZSE:深圳证券交易所 ， CFFEX:中金所，SHFE:上期所，DCE:大商所， CZCE:郑商所， INE:上海国际能源交易中心 ，GFEX:广期所
symbols	str or list	标的代码	N	None	输入标的代码，可输入多个. 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002'] 默认None表示所有标的.
df	bool	返回格式	N	False	是否返回 dataframe 格式，默认False返回字典格式，返回 list[dict]， 列表每项的 dict 的 key 值为 fields 字段.
返回值：

字段名	类型	中文名称	说明	股票字段	基金字段	债券字段	期货字段	期权字段	指数字段
symbol	str	标的代码	exchange.sec_id	√	√	√	√	√	√
sec_type1	int	证券品种大类	1010: 股票，1020: 基金， 1030: 债券，1040: 期货， 1050: 期权，1060: 指数，1070：板块	√	√	√	√	√	√
sec_type2	int	证券品种细类	- 股票 101001:A 股，101002:B 股，101003:存托凭证 - 基金 102001:ETF，102002:LOF，102005:FOF，102009:基础设施REITs - 债券 103001:可转债，103003:国债，103006:企业债，103008:回购 - 期货 104001:股指期货，104003:商品期货，104006:国债期货 - 期权 105001:股票期权，105002:指数期权，105003:商品期权 - 指数 106001:股票指数，106002:基金指数，106003:债券指数，106004:期货指数 - 板块：107001:概念板块	√	√	√	√	√	√
board	int	板块	A 股 10100101:主板 A 股 10100102:创业板 10100103:科创版 10100104:北交所股票 ETF 10200101:股票 ETF 10200102:债券 ETF 10200103:商品 ETF 10200104:跨境 ETF 10200105:货币 ETF 可转债 10300101:普通可转债 10300102:可交换债券 10300103:可分离式债券	√	√	√	无	无	无
exchange	str	交易所代码	SHSE:上海证券交易所， SZSE:深圳证券交易所， CFFEX:中金所， SHFE:上期所， DCE:大商所， CZCE:郑商所， INE:上海国际能源交易中心 ，GFEX:广期所	√	√	√	√	√	√
sec_id	str	交易所标的代码	股票,基金,债券,指数的证券代码; 期货,期权的合约代码	√	√	√	√	√	√
sec_name	str	交易所标的名称	股票,基金,债券,指数的证券名称; 期货,期权的合约名称	√	√	√	√	√	√
sec_abbr	str	交易所标的简称	拼音或英文简称	√	√	√	√	√	√
price_tick	float	最小变动单位	交易标的价格最小变动单位	√	√	√	√	√	√
trade_n	int	交易制度	0 表示 T+0，1 表示 T+1，2 表示 T+2	√	√	√	√	√	√
listed_date	datetime.datetime	上市日期	证券/指数的上市日、衍生品合约的挂牌日	√	√	√	√	√	√
delisted_date	datetime.datetime	退市日期	股票/基金的退市日， 期货/期权的到期日(最后交易日)， 可转债的赎回登记日	√	√	√	√	√	√
underlying_symbol	str	标的资产	期货/期权的合约标的物 symbol，可转债的正股标的 symbol	无	无	√	√	√	无
option_type	str	行权方式	期权行权方式，仅期权适用，E:欧式，A:美式	无	无	无	无	√	无
option_margin_ratio1	float	期权保证金计算系数 1	计算期权单位保证金的第 1 个系数，仅期权适用	无	无	无	无	√	无
option_margin_ratio2	float	期权保证金计算系数 2	计算期权单位保证金的第 2 个系数，仅期权适用	无	无	无	无	√	无
call_or_put	str	合约类型	期权合约类型，仅期权适用，C:Call(认购或看涨)， P:Put(认沽或看跌)	无	无	无	无	√	无
conversion_start_date	datetime.datetime	可转债开始转股日期	可转债初始转股价的执行日期，仅可转债适用	无	无	√	无	无	无
delisting_begin_date	datetime.datetime	退市整理开始日	股票退市整理期的开始日，退市公告前为空，退市整理期：[退市整理开始日, 退市日前一个交易日]	√	无	无	无	无	无
示例：

get_symbol_infos(sec_type1=1010, symbols='SHSE.600008,SZSE.000002')
 
        复制成功
    
输出：

[{'symbol': 'SHSE.600008', 'sec_type1': 1010, 'sec_type2': 101001, 'board': 10100101, 'exchange': 'SHSE', 'sec_id': '600008', 'sec_name': '首创环保', 'sec_abbr': 'SCHB', 'price_tick': 0.01, 'trade_n': 1, 'listed_date': datetime.datetime(2000, 4, 27, 0, 0, tzinfo=tzfile('PRC')), 'delisted_date': datetime.datetime(2038, 1, 1, 0, 0, tzinfo=tzfile('PRC')), 'underlying_symbol': '', 'option_type': '', 'option_margin_ratio1': 0.0, 'option_margin_ratio2': 0.0, 'call_or_put': '', 'conversion_start_date': None},
 {'symbol': 'SZSE.000002', 'sec_type1': 1010, 'sec_type2': 101001, 'board': 10100101, 'exchange': 'SZSE', 'sec_id': '000002', 'sec_name': '万科A', 'sec_abbr': 'WKA', 'price_tick': 0.01, 'trade_n': 1, 'listed_date': datetime.datetime(1991, 1, 29, 0, 0, tzinfo=tzfile('PRC')), 'delisted_date': datetime.datetime(2038, 1, 1, 0, 0, tzinfo=tzfile('PRC')), 'underlying_symbol': '', 'option_type': '', 'option_margin_ratio1': 0.0, 'option_margin_ratio2': 0.0, 'call_or_put': '', 'conversion_start_date': None}]
 
        复制成功
    
注意：

1. sec_type1为必填参数，即一次只能查询一个品种的标的基本信息。

2. 查询的标的信息根据参数组合sec_type1, sec_type2, exchanges, symbols取交集，若输入参数之间出现任何矛盾（换句话说，所有的参数限制出满足要求的交集为空)，则返回空list/空DataFrame ，例如get_symbol_infos(sec_type1=1040，exchanges='SZSE')返回的是空值。

3. 若输入包含无效标的代码symbols，则返回的list/DataFrame只包含有效标的代码对应的数据。

4. 参数组合示例：

查询以下范围 symbol 的基本信息	sec_type1	sec_type2	exchanges	symbols
查询指定股票	1010	None	None	'SHSE.600008,SZSE.000002'
查询 A 股股票	1010	101001	None	None
查询深交所股票	1010	None	'SZSE'	None
查询 ETF	1020	102001	None	None
查询上交所 LOF	1020	102002	'SHSE'	None
查询可转债	1030	103001	None	None
查询深交所可转债	1030	103001	'SZSE'	None
查询股指期货	1040	104001	None	None
查询商品期货	1040	104003	None	None
查询郑商所和大商所期货	1040	None	'CZCE,DCE'	None
查询股票期权	1050	105001	None	None
查询上交所股票期权	1050	105001	'SHSE'	None
查询指数期权	1050	105002	None	None
查询商品期权	1050	105003	None	None
查询上期所商品期权	105003	None	'SHFE'	None
查询股票指数	1060	106001	None	None
