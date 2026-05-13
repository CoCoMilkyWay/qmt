获取期货合约的信息(合约乘数，最小报价单位，交易时间)

历史范围：2005年至今；更新时间：8:00更新

get_futures_info(securities=None, fields=('contract_multiplier','tick_size','trade_time'))

参数

securities: str或list，一个或多个标的代码

fields : 获取的信息，可为多个,支持'contract_multiplier':合约乘数，'tick_size':最小报价单位，'trade_time':交易时间(期货可能调整交易时间，所以可能存在多个)

返回

返回一个嵌套dict ,第一层key是标的代码,value是标的对应的信息，第二层key是指标名，value是指标值

tick_size 最小报价单位: float

contract_multiplier 合约乘数 : float

trade_time:表示可交易时间段，以每次可交易时间变更作为一条记录。例如：AG2012.XSGE 的交易时间为2019-12-17至2020-12-15 的 '21:00~02:30', '09:00~10:15', '10:30~11:30', '13:30~15:00' 四个时间段。

示例

from jqdatasdk import *
infos = get_futures_info(["AG2012.XSGE",'A1609.XDCE' ])
print(infos)
>>>{'AG2012.XSGE': {'tick_size': 1.0,
'trade_time': [['2019-12-17', '2020-12-15', '21:00~02:30', '09:00~10:15', '10:30~11:30', '13:30~15:00']],
'contract_multiplier': 15.0},
'A1609.XDCE': {'tick_size': 1.0,
'trade_time': [['2015-03-16', '2015-05-08', '21:00~02:30', '09:00~10:15', '10:30~11:30', '13:30~15:00'],
['2015-05-09', '2016-09-14', '21:00~23:30', '09:00~10:15', '10:30~11:30', '13:30~15:00']],
'contract_multiplier': 10.0}}
