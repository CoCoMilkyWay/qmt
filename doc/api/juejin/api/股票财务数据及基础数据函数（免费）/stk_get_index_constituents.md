stk_get_index_constituents - 查询指数成分股
查询指定指数在最新交易日的成分股和权重(中证系列指数，因版权不提供成分股权重，weight=0)

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

stk_get_index_constituents(index, trade_date=None)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
index	str	指数代码	Y	无	必填，只能输入一个指数，如：'SHSE.000905'
trade_date	str	交易日期	N	None	交易日期，%Y-%m-%d 格式， 默认None为最新交易日
返回值：dataframe

字段名	类型	中文名称	说明
index	str	指数代码	查询成分股的指数代码
symbol	str	成分股代码	exchange.sec_id
weight	float	成分股权重	成分股 symbol 对应的指数权重 (中证系列指数不支持该字段）
trade_date	str	交易日期	最新交易日，%Y-%m-%d 格式
market_value_total	float	总市值	单位：亿元
market_value_circ	float	流通市值	单位：亿元
示例：

stk_get_index_constituents(index='SHSE.000300')
 
        复制成功
    
输出：

          index       symbol  weight  trade_date  market_value_total  market_value_circ
0    SHSE.000300  SHSE.600519    0.05  2023-04-18            22083.96           22083.96
1    SHSE.000300  SZSE.300750    0.03  2023-04-18             9989.35            8822.91
2    SHSE.000300  SHSE.601318    0.03  2023-04-18             8887.85            5266.84
3    SHSE.000300  SHSE.600036    0.02  2023-04-18             8998.44            7360.41
4    SHSE.000300  SZSE.000858    0.02  2023-04-18             6921.68            6921.39
5    SHSE.000300  SZSE.000333    0.01  2023-04-18             3972.72            3891.18
6    SHSE.000300  SHSE.601166    0.01  2023-04-18             3616.80            3616.80
7    SHSE.000300  SHSE.600900    0.01  2023-04-18             5030.92            4834.92
8    SHSE.000300  SHSE.601012    0.01  2023-04-18             3033.36            3031.97
9    SHSE.000300  SZSE.300059    0.01  2023-04-18             2859.02            2399.14
10   SHSE.000300  SZSE.002594    0.01  2023-04-18             7248.75            2900.26...
 
        复制成功
    
注意：

1. 数据日频更新，在交易日约 20 点更新当日数据。如果当日数据尚未更新，调用时不指定trade_date会返回前一交易日的成分数据，调用时指定trade_date为当日会返回空 dataframe。

2. trade_date输入非交易日，会返回空 dataframe。trade_date出入的日期格式错误，会报错。

3. 指数列表参考

