fnd_get_dividend - 查询基金分红信息
查询指定基金在一段时间内的分红信息

函数原型：

fnd_get_dividend(fund, start_date, end_date)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
fund	str	基金代码	Y	无	必填，只能输入一个基金的symbol，如：'SZSE.159919'
start_date	str	开始时间	Y	无	必填，开始时间日期（场内除息日），%Y-%m-%d 格式
end_date	str	结束时间	Y	无	必填，结束时间日期（场内除息日），%Y-%m-%d 格式
返回值：dataframe

字段名	类型	中文名称	说明
fund	str	基金代码	查询分红信息的基金代码
pub_date	str	公告日	%Y-%m-%d 格式
event_progress	str	方案进度	正式，预案
dvd_ratio	float	派息比例	10:X，每 10 份税前分红
dvd_base_date	str	分配收益基准日	%Y-%m-%d 格式
rt_reg_date	str	权益登记日	%Y-%m-%d 格式
ex_act_date	str	实际除息日	%Y-%m-%d 格式
ex_dvd_date	str	场内除息日	%Y-%m-%d 格式
pay_dvd_date	str	场内红利发放日	%Y-%m-%d 格式
trans_dvd_date	str	场内红利款账户划出日	%Y-%m-%d 格式
reinvest_cfm_date	str	红利再投资确定日	%Y-%m-%d 格式
ri_shr_arr_date	str	红利再投资份额到账日	%Y-%m-%d 格式
ri_shr_rdm_date	str	红利再投资赎回起始日	%Y-%m-%d 格式
earn_distr	float	可分配收益	单位：元
cash_pay	float	本期实际红利发放	单位：元
base_unit_nv	float	基准日基金份额净值	单位：元
dvd_target	str	分派对象	
示例：

fnd_get_dividend(fund='SZSE.161725', start_date="2000-01-01", end_date="2022-09-07")
 
        复制成功
    
输出：

  fund    pub_date event_progress  dvd_ratio dvd_base_date rt_reg_date ex_act_date ex_dvd_date pay_dvd_date trans_dvd_date reinvest_cfm_date ri_shr_arr_date ri_shr_rdm_date  earn_distr  cash_pay  base_unit_nv
0  SZSE.161725  2021-09-02             正式       0.12    2021-08-27  2021-09-07  2021-09-07  2021-09-08   2021-09-09     2021-09-09        2021-09-07      2021-09-08      2021-09-09  3.7574e+10       0.0        1.1893
1  SZSE.161725  2021-12-07             正式       0.28    2021-12-02  2021-12-09  2021-12-09  2021-12-10   2021-12-13     2021-12-13        2021-12-09      2021-12-10      2021-12-13  3.3549e+10       0.0        1.3696
2  SZSE.161725  2021-12-29             正式       0.45    2021-12-24  2021-12-31  2021-12-31  2022-01-04   2022-01-05     2022-01-05        2021-12-31      2022-01-04      2022-01-05  3.0723e+10       0.0        1.4178


 
        复制成功
    
注意：

1. 仅提供场内基金（ETF、LOF、FOF-LOF）的复权因子数据。

2. start_date 小于或等于 end_date时取指定时间段的数据,当start_date > end_date时返回报错。

