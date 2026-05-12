get_trading_dates_by_year - 查询年度交易日历
查询一个交易所的指定年份的交易日历.

此函数为掘金公版(体验版/专业版/机构版)函数，券商版以升级提示为准

函数原型：

get_trading_dates_by_year(exchange, start_year, end_year)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
exchange	str	交易所代码	Y	无	只能填写一个交易所代码 交易所代码清单: SHSE:上海证券交易所，SZSE:深圳证券交易所，CFFEX:中金所，SHFE:上期所，DCE:大商所，CZCE:郑商所，INE:上海国际能源交易中心，GFEX:广期所
start_year	int	开始年份	Y	无	查询交易日历开始年份（含），yyyy 格式
end_year	int	结束年份	Y	无	查询交易日历结束年份（含），yyyy 格式
返回值:dataframe

字段名	类型	中文名称	说明
date	str	自然日期	查询年份的自然日日期
trade_date	str	交易日期	查询年份的交易日日期，如果所在自然日不是交易日，交易日期为空字符串''
next_trade_date	str	下一交易日	交易日对应的下一交易日
pre_trade_date	str	上一交易日	交易日对应的上一交易日
示例：

# coding=utf-8
from __future__ import print_function, absolute_import
from gm.api import *


def init(context):

    # 实时模式
    if context.mode == MODE_LIVE:
        context.trade_date = get_trading_dates_by_year(exchange='SHSE', start_year=int(context.now.strftime('%Y')) - 1,
                                                       end_year=int(context.now.strftime('%Y')) + 1)
        context.trade_date.index = context.trade_date['date']
    # 回测模式
    else:
        context.trade_date = get_trading_dates_by_year(exchange='SHSE', start_year=int(context.backtest_start_time[:4]) - 1,
                                                       end_year=int(context.backtest_end_time[:4]) + 1)
        context.trade_date.index = context.trade_date['date']
    today = context.now.strftime('%Y-%m-%d')
    next_trade_date = context.trade_date.loc[today, 'next_trade_date']
    pre_trade_date = context.trade_date.loc[today, 'pre_trade_date']
    print('今天：{}, 上个交易日：{}， 下个交易日：{}'.format(today, pre_trade_date, next_trade_date))

    # 判断当天是否为交易日
    trade_date = context.trade_date['trade_date'].tolist()
    if context.now.strftime('%Y-%m-%d') not in  trade_date:
        print(context.now,"当前为非交易日")
    else:
        print(context.now, "当前为交易日")
 
        复制成功
    
输出：

今天：2023-08-21, 上个交易日：2023-08-18， 下个交易日：2023-08-22
 
        复制成功
    
示例：

get_trading_dates_by_year(exchange='SHSE', start_year=2020, end_year=2023)
 
        复制成功
    
输出：

            date next_trade_date pre_trade_date  trade_date
0     2020-01-01      2020-01-02     2019-12-31
1     2020-01-02      2020-01-03     2019-12-31  2020-01-02
2     2020-01-03      2020-01-06     2020-01-02  2020-01-03
3     2020-01-04      2020-01-06     2020-01-03
4     2020-01-05      2020-01-06     2020-01-03
         ...             ...            ...         ...
1456  2023-12-27      2023-12-28     2023-12-26  2023-12-27
1457  2023-12-28      2023-12-29     2023-12-27  2023-12-28
1458  2023-12-29      2024-01-02     2023-12-28  2023-12-29
1459  2023-12-30      2024-01-02     2023-12-29
1460  2023-12-31      2024-01-02     2023-12-29

[1461 rows x 4 columns]
 
        复制成功
    
注意：

1. exchange参数仅支持输入单个交易所代码，若代码错误，会报错

2. 开始年份必须不晚于结束年份，否则返回空dataframe

