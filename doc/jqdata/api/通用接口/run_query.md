查询数据库数据

试用账号历史范围：前15个月~前3个月 ; 正式账号历史范围：不限制

finance.run_query(query_object) #查询股票/期货/基金数据库
opt.run_query(query_object) #查询期权数据库
bond.run_query(query_object) #查询债券数据库
macro.run_query(query_object) #查询宏观数据库

run_query查询数据库数据，注意区分数据库，接收Query对象,query对象的教程见Query的简单教程

注意

单次返回最多5000条，如查询数据量超出请限制查询范围或者使用run_offset_query

查询时尽量根据id,日期,或者标的代码(一般地这些字段都会被设置为索引)进行filter,查询如果命中索引返回就会较快

示例

# 查询finance库，万科 AH 股价格的前10条数据
from jqdatasdk import query,finance
# 或from jqdatasdk import *
q=query(finance.STK_AH_PRICE_COMP
).filter(
finance.STK_AH_PRICE_COMP.a_code=='000002.XSHE'
).order_by(
finance.STK_AH_PRICE_COMP.day
).limit(10)
df=finance.run_query(q)

# 查询opt库，当前可交易的50ETF期权合约信息
from jqdatasdk import query,opt
# 或from jqdatasdk import *
from datetime import datetime
opt.run_query(query(opt.OPT_CONTRACT_INFO).filter((opt.OPT_CONTRACT_INFO.underlying_symbol == "510050.XSHG") & (opt.OPT_CONTRACT_INFO.last_trade_date > datetime.now())))
