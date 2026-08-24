history - 查询历史行情
查询标的在指定时间段的历史行情数据

函数原型：

history(symbol, frequency, start_time, end_time, fields=None, skip_suspended=True,
        fill_missing=None, adjust=ADJUST_NONE, adjust_end_time='', df=False)
 
        复制成功
    
参数：

参数名	类型	说明
symbol	str or list	标的代码, 如有多个代码, 中间用 , (英文逗号) 隔开, 也支持 ['symbol1', 'symbol2'] 这种列表格式 ，使用参考symbol
frequency	str	频率, 支持 'tick', '1d', '60s' 等, 默认 '1d', 详情见股票行情数据和期货行情数据, 实时行情支持的频率
start_time	str or datetime.datetime	开始时间 (%Y-%m-%d %H:%M:%S 格式), 也支持 datetime.datetime 格式
end_time	str or datetime.datetime	结束时间 (%Y-%m-%d %H:%M:%S 格式), 也支持 datetime.datetime 格式
fields	str	指定返回对象字段, 如有多个字段, 中间用, 隔开, 默认所有, 具体字段见:tick 对象 和 bar 对象
adjust	int	ADJUST_NONE or 0: 不复权, ADJUST_PREV or 1: 前复权, ADJUST_POST or 2: 后复权 默认不复权
adjust_end_time	str	复权基点时间, 默认当前时间
df	bool	是否返回 dataframe 格式, 默认 False, 返回 list[dict]
返回值:参考tick 对象 和 bar 对象。

当 df = True 时， 返回

类型	说明
dataframe	tick 的 dataframe 或者 bar 的 dataframe
示例：

history_data = history(symbol='SHSE.000300', frequency='1d', start_time='2010-07-28',  end_time='2017-07-30', fields='open, close, low, high, eob', adjust=ADJUST_PREV, df= True)
 
        复制成功
    
输出：

          open      close        low       high                       eob
0     2796.4829  2863.7241  2784.1550  2866.4041 2010-07-28 00:00:00+08:00
1     2866.7720  2877.9761  2851.9961  2888.5991 2010-07-29 00:00:00+08:00
2     2871.4810  2868.8459  2844.6819  2876.1360 2010-07-30 00:00:00+08:00
3     2868.2791  2917.2749  2867.4500  2922.6121 2010-08-02 00:00:00+08:00
4     2925.2539  2865.9709  2865.7610  2929.6140 2010-08-03 00:00:00+08:00

 
        复制成功
    
当 df = False 时， 返回

类型	说明
list	tick 列表 或者 bar 列表
注意：

history_data = history(symbol='SHSE.000300', frequency='1d', start_time='2017-07-30',  end_time='2017-07-31', fields='open, close, low, high, eob', adjust=ADJUST_PREV, df=False)
 
        复制成功
    
输出：

[{'open': 3722.42822265625, 'close': 3737.873291015625, 'low': 3713.655029296875, 'high': 3746.520751953125, 'eob': datetime.datetime(2017, 7, 31, 0, 0, tzinfo=tzfile('PRC'))}]

 
        复制成功
    
1. 返回的list/DataFrame是以参数eob/bob的升序来排序的，若要获取多标的的数据，通常需进一步的数据处理来分别提取出每只标的的历史数据。

2. 获取数据目前采用前开后闭区间的方式，根据eob升序排序。

3. 若输入无效标的代码，返回空列表/空DataFrame。

4. 若输入代码正确，但查询字段包含无效字段，返回的列表、DataFrame 只包含 eob、symbol和输入的其他有效字段。

5. skip_suspended 和 fill_missing 参数暂不支持。

6. 日内数据单次返回数据量最大返回 33000, 超出部分不返回。

7. start_time 和 end_time 输入不存在日期时，会报错 details = "failed to parse datetime"。

8. 盘后18点清洗入库更新当天数据，需要盘后取数据的，请在18点后取。

