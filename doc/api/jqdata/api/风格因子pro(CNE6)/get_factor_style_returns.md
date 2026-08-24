获取风格因子回报率

get_factor_style_returns(factors=None, start_date=None,
end_date=None, count=None,
universe=None, industry='sw_l1')

参数

factors : 因子名称，单个因子（字符串）或一个因子列表。

支持风格因子如"size"

支持申万/聚宽一级行业如 "801010"和"HY001", 以及国家因子 "country", 注意为了避免混淆, 风格因子仅支持传递 style 和 style_pro 中的一类，也可以指定因子分类('style'或'style_pro') 表示分类下的所有风格因子

start_date : 开始日期，字符串或 datetime 对象

end_date : 结束日期，字符串或 datetime 对象，可以与 start_date 或 count 配合使用

count: 截止 end_date 之前交易日的数量（含 end_date 当日）

universe : 市场范围,默认为None代表全市场, 可选 : 'hs300': 沪深300 ; 'zz500': 中证500'; zz800': 中证800; 'zz1000':中证1000; 'zz2000':中证2000; 'zzqz':中证全指

industry : 行业选取, 默认为 'sw_l1',可选 : 'sw_l1':申万一级, 'jq_l1':聚宽一级; 为了避免混淆, factors 中的行业因子仅返回 industy 下的行业

注意:

当指定universe时，回归所用的风格因子有根据市场范围进行标准化等重新计算

返回

一个 DataFrame, index 是日期, columns为因子名, 值为因子暴露收益率

ps：股票收益的日期取T-1日的因子暴露及T日的股票收益计算暴露收益率 ,数据时间戳为T

示例：

#返回市净率因子、分红因子、盈利质量因子和申万行业中电器设备Ⅰ的风格暴露收益率
from jqdatasdk import *
df = get_factor_style_returns(['btop','divyild','earnqlty','801730'] , end_date='2022-09-01',count=2)
print(df)

btop   divyild  earnqlty    801730
2022-08-31 -0.001866 -0.000192  0.000838 -0.026796
2022-09-01  0.001239  0.001243 -0.000076 -0.001498

#返回各CNE6风格因子和行业的风格暴露收益率
factors=['btop', 'divyild', 'earnqlty', 'earnvar', 'earnyild', 'financial_leverage', 'invsqlty', 'liquidty', 'long_growth', 'ltrevrsl', 'market_beta', 'market_size', 'midcap', 'profit', 'relative_momentum', 'resvol']
industry=get_industries(name='sw_l1', date="2024-02-04").index.tolist()
factors+=industry
df = get_factor_style_returns(factors , end_date='2024-02-04',count=2)
print(df)
>>>             btop   divyild  earnqlty    ...       801150    801730    801040
2024-02-01 -0.000462 -0.000610 -0.000269    ...     0.003406  0.001192  0.001316
2024-02-02 -0.000113  0.000923 -0.000061    ...    -0.006946 -0.005291  0.005308

[2 rows x 47 columns]
