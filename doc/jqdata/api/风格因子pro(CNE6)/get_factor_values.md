风格因子

# 导入函数库
from jqdatasdk import *
# 取值函数
get_factor_values(securities, factors, start_date, end_date, count)

风格因子数据处理说明

对描述因子和风格因子的数据分别进行正规化的处理，步骤如下：

对描述因子分别进行去极值和标准化

去极值为将2.5倍标准差之外的值，赋值成2.5倍标准差的边界值

标准化为市值加权标准化

x=(x- mean(x))/(std(x))

其中，均值的计算使用股票的市值加权，标准差为正常标准差。

对描述因子按照权重加权求和

按照公式给出的权重对描述因子加权求和。如果某个因子的值为nan，则对不为nan的因子加权求和，同时权重重新归一化；

如果所有因子都为nan，则结果为nan。

对风格因子市值加权标准化

缺失值填充

按照聚宽一级行业分行业，以不缺失的股票因子值相对于市值的对数进行回归，对缺失值进行填充

对风格因子去极值，去极值方法同上面去极值描述

风格因子简介

因子 code	因子名称	简介

size	市值	捕捉大盘股和小盘股之间的收益差异

beta	贝塔	表征股票相对于市场的波动敏感度

momentum	动量	描述了过去两年里相对强势的股票与弱势股票之间的差异

residual_volatility	残差波动率	解释了剥离了市场风险后的波动率高低产生的收益率差异

non_linear_size	非线性市值	描述了无法由规模因子解释的但与规模有关的收益差异，通常代表中盘股

book_to_price_ratio	账面市值比	描述了股票估值高低不同而产生的收益差异, 即价值因子

liquidity	流动性	解释了由股票相对的交易活跃度不同而产生的收益率差异

earnings_yield	盈利能力	描述了由盈利收益导致的收益差异

growth	成长	描述了对销售或盈利增长预期不同而产生的收益差异

leverage	杠杆	描述了高杠杆股票与低杠杆股票之间的收益差异

除了上面的风格因子，在计算风格因子过程中的描述因子daily_standard_deviation、cumulative_range等也可以通过get_factor_values、get_all_factors以及get_factor_kanban_values获取；描述因子是原始值，没有进行数据处理。

风格因子-次级因子简介

风格因子-次级因子 code	因子名称	简介

natural_log_of_market_cap	对数总市值	公司的总市值的自然对数

raw_beta	RAW	CAPM 模型中的β，过去252个交易日股票的收益与市场收益（全A股票收益按流通市值加权）进行时间序列指数加权回归后的斜率系数。指数加权的半衰期为63个交易日

relative_strength	相对强弱	相对强弱 relative_strength：滞后21个交易日的过去504个交易日股票超额对数收益率的指数加权之和。其中指数权重半衰期为126个交易日。停牌股票收益率为0，上市之前收益率为nan

daily_standard_deviation	日收益率标准差	日标准差，过去252日的超额收益的指数加权标准差，以42个交易日为半衰期

cumulative_range	收益离差	过去12个月中月收益率（以21个交易日为一个月）的最大值和最小值之间的差异。股票需上市需超过6个月，否则结果为nan

historical_sigma	残差历史波动率	计算beta时的回归残差项的过去252个交易日的标准差。股票上市需超过21个交易日，否则结果为nan

cube_of_size	市值立方因子	市值立方因子 cube_of_size：首先对标准化后的市值因子size暴露值求立方，将得到的结果与市值进行加权回归的正交化处理

book_to_price_ratio	市净率因子	最新一季财报的账面价值与当前市值的比值（pb_ratio的倒数）。其中小于0的值设置为nan

share_turnover_monthly	月换手率	股票一个月换手率，过去21日的股票换手率之和的对数。股票需上市超过1个月，否则结果为nan

average_share_turnover_quarterly	季度平均平均月换手率	股票过去3个月平均换手率，计算过去3个月的平均share_turnover_monthly，并取对数。股票需上市超过3个月，否则结果为nan

average_share_turnover_annual	年度平均月换手率	股票过去12个月平均换手率，计算过去12个月的平均share_turnover_monthly，并取对数。股票需上市超过12个月，否则结果为nan

predicted_earnings_to_price_ratio	预期市盈率	用未来12个月的净利预测值除以当前市值

cash_earnings_to_price_ratio	现金流量市值比	过去12个月的净经营现金流除以当前股票市值

earnings_to_price_ratio	利润市值比	过去12个月的归母净利润除以当前股票市值

long_term_predicted_earnings_growth	预期长期盈利增长率	未来三年净利润分析师一致预期相对于净利润(不含少数股东损益)最新年报值的平均增长率。一个分析师预测的股票值为nan

short_term_predicted_earnings_growth	预期短期盈利增长率	未来一年净利润分析师一致预期相对于净利润(不含少数股东损益)最新年报值的平均增长率，只有一个分析师预测的股票值为nan

earnings_growth	5年盈利增长率	盈利增长率，过去5年的基本每股收益（basic_eps）关于[0,1,2,3,4]回归的斜率系数，然后再除以过去 5 年基本每股收益的均值的绝对值

sales_growth	5年营业收入增长率	营收增长率，过去 5 年每股营业收入关于[0,1,2,3,4]回归的斜率系数，然后再除以过去 5 年每股营业收入的均值的绝对值。对于保险行业的股票，使用“已赚保费”代替“销售收入”计算每股营业收入，对于银行业的股票，sales_growth为nan

market_leverage	市场杠杆	(当前的普通股市值+优先股账面价值+长期债务账面价值)/当前的普通股市值

debt_to_assets	总负债的账面价值/总资产的账面价值	总负债的账面价值/总资产的账面价值

book_leverage	账面杠杆	(普通股账面价値+优先股账面价值+长期债务账面价值)/普通股账面价值。账面杠杆需在0到100之间，否则结果为nan

风格因子及次级因子计算说明

市值因子 size

定义：1•natural_log_of_market_cap

解释：对数市值 natural_log_of_market_cap：公司的总市值的自然对数。

贝塔因子 beta

定义：1•raw_beta

解释：raw_beta：CAPM 模型中的β，过去252个交易日股票的收益与市场收益（全A股票收益按流通市值加权）进行时间序列指数加权回归后的斜率系数。指数加权的半衰期为63个交易日。

停牌股票收益率为0，股票上市需超过21个交易日，否则beta为nan。

动量因子 momentum

定义：1•relative_strength

解释：相对强弱 relative_strength：滞后21个交易日的过去504个交易日股票超额对数收益率的指数加权之和。其中指数权重半衰期为126个交易日。停牌股票收益率为0，上市之前收益率为nan。

残差波动率因子 residual_volatility

定义：0.74•daily_standard_deviation + 0.16•cumulative_range + 0.10•historical_sigma

解释：日收益率标准差 daily_standard_deviation：日标准差，过去252日的超额收益的指数加权标准差，以42个交易日为半衰期。

收益离差 cumulative_range：过去12个月中月收益率（以21个交易日为一个月）的最大值和最小值之间的差异。股票需上市需超过6个月，否则结果为nan。

残差历史波动率 historical_sigma：计算beta时的回归残差项的过去252个交易日的标准差。股票上市需超过21个交易日，否则结果为nan。

用 daily_standard_deviation、cumulative_range、historical_sigma 加权求和得到的 residual_volatility，之后 关于 beta 和 size 因子做正交化以消除共线性。

非线性市值因子 non_linear_size

定义：1•cube_of_size

解释：市值立方因子 cube_of_size：首先对标准化后的市值因子size暴露值求立方，将得到的结果与市值进行加权回归的正交化处理。

账面市值比因子 book_to_price_ratio

定义：book_to_price_ratio

解释：最新一季财报的账面价值与当前市值的比值（pb_ratio的倒数）。其中小于0的值设置为nan。

流动性因子 liquidity

定义：0.35•share_turnover_monthly + 0.35•average_share_turnover_quarterly + 0.3•average_share_turnover_annual

解释：月换手率 share_turnover_monthly：股票一个月换手率，过去21日的股票换手率之和的对数。股票需上市超过1个月，否则结果为nan。

季度平均平均月换手率 average_share_turnover_quarterly：过去3个月平均换手率，计算过去3个月的平均share_turnover_monthly，并取对数。股票需上市超过3个月，否则结果为nan。

年度平均月换手率 average_share_turnover_annual：过去12个月平均换手率，计算过去12个月的平均share_turnover_monthly，并取对数。股票需上市超过12个月，否则结果为nan。

用 share_turnover_monthly、average_share_turnover_quarterly、average_share_turnover_annual 加权求和得到的 liquidity 关于对数市值做正交化以消除共线性。

盈利能力因子 earnings_yield

定义:0.68•predicted_earnings_to_price_ratio + 0.21•cash_earnings_to_price_ratio + 0.11•earnings_to_price_ratio

解释：预期利润市值比 predicted_earnings_to_price_ratio：用未来12个月的净利预测值除以当前市值。

现金流量市值比 cash_earnings_to_price_ratio：过去12个月的净经营现金流除以当前股票市值。

利润市值比 earnings_to_price_ratio：过去12个月的归母净利润除以当前股票市值。

成长因子 growth

定义：0.18•long_term_predicted_earnings_growth + 0.11•short_term_predicted_earnings_growth + 0.24•earnings_growth + 0.47•sales_growth

解释：预期长期盈利增长率 long_term_predicted_earnings_growth：未来三年净利润分析师一致预期相对于净利润(不含少数股东损益)最新年报值的平均增长率。一个分析师预测的股票值为nan。

预期短期盈利增长率 short_term_predicted_earnings_growth：未来一年净利润分析师一致预期相对于净利润(不含少数股东损益)最新年报值的平均增长率，只有一个分析师预测的股票值为nan。

5年盈利增长率 earnings_growth：盈利增长率，过去5年的基本每股收益（basic_eps）关于[0,1,2,3,4]回归的斜率系数，然后再除以过去 5 年基本每股收益的均值的绝对值。

5年营业收入增长率 sales_growth：营收增长率，过去 5 年每股营业收入关于[0,1,2,3,4]回归的斜率系数，然后再除以过去 5 年每股营业收入的均值的绝对值。对于保险行业的股票，使用“已赚保费”代替“销售收入”计算每股营业收入，对于银行业的股票，sales_growth为nan。

earnings_growth和sales_growth至少需要有4年的财务数据，否则为nan。

杠杆因子 leverage

定义：0.38•market_leverage + 0.35•debt_to_assets + 0.27•book_leverage

解释：市场杠杆 market_leverage：(当前的普通股市值+优先股账面价值+长期债务账面价值)/当前的普通股市值。

资产负债比 debt_to_assets：总负债的账面价值/总资产的账面价值。

账面杠杆 book_leverage：(普通股账面价値+优先股账面价值+长期债务账面价值)/普通股账面价值。账面杠杆需在0到100之间，否则结果为nan。

风格因子数据处理说明

对描述因子和风格因子的数据分别进行正规化的处理，步骤如下：

对描述因子分别进行去极值和标准化

去极值为将2.5倍标准差之外的值，赋值成2.5倍标准差的边界值

标准化为市值加权标准化

x=(x- mean(x))/(std(x))

其中，均值的计算使用股票的市值加权，标准差为正常标准差。

对描述因子按照权重加权求和

按照公式给出的权重对描述因子加权求和。如果某个因子的值为nan，则对不为nan的因子加权求和，同时权重重新归一化

对风格因子市值加权标准化

缺失值填充

按照聚宽一级行业分行业，以不缺失的股票因子值相对于市值的对数进行回归，对缺失值进行填充

对风格因子去极值，去极值方法同上面去极值描述

from jqdatasdk import get_factor_values
factor_data = get_factor_values(securities=['000001.XSHE'], factors=['size','beta','momentum'],
start_date='2022-01-01', end_date='2022-01-10')
# 查看因子值
print(factor_data['size'])
