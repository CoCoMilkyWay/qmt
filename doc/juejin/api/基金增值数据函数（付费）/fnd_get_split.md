fnd_get_split - 查询基金拆分折算信息
查询指定基金在一段时间内的拆分折算信息

函数原型：

fnd_get_split(fund, start_date, end_date)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
fund	str	基金代码	Y	无	必填，只能输入一个基金的symbol，如：'SZSE.159919'
start_date	str	开始时间	Y	无	必填，开始时间日期（场内除权日），%Y-%m-%d 格式
end_date	str	结束时间	Y	无	必填，结束时间日期（场内除权日），%Y-%m-%d 格式
返回值：dataframe

字段名	类型	中文名称	说明
fund	str	基金代码	查询分红信息的基金代码
pub_date	str	公告日	%Y-%m-%d 格式
split_type	str	拆分折算类型	折算，拆分，特殊折算
split_ratio	float	拆分折算比例	10:X
base_date	str	拆分折算基准日	%Y-%m-%d 格式
ex_date	str	拆分折算场内除权日	%Y-%m-%d 格式
share_change_reg_date	str	基金份额变更登记日	%Y-%m-%d 格式
nv_split_pub_date	str	基金披露净值拆分折算日	%Y-%m-%d 格式
rt_reg_date	str	权益登记日	%Y-%m-%d 格式
ex_date_close	str	场内除权日(收盘价)	%Y-%m-%d 格式
示例：

fnd_get_split(fund='SZSE.161725', start_date="2000-01-01", end_date="2022-09-07")
 
        复制成功
    
输出：

           fund    pub_date split_type  split_ratio   base_date     ex_date share_change_reg_date nv_split_pub_date rt_reg_date ex_date_close
0  SZSE.161725  2015-12-17         折算      10.1801  2015-12-15  2015-12-17            2015-12-16        2015-12-15        None    2015-12-17
1  SZSE.161725  2016-12-19         折算      10.2300  2016-12-15  2016-12-19            2016-12-16        2016-12-15        None    2016-12-19
2  SZSE.161725  2017-09-28         折算      14.9420  2017-09-26  2017-09-28            2017-09-27        2017-09-26        None    2017-09-28
3  SZSE.161725  2017-12-19         折算      10.0445  2017-12-15  2017-12-19            2017-12-18        2017-12-15        None    2017-12-19
4  SZSE.161725  2018-12-19         折算      10.2547  2018-12-17  2018-12-19            2018-12-18        2018-12-17        None    2018-12-19
5  SZSE.161725  2019-07-04         折算      15.5686  2019-07-02  2019-07-04            2019-07-03        2019-07-02        None    2019-07-04
6  SZSE.161725  2019-12-18         折算      10.1067  2019-12-16  2019-12-18            2019-12-17        2019-12-16        None    2019-12-18
7  SZSE.161725  2020-08-28         折算      14.9817  2020-08-26  2020-08-28            2020-08-27        2020-08-26        None    2020-08-28
8  SZSE.161725  2020-12-17         折算      10.0544  2020-12-15  2020-12-17            2020-12-16        2020-12-15        None    2020-12-17
 
        复制成功
    
注意：

1. 仅提供场内基金（ETF、LOF、FOF-LOF）的复权因子数据。

2. start_date 小于或等于 end_date时取指定时间段的数据,当start_date > end_date时返回报错。

