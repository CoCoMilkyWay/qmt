获取特异收益率(无法被风格因子解释的收益)

get_factor_specific_returns(security, start_date=None, end_date=None, count=None, category="style",universe=None, industry='sw_l1')

参数

security : 股票代码, 或者股票代码组成的list

start_date : 开始日期，字符串或 datetime 对象

end_date : 结束日期，字符串或 datetime 对象，可以与 start_date 或 count 配合使用

count: 截止 end_date 之前交易日的数量（含 end_date 当日）

category : 风格因子分类, 可选 'style' 和 'style_pro', 默认 'style'

universe : 市场范围,默认为None代表全市场, 可选 : 'hs300': 沪深300 ; 'zz500': 中证500'; zz800': 中证800; 'zz1000':中证1000; 'zz2000':中证2000; 'zzqz':中证全指

industry : 行业选取, 默认为 'sw_l1',可选 : 'sw_l1':申万一级, 'jq_l1':聚宽一级; 为了避免混淆, factors 中的行业因子仅返回 industy 下的行业

注意:

当指定universe时，回归所用的风格因子有根据市场范围进行标准化等重新计算

返回

个股被风格因子无法解释的那部分收益，即特质收益率

示例：

from jqdatasdk import *
df = get_factor_specific_returns(['000001.XSHE','600000.XSHG'],end_date='2022-09-01',count=5)
print(df)
>>>            000001.XSHE  600000.XSHG
2022-08-26     0.005917     0.004784
2022-08-29    -0.006386    -0.004693
2022-08-30    -0.001301    -0.004310
2022-08-31     0.004911    -0.012619
2022-09-01    -0.003611    -0.001511
