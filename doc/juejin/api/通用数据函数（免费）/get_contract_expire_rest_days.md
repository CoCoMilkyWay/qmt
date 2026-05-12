get_contract_expire_rest_days - 查询合约到期剩余天数
查询期货合约、期权合约、可转债的到期剩余天数。

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

get_contract_expire_rest_days(symbols, start_date=None, end_date=None, trade_flag = False, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	标的代码	Y	无	输入标的代码，可输入多个. 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'CFFEX.IF2212,CFFEX.IC2212' 采用 list 格式时，多个标的代码示例：['CFFEX.IF2212', CFFEX.IC2212'].
start_date	str or datetime	开始日期	N	None	%Y-%m-%d 格式，不早于合约上市日 默认None表示最新时间.
end_date	str or datetime	结束日期	N	None	%Y-%m-%d 格式，不早于指定的开始日期，否则返回报错 默认None表示最新时间.
trade_flag	bool	交易日	N	False	是否需要按交易日计算，默认False按自然日计算，则返回到期剩余自然日天数; 设置为True按交易日计算，则返回到期剩余交易日天数
df	bool	返回格式	N	False	是否返回 dataframe 格式， 默认False返回字典格式，返回list[dict]，列表每项的 dict 的 key 值见返回字段名
返回值：

字段名	类型	中文名称	说明
date	str	日期	[开始日期,结束日期]内的自然日期
symbol	str	合约代码	exchange.sec_id
days_to_expire	int	到期剩余天数	合约在指定交易时间至合约到期日的剩余天数. trade_flag=False，计算方法按自然日 trade_flag=True，计算方法按交易日
示例：

get_contract_expire_rest_days(symbols='CFFEX.IM2212', start_date='2022-12-12', end_date='2022-12-16', trade_flag = True, df=True)
 
        复制成功
    
输出：

         date        symbol  days_to_expire
0  2022-12-12  CFFEX.IM2212               4
1  2022-12-13  CFFEX.IM2212               3
2  2022-12-14  CFFEX.IM2212               2
3  2022-12-15  CFFEX.IM2212               1
4  2022-12-16  CFFEX.IM2212               0
 
        复制成功
    
注意：

1. 参数start_date和end_date必须是 pd.to_dateime()可识别的字符串 str 格式，例'yyyy-mm-dd'， 'yyyy-mm-dd %H:%M:%S'，或者是 datetime 对象

2. 在到期日当天，到期剩余天数为 0。正数表示距离到期日的剩余天数，0 表示到期日当天，负数表示距离到期日已经过去的天数。

3. 如果输入不存在的合约代码symbol，会报错提示"该合约[symbol]不存在"。

4. 如果输入的合约代码symbol在时间段内的某个日期未上市，在该日期的到期剩余天数返回 NaN。

5. 用于剩余天数计算的到期日是最后交易日。