history_n - 查询历史行情最新 n 条
查询标的最新n条的历史行情数据

函数原型：

history_n(symbol, frequency, count, end_time=None, fields=None, skip_suspended=True,
          fill_missing=None, adjust=ADJUST_NONE, adjust_end_time='', df=False)
 
        复制成功
    
参数：

参数名	类型	说明
symbol	str	标的代码(只允许单个标的的代码字符串)，使用时参考symbol
frequency	str	频率, 支持 'tick', '1d', '60s' 等, 默认 '1d', 详情见股票行情数据和期货行情数据, 实时行情支持的频率
count	int	数量(正整数)
end_time	str or datetime.datetime	结束时间 (%Y-%m-%d %H:%M:%S 格式), 也支持 datetime.datetime 格式，默认 None 时，用了实际当前时间，非回测当前时间
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有, 具体字段见:tick 对象 和 bar 对象
adjust	int	ADJUST_NONE or 0: 不复权, ADJUST_PREV or 1: 前复权, ADJUST_POST or 2: 后复权 默认不复权
adjust_end_time	str	复权基点时间, 默认当前时间
df	bool	是否返回 dataframe 格式, 默认 False, 返回 list[dict]
返回值:参考tick 对象 和 bar 对象。

当 df = True 时，返回

类型	说明
dataframe	tick 的 dataframe 或者 bar 的 dataframe
示例：

history_n_data = history_n(symbol='SHSE.600519', frequency='1d', count=100, end_time='2020-10-20 15:30:00', fields='symbol, open, close, low, high, eob', adjust=ADJUST_PREV, df=True)
 
        复制成功
    
输出：

 symbol       open  ...       high                       eob
0   SHSE.600519  1350.2278  ...  1350.3265 2020-05-22 00:00:00+08:00
1   SHSE.600519  1314.6434  ...  1350.8010 2020-05-25 00:00:00+08:00
2   SHSE.600519  1354.0629  ...  1354.1321 2020-05-26 00:00:00+08:00
3   SHSE.600519  1343.3086  ...  1344.2970 2020-05-27 00:00:00+08:00
4   SHSE.600519  1322.5214  ...  1331.3878 2020-05-28 00:00:00+08:00
 
        复制成功
    
当 df = False 时， 返回

类型	说明
list	tick 列表 或者 bar 列表
示例：

history_n_data = history_n(symbol='SHSE.600519', frequency='1d', count=2, end_time='2020-10-20 15:30:00', fields='symbol, open, close, low, high, eob', adjust=ADJUST_PREV, df=False)
 
        复制成功
    
输出：

[{'symbol': 'SHSE.600519', 'open': 1725.0, 'close': 1699.0, 'low': 1691.9000244140625, 'high': 1733.97998046875, 'eob': datetime.datetime(2020, 10, 19, 0, 0, tzinfo=tzfile('PRC'))}, {'symbol': 'SHSE.600519', 'open': 1699.989990234375, 'close': 1734.0, 'low': 1695.0, 'high': 1734.969970703125, 'eob': datetime.datetime(2020, 10, 20, 0, 0, tzinfo=tzfile('PRC'))}]

 
        复制成功
    
注意：

1. 返回的list/DataFrame是以参数eob/bob的升序来排序的

2. 若输入无效标的代码，返回空列表/空DataFrame

3. 若输入代码正确，但查询字段包含无效字段，返回的列表、DataFrame 只包含 eob、symbol和输入的其他有效字段

4. end_time 中月,日,时,分,秒均可以只输入个位数,例:'2017-7-30 20:0:20',但若对应位置为零，则0不可被省略,如不可输入'2017-7-30 20: :20'

5. skip_suspended 和 fill_missing 参数暂不支持

6. 单次返回数据量最大返回 33000, 超出部分不返回

7. end_time 输入不存在日期时，会报错 details = "Can't parse string as time: 2020-10-40 15:30:00"

8. 盘后18点清洗入库更新当天数据，需要盘后取数据的，请在18点后取。

