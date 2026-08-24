宏观数据

部分数据已经停止更新，未来全部宏观数据可能将不再更新或提供。

from jqdatasdk import macro
macro.run_query(query(macro.table_name).filter(macro.table_name.indicator==value).limit(n))

macro.run_offset_query(query_object) #分页查询宏观数据库

描述

宏观数据的调用方式

返回结果

返回一个 dataframe，每一行对应数据表中的一条数据， 列索引是您所查询的字段名称

注意

为了防止返回数据量过大, 我们每次最多返回5000行

不能进行连表查询，即同时查询多张表的数据

query函数的使用技巧

query函数的更多用法详见：query简易教程

分页获取注意事项

因为run_query 有单次调取最大返回5000条的限制，此函数利用MySql的offset方法循环获取数据，便于提取超过5000条的数据集

因为随着offset值的增大，查询性能是递减的 ,因此此方法仍然设置了查询上限, 最多返回20万条数据，如查询超过此上限返回数据可能不完整，请注意控制查询范围，可利用数据的日期,标的代码等字段限制查询范围, 分批查询

因为该方法是通过指定limit和offset来实现分页查询的，因此用户自己给Query对象中传递的limit及offset参数将不生效

查询时尽量根据id,日期,或者标的代码(一般地这些字段都会被设置为索引)进行filter,查询如果命中索引返回就会较快

宏观数据

query(macro.table_name)表示从macro.table_name这张表中查询宏观经济数据，table_name是所要查询的宏观经济分类表，如macro.MAC_INDUSTRY_ESTATE_INVEST_MONTH代表房地产开发投资情况表，更多表名点击[宏观经济数据分类](https://www.joinquant.com/help/api/help?name=macroData#%E5%86%9C%E4%B8%9A)查看。还可以指定所要查询的字段名，格式如下：query(库名.表名.字段名1，库名.表名.字段名2），多个字段用逗号分隔进行提取；query函数的更多用法详见：sqlalchemy.orm.query.Query对象。；query函数的更多用法详见：[query简易教程]

macro.table_name.column_name ：收录了宏观数据数据，表结构和字段信息如下：

filter(macro.table_name.indicator==value)：指定限制条件，macro.table_name.indicator是具体表的字段名称，如macro.MAC_INDUSTRY_ESTATE_INVEST_MONTH.stat_quarter==12代表取出房地产开发投资情况中统计季度等于第四季度的情况。

limit(n)：限制返回的数据条数，n指定返回条数。

宏观数据表类汇总

分类	表名

农业	分地区农林牧渔业总产值表(季度累计)：MAC_INDUSTRY_AREA_AGR_OUTPUT_VALUE_QUARTER
分地区农林牧渔业总产值表(年度)：MAC_INDUSTRY_AREA_AGR_OUTPUT_VALUE_YEAR
全国农产品生产价格指数表(季度)：MAC_INDUSTRY_AGR_PRODUCT_IDX_QUARTER

国内贸易	社会消费品销售总额（月度）：MAC_SALE_RETAIL_MONTH
限额以上零售分类表（月度）： MAC_SALE_SCALE_RETAIL_MONTH
分地区消费品零售总额（年度）：MAC_AREA_RETAIL_SALE
亿元以上商品交易市场基本情况（年度）：MAC_SALE_MARKET
分地区亿元以上商品交易市场基本情况（年度）：MAC_AREA_SALE_MARKET

就业与工资	分地区城镇登记失业率（年度）：MAC_AREA_UNEMPLOY
就业情况基本表(年度)：MAC_EMPLOY_YEAR
分地区城镇单位就业人员情况表(年度)：MAC_AREA_WAGEIDX_YEAR
分地区分行业城镇单位就业人员工资情况表(年度)：MAC_AREA_INDUSTRY_WAGE_YEAR
分行业城镇单位就业人员工资情况表(年度)：MAC_INDUSTRY_WAGE_YEAR
分地区按注册类型分城镇单位就业人员工资情况表(年度)：MAC_AREA_REGISTERED_WAGE_YEAR
分地区按行业分城镇单位就业人员情况表（年度）：MAC_AREA_INDUSTRY_EMPLOY_YEAR

资源环境	各地区森林资源情况表（年度）：MAC_RESOURCES_AREA_FOREST
生态环境情况信息表（年度）：MAC_RESOURCES_ECOLOGICAL_ENVIRONMENT
水资源情况表（年度）：MAC_RESOURCES_AREA_WATER_RESOURCES
全国水资源量年度信息表（年度）：MAC_RESOURCES_WATER_RESOURCES_YEAR
各地区供水用水情况表（年度）：MAC_RESOURCES_AREA_WATER_SUPPLY_USE
供水用水情况表（年度）：MAC_RESOURCES_WATER_SUPPLY_USE_YEAR
水环境情况信息表（年度）：MAC_RESOURCES_WATER_ENVIRONMENT
各地区废气排放及处理情况表（年度）：MAC_RESOURCES_AREA_WASTE_GAS_EMISSION
自然灾害情况信息表（年度）：MAC_RESOURCES_NATURAL_DISASTER
环境污染治理投资情况信息表（年度）：MAC_RESOURCES_ENVIRONMENT_TREAT_INVEST

房地产行业	房地产开发投资情况表(月度累计)：MAC_INDUSTRY_ESTATE_INVEST_MONTH
分地区房地产开发投资情况表(月度累计)：MAC_INDUSTRY_AREA_ESTATE_INVEST_MONTH
房地产开发投资资金来源情况表(月度累计)：MAC_INDUSTRY_ESTATE_FUND_SOURCE_MONTH
各地区房地产开发规模与开、竣工面积增长情况表(月度累计)：MAC_INDUSTRY_AREA_ESTATE_BUILD_MONTH
70个大中城市房屋销售价格指数(月度)：MAC_INDUSTRY_ESTATE_70CITY_INDEX_MONTH

金融业	人民币外汇牌价(日级)：MAC_RMB_EXCHANGE_RATE
银行间拆借利率表（日级）：MAC_LEND_RATE
金融机构人民币信贷资金平衡表（年度）：MAC_CREDIT_BALANCE_YEAR
货币供应量(月度)：MAC_MONEY_SUPPLY_MONTH
货币供应量(年度)：MAC_MONEY_SUPPLY_YEAR
货币当局资产负债表（年度）：MAC_CURRENCY_STATE_YEAR
其他存款性公司资产负债表（年度）：MAC_OTHER_DEPOSIT
社会融资规模及构成（年度）：MAC_SOCIAL_SCALE_FINANCE
证券市场基本情况（年度）：MAC_STK_MARKET
黄金和外汇储备（月度）：MAC_GOLD_FOREIGN_RESERVE
股票发行量和筹资额（年度）：MAC_STK_ISSUE
股票市场统计表（年度）：MAC_STK_TRADE

财政政策	国家财政收支总额及增长速度表（年度）：MAC_FISCAL_TOTAL_YEAR
中央财政与地方财政收支及比重表（年度）：MAC_FISCAL_BALANCE_YEAR
中央和地方财政主要收入项目情况表(年度)：MAC_FISCAL_CENTRAL_REVENUE_YEAR
中央和地方财政主要支出项目情况表(年度)：MAC_FISCAL_CENTRAL_EXPENSE_YEAR
各项税收表（年度）：MAC_FISCAL_TAX_YEAR
预算外资金分项目收支表（年度）：MAC_FISCAL_EXTRA_REVENUE_EXPENSE_YEAR
中央财政与地方财政预算外收支表（年度）：MAC_FISCAL_EXTRAL_BALANCE_YEAR
外债余额表（年度）：MAC_FISCAL_EXTERNAL_DEBT_YEAR
外债风险指标表（年度）：MAC_FISCAL_RISK_INDICATOR_YEAR
各地区财政收入表（年度）：MAC_AREA_FISCAL_REVENUE_YEAR
各地区财政支出表（年度）：MAC_AREA_FISCAL_EXPENSE_YEAR

固定资产投资	固定资产投资情况（月度）：MAC_FIXED_INVESTMENT
分地区固定资产投资情况（月度）：MAC_AREA_FIXED_INVESTMENT
分行业固定资产投资情况（月度）：MAC_INDUSTRY_FIXED_INVEST
按注册类型登记分固定资产投资（月度）：MAC_REGISTERED_FIXED_INVESTMENT
固定资产投资情况表(年度)：MAC_FIXED_INVESTMENT_YEAR

对外贸易	货物进出口总额表（年度）：MAC_TRADE_VALUE_YEAR
海关进出口货物分类金额表（年度）：MAC_TRADE_VALUE_SITC_YEAR
地区按经营单位所在地分货物进出口总额表（年度）：MAC_TRADE_VALUE_LOCATION_YEAR
各地区按境内目的地和货源地分货物进出口总额表（年度）：MAC_TRADE_VALUE_DESTINATION_YEAR
利用外资情况表（月度）：MAC_FOREIGN_CAPITAL_MONTH
利用外资概况表（年度）：MAC_FOREIGN_CAPITAL_YEAR
按行业分对外直接投资情况表（年度）：MAC_INDUSTRY_OFDI_YEAR
分国别对外外直接投资情况表（年度）：MAC_NATION_OFDI
分地区外商投资企业年底注册登记情况表（年度）：MAC_AREA_FOREIGN_REGISTER
按行业分外商投资企业年底注册登记情况表（年度）：MAC_INDUSTRY_FOREIGN_REGISTER
对外经济合作表（年度）：MAC_FOREIGN_COOPERATE_YEAR
按国别对外经济合作表（年度）：MAC_NATION_COOPERATE_YEAR

景气指数	宏观经济景气指数（月度）：MAC_ECONOMIC_BOOM_IDX
消费者景气指数（月度）：MAC_CONSUMER_BOOM_IDX
宏观经济景气预警指数（月度）：MAC_BOOM_WARNING_IDX
企业景气及企业家信心指数（季度）：MAC_ENTERPRISE_BOOM_CONFIDENCE_IDX
制造业采购经理指数（月度）：MAC_MANUFACTURING_PMI
非制造业采购经理指数（月度）：MAC_NONMANUFACTURING_PMI
分地区居民消费价格指数（月度）：MAC_AREA_CPI_MONTH
全国居民消费价格指数（月度）：MAC_CPI_MONTH

工业	全国工业增长速度（月度）：MAC_INDUSTRY_GROWTH
全国工业分行业增长速度（月度）：MAC_INDUSTRY_CATEGORY_GROWTH
全国工业企业主要经济指标（月度）：MAC_INDUSTRY_INDICATOR

保险业	全国各地区保险业务统计表(年度)：MAC_INSURANCE_AREA_YEAR
保险公司保费金额表(年度)：MAC_INSURANCE_PREMIUM_YEAR
保险公司赔款及给付表(年度)：MAC_INSURANCE_PAYMENT_YEAR
保险公司资产情况（年度）：MAC_INSURANCE_ASSETS_YEAR
保险公司原保费收入和赔付支出情况（年度）：MAC_INSURANCE_REVENUE_EXPENSE_YEAR

国民经济	全国各地区的行政划分（年度）：MAC_AREA_DIV
分地区国内生产总值表(季度)：MAC_AREA_GDP_QUARTER
分地区国内生产总值表(年度)：MAC_AREA_GDP_YEAR
分地区国内生产总值指数表(上年=100，年度)：MAC_AREA_GDP_YEAR_IDX
分地区国内生产总值指数表（年度）：MAC_AREA_GDP_YEAR_IDX_1978
分地区支出法国内生产总值表(年度)：MAC_AREA_GDP_EXPEND_YEAR
分地区收入法国内生产总值表(年度)：MAC_AREA_GDP_INCOME_YEAR
国家统计局发布经济信息的日程表（年度）：MAC_STATS_REPORT_CALENDAR

人民生活	各地区居民消费水平表(年度)：MAC_AREA_CONSUME_YEAR
居民人均收入支出表(年度)：MAC_REVENUE_EXPENSE_YEAR
城乡居民家庭人均收入及恩格尔系数(年度)：MAC_ENGEL_COEFFICIENT_YEAR
城乡居民人民币储蓄存款表(年度)：MAC_RESIDENT_SAVING_DEPOSIT_YEAR
分地区城镇居民家庭平均每人全年收入来源表(年度)：MAC_AREA_URBAN_INCOME_YEAR
分地区城镇及农村居民家庭平均每人全年消费性支出表(年度)：MAC_AREA_URBAN_RURAL_EXPENSE_YEAR
农村居民家庭平均每人纯收入(年度)：MAC_RURAL_NET_INCOME_YEAR
各地区按来源分农村居民家庭人均纯收入(年度)：MAC_AREA_RURAL_NET_INCOME_YEAR
分地区农村居民家庭住房情况表(年度)：MAC_AREA_RURAL_HOUSE_YEAR

人口信息	人口基本情况表(年度)：：MAC_POPULATION_YEAR
各地区人口平均预期寿命表（年度）：MAC_LIFE_EXPECT
按年龄和性别分人口数表（年度）：MAC_POPULATION_AGE
各地区户数、人口数、性别比和户规模表（年度）：MAC_AREA_HOUSEHOLD_SIZE
户口登记状况（年度）：MAC_AREA_HOUSEHOLD_REGISTER
各地区人口年龄结构和抚养比例表（年度）：MAC_AREA_POP_DEPENDENCY
各地区按性别和婚姻状况分的人口表（年度）：MAC_AREA_POP_MARITAL
各地区按性别和受教育程度分人口情况表（年度）：MAC_AREA_POP_EDUCATION
各地区按性别分的15岁及以上文盲人口表（年度）：MAC_AREA_POP_ILLITERATE
各地区按家庭户规模分的户数表（年度）：MAC_AREA_FAMILY_HOUSEHOLD
育龄妇女分年龄生育状况表（年度）：MAC_POP_FERTILITY_RATE
人口年龄结构和抚养比例（年度）：MAC_POPULATION_DEPENDENCY

示例：

批量获取农林牧渔业总产值表(季度累计)数据

q = query(macro.MAC_INDUSTRY_AREA_AGR_OUTPUT_VALUE_QUARTER
)
df = macro.run_offset_query(q)
print(df)

查询分地区农林牧渔业总产值表(季度累计) 的前4条数据

# 查询分地区农林牧渔业总产值表(季度累计) 的前4条数据
q = query(macro.MAC_INDUSTRY_AREA_AGR_OUTPUT_VALUE_QUARTER
).limit(4)
df = macro.run_query(q)
print(df)

id stat_quarter area_code area_name      total    farming  forestry  \
0   1      2015-06    350000       福建省  1240.1000   430.4000  101.2000
1   2      2014-09    350000       福建省  2027.9000   830.8000  155.2000
2   3      2015-03    350000       福建省   538.2000   148.2000   36.4000
3   4      2014-12    350000       福建省  3522.3053  1529.5705  323.2506

animal_husbandry    fishery
0          237.7000   417.6000
1          368.4000   591.2000
2          127.6000   197.9000
3          522.8944  1025.1946

# 查询2014年的分地区农林牧渔业总产值表(年度)
q = query(macro.MAC_INDUSTRY_AREA_AGR_OUTPUT_VALUE_YEAR
).filter(macro.MAC_INDUSTRY_AREA_AGR_OUTPUT_VALUE_YEAR.stat_year=='2014')
df = macro.run_query(q)
print(df[:4])

id stat_year area_code area_name      total    farming  forestry  \
0  104      2014    110000       北京市   420.0672   155.1015   90.6852
1  214      2014    120000       天津市   367.7488   182.2270    3.2204
2  264      2014    130000       河北省  5373.7637  2893.2898  118.4668
3  164      2014    140000       山西省  1440.5971   872.5551   92.4834

animal_husbandry   fishery  total_idx  farming_idx  forestry_idx  \
0          152.6590   13.2024    99.9874      91.3496      119.5023
1          102.7424   68.8678   102.9743     103.3167      103.5446
2         1895.9047  175.8537   104.0124     103.1439      108.9083
3          384.0043    7.9749   104.0256     103.2592      102.3852

animal_husbandry_idx  fishery_idx
0               99.2970     105.1272
1              102.8987     102.1849
2              105.0522     103.2209
3              105.6783     109.1235
