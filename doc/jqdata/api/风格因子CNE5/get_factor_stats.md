获取因子看板分位数历史收益率

get_factor_stats(factor_names=None, universe_type='hs300',
start_date=None, end_date=None, count=None,
skip_paused=False, commision_fee=0.0)

参数

factor_names :因子名称，单个因子（字符串）或一个因子列表，支持的因子见聚宽因子库

universe_type : 股票池包括以下五种( 默认为'hs300')：

        'hs300': 沪深300

        'zz500': 中证500

        'zz800': 中证800

        'zz1000': 中证1000

        'zzqz': 中证全指

start_date : 开始日期，字符串或 datetime 对象

end_date : 结束日期，字符串或 datetime 对象，可以与 start_date 或 count 配合使用

count: 截止 end_date 之前交易日的数量（含 end_date 当日）

skip_paused : 是否跳过停牌，bool，默认为False

commision_fee : 手续费，float，0.0/0.0008/0.0018, 默认为0.0

返回

因子的收益需要下一交易日才可得到 ,因此实际数据可获取的时间比date晚一个交易日(T日收盘后的因子收益需要T+1的收盘价才可得出，数据需要在T+2日凌晨3点计算之后才可得到)

一个 dict，其中 key 是因子名称，value 是一个 pandas.DataFrame

DataFrame 的 index 是日期，column 1,2,3,4,5是一分位、二分位、三分位、四分位、五分位累积收益

示例

from jqdatasdk import *
data = get_factor_stats(factor_names=['btop','divyild','earnqlty'] , end_date ='2022-09-20',count=5  )
print(data['btop'])

>>>                    1         2         3         4         5
2022-09-14  1.721280  1.923886  1.938821  1.850656  2.205259
2022-09-15  1.697609  1.880201  1.884546  1.790639  2.154095
2022-09-16  1.701172  1.882062  1.878233  1.787517  2.156027
2022-09-19  1.717246  1.893436  1.879293  1.769476  2.142427
2022-09-20  1.687232  1.869481  1.866279  1.774785  2.152991

</article>
</div>
