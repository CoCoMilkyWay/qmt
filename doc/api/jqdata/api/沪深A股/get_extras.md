股票ST信息

历史范围：2005年至今；更新时间：盘前09:15更新

get_extras(info, security_list, start_date='2015-01-01', end_date='2015-12-31', df=True, count=None)

获取的是相对广义的一个 st状态,   股改s,  st, *st和退市整理期标的 都会被判定为 True

参数

参数名称	参数说明	注释

info	['is_st']	是则返回 True，否则返回 False

start_date/end_date	开始/结束日期,

df	True或False	df=True返回[pandas.DataFrame]对象
列索引是股票代号, 行索引是[datetime.datetime]

df=False，返回一个dict, key是股票代号，案例如下

count	数量	与 start_date 二选一, 不可同时使用, 必须大于 0。
表示取 end_date 往前的 count 个交易日的数据

代码示例

当df=True，返回为dataframe

# 平安银行和神城A在2021-12-01至2021-12-03是否为ST
get_extras('is_st', ['000001.XSHE', '000018.XSHE'], start_date='2021-12-01', end_date='2021-12-03')

# 输出：
000001.XSHE 000018.XSHE
2021-12-01    False   True
2021-12-02    False   True
2021-12-03    False   True
