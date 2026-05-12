stk_get_finance_forecast - 查询公司业绩预告
获取股票所属上市公司的业绩预告数据

gm SDK 3.0.172 版本新增

函数原型：

stk_get_finance_forecast(symbols, rpt_type=None, date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	必填，可输入多个，使用时参考symbol. 采用 str 格式时，多个标的代码必须用英文逗号分割（逗号中间不能有空格），如：'SHSE.600008,SZSE.000002'; 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
date	str	查询日期	N	None	查询时间，时间类型为最新公告日期，%Y-%m-%d 格式，默认None表示最新时间
rpt_type	str	预测报表类型	N	None	按报告期查询可指定以下报表类型：1-一季度报，6-中报，9-前三季报，12-年报，默认None为不限
df	bool	返回格式	N	False	是否返回dataframe格式， 默认False返回字典格式，返回 list[dict]， 列表每项的dict的key值为参数指定的 fields
返回值：df=True, 返回dataframe; df=False, 返回list[dict]

字段名	类型	中文名称	说明
symbol	str	股票代码	格式exchange.sec_id（SHSE.600000, SZSE.000001）
pub_date	str	最新公告日期	
begin_date	str	预测起始日	
end_date	str	预测截止日	
rpt_type	str	预测报表类型	
fcst_type	str	业绩预告类型	001001 预增，001002 略增，001003 略减，001004 预减，001005 续盈，001006 首亏，002 不确定，003001 扭亏，003002 续亏，003003 减亏，003004 增亏
fcst_field	str	预测财务指标	001 主营业务收入，002 净利润 net_prof，003 每股收益 eps_basic，004 归属于上市公司股东的净利润 net_prof_pcom，005 扣除非经常性损益后的净利润 net_prof_pcom_cut，006 营业收入 inc_oper，007 非经常性损益 nr_prof_loss，008 扣除后营业收入
fcst_amount_max	float	预测金额元(上限)	单位：元
fcst_amount_min	float	预测金额元(下限)	单位：元
amount_ly	float	上年同期元	单位：元
increase_pct_max	float	增长幅度(上限)	单位：%
increase_pct_min	float	增长幅度(下限)	单位：%
fcst_content	str	财务指标预告内容	
ann_fcst_amount_unit	str	公告预测金融单位	
ann_fcst_amount_max	float	公告原始预测金额(上限)	单位：公告预测金融单位
ann_fcst_amount_min	float	公告原始预测金额(下限)	单位：公告预测金融单位
ann_amount_ly	float	上年原始同期	单位：公告预测金融单位
ann_increase_max	float	公告增长金额(上限)	单位：公告预测金融单位
ann_increase_min	float	公告增长金额(下限)	单位：公告预测金融单位
is_change	bool	是否变脸	0-否 1-是
change_reason	str	业绩变动原因说明	
示例：

stk_get_finance_forecast(symbols='SHSE.600000,SZSE.000001', rpt_type=None, date=None, df=False)
 
        复制成功
    
输出：

[{'symbol': 'SHSE.600000', 'pub_date': '2008-10-30T00:00:00+08:00', 'begin_date': '2008-01-01T00:00:00+08:00', 'end_date': '2008-12-31T00:00:00+08:00', 'rpt_type': '12', 'fcst_type': '预增', 'fcst_field': '归属于上市公司股东的净利润', 'fcst_content': '预计与上年同期相比发生大幅度变动。', 'fcst_amount_max': 0.0, 'fcst_amount_min': 0.0, 'amount_ly': 0.0, 'increase_pct_max': 0.0, 'increase_pct_min': 0.0, 'ann_fcst_amount_unit': '', 'ann_fcst_amount_max': 0.0, 'ann_fcst_amount_min': 0.0, 'ann_amount_ly': 0.0, 'ann_increase_max': 0.0, 'ann_increase_min': 0.0, 'is_change': False, 'change_reason': ''},
 {'symbol': 'SZSE.000001', 'pub_date': '2016-01-21T00:00:00+08:00', 'begin_date': '2015-01-01T00:00:00+08:00', 'end_date': '2015-12-31T00:00:00+08:00', 'rpt_type': '12', 'fcst_type': '略增', 'fcst_field': '归属于上市公司股东的净利润', 'fcst_amount_max': 22772250000.0, 'fcst_amount_min': 20792060000.0, 'increase_pct_max': 15.0, 'increase_pct_min': 5.0, 'fcst_content': '预计2015年1-12月归属于上市公司股东的净利润:2,079,206-2,277,225万元,同比上年上升:5%-15%', 'change_reason': '业绩增长的主要原因是资产规模的稳定增长、 息差改善以及成本有效控制。', 'amount_ly': 0.0, 'ann_fcst_amount_unit': '', 'ann_fcst_amount_max': 0.0, 'ann_fcst_amount_min': 0.0, 'ann_amount_ly': 0.0, 'ann_increase_max': 0.0, 'ann_increase_min': 0.0, 'is_change': False},
 {'symbol': 'SZSE.000001', 'pub_date': '2016-01-21T00:00:00+08:00', 'begin_date': '2015-01-01T00:00:00+08:00', 'end_date': '2015-12-31T00:00:00+08:00', 'rpt_type': '12', 'fcst_type': '略增', 'fcst_field': '每股收益', 'fcst_amount_max': 1.62, 'fcst_amount_min': 1.48, 'fcst_content': '预计2015年1-12月每股收益:1.48-1.62元', 'change_reason': '业绩增长的主要原因是资产规模的稳定增长、 息差改善以及成本有效控制。', 'amount_ly': 0.0, 'increase_pct_max': 0.0, 'increase_pct_min': 0.0, 'ann_fcst_amount_unit': '', 'ann_fcst_amount_max': 0.0, 'ann_fcst_amount_min': 0.0, 'ann_amount_ly': 0.0, 'ann_increase_max': 0.0, 'ann_increase_min': 0.0, 'is_change': False}]

 
        复制成功
    
注意：

1. 为避免未来数据，指定查询日期date后，返回公告日期小于等于查询日期下的最新报告期数据。

