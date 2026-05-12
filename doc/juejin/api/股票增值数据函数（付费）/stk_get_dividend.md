stk_get_dividend - 查询股票分红送股信息
查询指定股票在一段时间内的分红送股信息

函数原型：

stk_get_dividend(symbol, start_date, end_date)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	标的代码	Y	无	必填，只能填一个股票标的，使用时参考symbol
start_date	str	开始时间	Y	无	必填，开始时间日期（除权除息日），%Y-%m-%d 格式
end_date	str	结束时间	Y	无	必填，结束时间日期（除权除息日），%Y-%m-%d 格式
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	股票代码	exchange.sec_id
scheme_type	str	分配方案	现金分红，送股，转增
pub_date	str	公告日	%Y-%m-%d 格式
equity_reg_date	str	股权登记日	%Y-%m-%d 格式
ex_date	str	除权除息日	%Y-%m-%d 格式
cash_pay_date	str	现金红利发放日	%Y-%m-%d 格式
share_acct_date	str	送转股到账日	%Y-%m-%d 格式
share_lst_date	str	新增股份上市流通日	红股上市日或送（转增）股份上市交易日, %Y-%m-%d 格式
cash_af_tax	float	税后红利	单位：元/10 股
cash_bf_tax	float	税前红利	单位：元/10 股
bonus_ratio	float	送股比例	10:X
convert_ratio	float	转增比例	10:X
base_date	str	股本基准日	%Y-%m-%d 格式
base_share	float	股本基数	基准股本
dvd_target	str	分派对象	如：全体股东，流通股股东，非流通股股东，A股股东，A股流通股股东，A股限售股股东
示例：

stk_get_dividend(symbol='SHSE.600000', start_date='2022-07-01', end_date='2022-07-31')
 
        复制成功
    
输出：

        symbol scheme_type    pub_date equity_reg_date     ex_date cash_pay_date share_acct_date share_lst_date  cash_af_tax  cash_bf_tax  bonus_ratio  convert_ratio   base_date  base_share
0  SHSE.600000        现金分红  2022-07-13      2022-07-20  2022-07-21    2022-07-21            None           None         3.69          4.1          0.0            0.0  2022-07-20  2.9352e+10
 
        复制成功
    
注意：

1. 当start_date小于或等于end_date 时取指定时间段的数据,当start_date>end_date时返回报错.

