获取因子看板列表数据

get_factor_kanban_values(universe='hs300',bt_cycle='month_3',model='long_only',category=['quality','basics','emotion','growth','risk','pershare'],skip_paused=False,commision_slippage=0)

参数

universe：股票池

        'hs300': 沪深300

        'zz500': 中证500

        'zz800': 中证800

        'zz1000': 中证1000

        'zzqz': 中证全指

bt_cycle：测试周期

        'month_3'：近三个月

        'year_1'：近一年

        'year_3'：近三年

        year_10'：近十年

model：组合构建模型

        'long_only'：纯多头组合

        'long_short'：多空组合

category：分类，具体见聚宽因子库

        'quality': 质量类

        'basics': 基础类

        'emotion': 情绪类

         'growth': 成长类

        'risk': 风险类

        'pershare': 每股类

         'style': 风险因子 - 风格因子

        'technical': 技术类

        'momentum': 动量类

kip_paused: 过滤涨停及停牌股

        False: 否

        True: 是

commision_slippage: 手续费及滑点

        0: 无

        1: 3‱佣金+1‰印花税+无滑点

        2: 3‱佣金+1‰印花税+1‰滑点

返回

pandas.DataFrame 针对 [model - 组合构建模型]选择的不同，返回的结构有所差异

一、long_only - 纯多头组合

index：自然增长的数字，从0开始，无意义

column：

        date： 因子的收益需要下一交易日才可得到 ,因此实际数据可获取的时间比date晚一个交易日(T日收盘后的因子收益需要T+1的收盘价才可得出，数据需要在T+2日凌晨3点计算之后才可得到)

        universe: 股票池

        bt_cycle: 测试周期

        skip_paused: 过滤涨停及停牌股

        commision_slippage: 手续费及滑点

        category: 因子分类

        code：因子代码

        compound_return_1q：一分位数累积收益

        compound_return_5q：五分位数累积收益

         annualized_return_1q：一分位数年化收益率

        annualized_return_5q：五分位数年化收益率

        max_drawdown_1q：一分位数最大回撤

        max_drawdown_5q：五分位数最大回撤

        sharpe_1q：一分位数夏普比率

        sharpe_5q：五分位数夏普比率

        turnover_ratio_1q：一分位数换手率

        turnover_ratio_5q：五分位数换手率

        annual_return_bm：基准指数年化收益率

        ic_mean：IC均值

        ir：IR值

        good_ic：IC绝对值大于0.02的比率

二、long_short - 多空组合

index：自然增长的数字，从0开始，无意义

column：

         date：数据的更新日期，因子的收益需要下一交易日才可得到 ,因此实际数据的时间比date晚一天(T日收盘后的因子收益需要T+1的收盘价才可得出，数据需要在T+2日凌晨3点计算之后才可得到)

         universe: 股票池

         bt_cycle: 测试周期

         skip_paused: 过滤涨停及停牌股

         commision_slippage: 手续费及滑点

         category: 因子分类

         code：因子代码

         compound_return_ls：累积收益

         annualized_return_ls：年化收益率

         max_drawdown_ls：最大回撤

         sharpe_ls：夏普比率

         turnover_ratio_ls：换手率

         annual_return_bm：基准指数年化收益率

         ic_mean：IC均值

         ir：IR值

         good_ic：IC绝对值大于0.02的比率

示例

from jqdatasdk import *
df = get_factor_kanban_values(universe='hs300',bt_cycle='month_3',model='long_only',
category=['style_pro'],skip_paused=False,commision_slippage=0)
print(df.head(5))

date   category         code universe bt_cycle  skip_paused  \
0 2024-03-22  style_pro       resvol    hs300  month_3        False
1 2024-03-22  style_pro  market_beta    hs300  month_3        False
2 2024-03-22  style_pro       midcap    hs300  month_3        False
3 2024-03-22  style_pro       profit    hs300  month_3        False
4 2024-03-22  style_pro     liquidty    hs300  month_3        False

commision_slippage  compound_return_1q  compound_return_5q  \
0                   0            0.082971            0.095416
1                   0            0.132900            0.087422
2                   0            0.115523            0.012262
3                   0            0.096083            0.045657
4                   0            0.091171            0.074147

annual_return_1q  annual_return_5q  max_drawdown_1q  max_drawdown_5q  \
0          0.418498          0.491396         0.028479         0.201936
1          0.728554          0.444245         0.033119         0.197513
2          0.615251          0.054910         0.046911         0.192985
3          0.495380          0.216301         0.032075         0.100147
4          0.575279          0.451410         0.028270         0.157285

sharpe_1q  sharpe_5q  turnover_mean_1q  turnover_mean_5q  annual_return_bm  \
0   2.965449   1.225099          0.015205          0.016667          0.324823
1   4.462848   1.209714          0.011111          0.017836          0.324823
2   3.804401   0.048918          0.552632          0.017544          0.324823
3   3.318204   0.946735          0.000292          0.000000          0.324823
4   3.551314   1.142525          0.010069          0.004861          0.324823

ic_mean        ir   good_ic
0 -0.075242 -0.207315  0.982456
1 -0.067486 -0.166541  0.964912
2 -0.045928 -0.156631  0.982456
3 -0.043151 -0.203651  0.929825
4 -0.036663 -0.102687  0.958333
