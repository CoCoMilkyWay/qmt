stk_get_shareholder_num - 查询股东户数
查询上市公司股东总数，A 股股东、B 股股东、H 股股东总数

函数原型：

stk_get_shareholder_num(symbol, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	股票代码	Y	无	必填，只能填一个股票标的，使用时参考symbol
start_date	str	开始时间	N	""	开始时间日期（公告日期），%Y-%m-%d 格式，默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期（公告日期），%Y-%m-%d 格式，默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	股票代码	exchange.sec_id
sec_name	str	股票名称	symbol 对应的股票名称
pub_date	str	公告日期	
expiry_date	str	截止日期	
total_share	int	股东总数	
total_share_a	int	A 股股东总数	
total_share_b	int	流通 B 股股东总数	
total_share_h	int	流通 H 股股东总数	
other_share	int	其他股东户数	
total_share_pfd	int	优先股股东总数（表决权恢复）	
total_share_mgn	int	股东户数（含融资融券）	合并普通账户和融资融券信用账户后的股东总户数
total_share_no_mgn	int	股东户数（不含融资融券）	普通账户的股东总户数
示例：

stk_get_shareholder_num(symbol='SZSE.002594', start_date="2022-01-01", end_date="2022-08-01")
 
        复制成功
    
输出：

        symbol sec_name    pub_date expiry_date  total_share  total_share_a  total_share_b  total_share_h  other_share  total_share_pfd  total_share_mgn  total_share_no_mgn
0  SZSE.002594      比亚迪  2022-03-30  2021-12-31       357227         357109              0            118            0                0                0                   0
1  SZSE.002594      比亚迪  2022-03-30  2022-02-28       392631         392511              0            120            0                0                0                   0
2  SZSE.002594      比亚迪  2022-04-28  2022-03-31       405607         405486              0            121            0                0                0                   0
 
        复制成功
    
注意：

当start_date == end_date时，取离end_date最近公告日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

