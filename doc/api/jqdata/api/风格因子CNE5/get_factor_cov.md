获取风格因子协方差矩阵

get_factor_cov(start_date, end_date, factors=None, columns=None, industry='sw_l1', universe=None):

参数

start_date : 数据开始日期

end_date : 数据结束日期

factors : 风格因子名, 默认所有风格因子, 可以自己加上行业名称, 当行业名称和 industry 不匹配时不返回对应的行业, 也可以传递国家因子名 "contry", 也可以只指定因子分类('style'或'style_pro') 表示分类下的所有风格因子

columns : 列名, 默认返回包含 industry 下的所有行业(默认不含国家因子 "contry", 可指定返回)

industry : 行业, 目前只支持 sw_l1/jq_l1

universe : 市场, 目前只支持全市场 None

返回

列名 : date,factor,column(1),column(2),...,column(n)

eg : date,factor_name,size,beta,..... HY001

'2023-11-16',size,0.001,0.2,.....

'2023-11-16',beta,0.23,0.0012,....

示例：

from jqdatasdk import *
df = get_factor_cov("2024-03-01",'2024-03-10')
print(df.head(5))

>>>     date              factors    ...       801970    801980
0 2024-03-01                 beta    ...    -0.010495 -0.050480
1 2024-03-01  book_to_price_ratio    ...    -0.001064  0.001780
2 2024-03-01       earnings_yield    ...    -0.001097  0.034653
3 2024-03-01               growth    ...     0.000614  0.005436
4 2024-03-01             leverage    ...     0.001298 -0.004435

[5 rows x 43 columns]
