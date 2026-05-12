stk_hk_inst_holding_info - 查询沪深港通标的港股机构持股数据
查询沪深港通标的港股机构持股数据

gm SDK 3.0.163 版本新增

交易所信息披露调整，数据最晚更新到2024.8.16，历史数据不受影响

函数原型：

stk_hk_inst_holding_info(symbols=None, trade_date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str	股票代码	N	None	输入标的代码，可输入多个. 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002'] 默认None表示所有标的。
trade_date	str or datetime.date	交易日期	N	None	交易日期，支持str格式（%Y-%m-%d 格式）和 datetime.date 格式，默认None表示最新交易日期。
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict],列表每项的dict的key值为参数指定的 fields 。
返回值：

字段名	类型	中文名称	说明
trade_date	str	最新交易日期	最新交易日期
symbol	str	证券代码	证券代码
sec_name	str	证券简称	证券简称
participant_name	str	参与者名称	参与者名称
cum_share_holding	int	累计持股量(股)	累计持股量(股)
cum_shares_rate	float	累计占已发行股份(%)	累计占已发行股份(%)
示例：

stk_hk_inst_holding_info(symbols='SHSE.600008,SZSE.000002', trade_date=None, df=False)
 
        复制成功
    
输出：

[{'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'cum_share_holding': 132319140, 'cum_shares_rate': 1.71},
 {'symbol': 'SZSE.000002', 'trade_date': '2024-01-25', 'sec_name': '万科A', 'cum_share_holding': 228964226, 'cum_shares_rate': 2.23}]
 
        复制成功
    
注意：

1. 数据日频更新，在交易日约20点更新当日数据。如果当前交易日数据尚未更新，调用时不指定trade_date会返回前一交易日的数据，调用时指定trade_date为当前交易日会返回空。

2. trade_date输入非交易日，会返回空。

