stk_get_share_change - 查询股本变动
查询上市公司的一段时间内公告的股本变动情况

函数原型：

stk_get_share_change(symbol, start_date="", end_date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbol	str	股票代码	Y	无	必填，只能填一个股票标的，使用时参考symbol
start_date	str	开始时间	N	""	开始时间日期（发布日期），%Y-%m-%d 格式，默认""表示最新时间
end_date	str	结束时间	N	""	结束时间日期（发布日期），%Y-%m-%d 格式，默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	股票代码	exchange.sec_id
company_name	str	公司名称	symbol 对应的公司名称
pub_date	str	发布日期	
chg_date	str	股本变动日期	
chg_reason	str	股本变动原因	
chg_event	str	股本变动事件	
share_total	float	总股本	未流通股份+已流通股份，单位：股
share_total_nlf	float	未流通股份	单位：股
share_prom	float	一、发起人股份	国有发起人股 + 发起社会法人股 + 其他发起人股份，单位：股
share_prom_state	float	1.国有发起人股	国家持股+国有法人股，单位：股
share_state	float	（1）国家股	单位：股
share_state_lp	float	（2）国有法人股	单位：股
share_prom_soc	float	2.发起社会法人股	境内社会法人股+境外法人股，单位：股
share_dc_lp	float	（1）境内社会法人股	单位：股
share_os_lp	float	（2）境外法人股	单位：股
share_prom_other	float	3.其他发起人股份	单位：股
share_rs	float	二、募集人股份	募集国家股+募集境内法人股+募集境外法人股，单位：股
share_rs_state	float	1.募集国家股	单位：股
share_rs_dc_lp	float	2.募集境内法人股	募集境内国有法人股+募集境内社会法人股，单位：股
share_rs_state_lp	float	（1）募集境内国有法人股	单位：股
share_rs_soc_lp	float	（2）募集境内社会法人股	单位：股
share_rs_os_lp	float	3.募集境外法人股	单位：股
share_emp_nlf	float	三、内部职工股	单位：股
share_pfd_nlf	float	四、优先股	单位：股
share_oth_nlf	float	五、其他未流通股份	单位：股
share_circ	float	流通股份	单位：股，无限售条件股份+有限售条件股份，实际流通股份可用share_ttl_unl（无限售条件股份）
share_ttl_unl	float	无限售条件股份	人民币普通股（A 股）+ 境内上市外资股（B 股）+ 境外上市外资股（H 股）+ 其他已流通股份，单位：股
share_a_unl	float	1.人民币普通股（A 股）	单位：股
share_b_unl	float	2.境内上市外资股（B 股）	单位：股
share_h_unl	float	3.境外上市外资股（H 股）	单位：股
share_oth_unl	float	4.其他已流通股份	单位：股
share_ttl_ltd	float	有限售条件股份	单位：股
share_gen_ltd	float	一、一般有限售条件股份	限售国家持股+ 限售国有法人持股+ 限售其他内资持股+ 限售外资持股+ 锁定股份+ 高管持股，单位：股
share_state_ltd	float	1.限售国家持股	单位：股
share_state_lp_ltd	float	2.限售国有法人持股	单位：股
share_oth_dc_ltd	float	3.限售其他内资持股	限售境内非国有法人持股+限售境内自然人持股，单位：股
share_nst_dc_lp_ltd	float	（1）限售境内非国有法人持股	单位：股
share_dc_np_ltd	float	（2）限售境内自然人持股	单位：股
share_forn_ltd	float	4.限售外资持股	限售境外法人持股+限售境外自然人持股，单位：股
share_os_lp_ltd	float	（1）限售境外法人持股	单位：股
share_os_np_ltd	float	（2）限售境外自然人持股	单位：股
share_lk_ltd	float	5.锁定股份	单位：股
share_gm_ltd	float	6.高管持股(原始披露)	单位：股
share_plc_lp_ltd	float	二、配售法人持股	战略投资者配售股份+一般法人投资者配售+ 证券投资基金配售股份，单位：股
share_plc_si_ltd	float	1.战略投资者配售股份	单位：股
share_plc_lp_gen_ltd	float	2.一般法人投资者配售股份	单位：股
share_plc_fnd_ltd	float	3.证券投资基金配售股份	单位：股
share_a_ltd	float	限售流通 A 股	单位：股
share_b_ltd	float	限售流通 B 股	单位：股
share_h_ltd	float	限售流通 H 股	单位：股
share_oth_ltd	float	其他限售股份	单位：股
share_list_date	str	变动股份上市日	%Y-%m-%d 格式
示例：

stk_get_share_change(symbol='SHSE.605090', start_date="2020-01-01", end_date="2022-10-01")
 
        复制成功
    
输出：

        symbol  company_name    pub_date    chg_date chg_reason chg_event  share_total  share_total_nlf  share_prom  share_prom_state  share_state  share_state_lp  share_prom_soc  share_dc_lp  share_os_lp  share_prom_other  share_rs  share_rs_state  share_rs_dc_lp  share_rs_state_lp  share_rs_soc_lp  share_rs_os_lp  share_emp_nlf  share_pfd_nlf  share_oth_nlf  share_circ  share_ttl_unl  share_a_unl  share_b_unl  share_h_unl  share_oth_unl  share_ttl_ltd  share_gen_ltd  share_state_ltd  share_state_lp_ltd  share_oth_dc_ltd  share_nst_dc_lp_ltd  share_dc_np_ltd  share_forn_ltd  share_os_lp_ltd  share_os_np_ltd  share_lk_ltd  share_gm_ltd  share_plc_lp_ltd  share_plc_si_ltd  share_plc_lp_gen_ltd  share_plc_fnd_ltd  share_a_ltd  share_b_ltd  share_h_ltd  share_oth_ltd share_list_date
0  SHSE.605090  江西九丰能源股份有限公司  2021-05-24  2021-05-13     首发A股上市      发行融资   4.4297e+08              0.0         0.0               0.0          0.0             0.0             0.0          0.0          0.0               0.0       0.0             0.0             0.0                0.0              0.0             0.0            0.0            0.0            0.0  4.4297e+08     8.2970e+07   8.2970e+07          0.0          0.0            0.0     3.6000e+08     3.6000e+08              0.0                 0.0        3.1967e+08           2.1591e+08       1.0376e+08      4.0330e+07       4.0330e+07              0.0           0.0    1.0376e+08               0.0               0.0                   0.0                0.0   3.6000e+08          0.0          0.0            0.0      2021-05-25
1  SHSE.605090  江西九丰能源股份有限公司  2022-05-12  2022-05-18      转增股上市      分红派息   6.2016e+08              0.0         0.0               0.0          0.0             0.0             0.0          0.0          0.0               0.0       0.0             0.0             0.0                0.0              0.0             0.0            0.0            0.0            0.0  6.2016e+08     1.1616e+08   1.1616e+08          0.0          0.0            0.0     5.0400e+08     5.0400e+08              0.0                 0.0        4.4754e+08           3.0228e+08       1.4526e+08      5.6462e+07       5.6462e+07              0.0           0.0    0.0000e+00               0.0               0.0                   0.0                0.0   5.0400e+08          0.0          0.0            0.0      2022-05-19
2  SHSE.605090  江西九丰能源股份有限公司  2022-05-25  2022-05-30   首发限售股份上市     限售股解禁   6.2016e+08              0.0         0.0               0.0          0.0             0.0             0.0          0.0          0.0               0.0       0.0             0.0             0.0                0.0              0.0             0.0            0.0            0.0            0.0  6.2016e+08     2.5281e+08   2.5281e+08          0.0          0.0            0.0     3.6735e+08     3.6735e+08              0.0                 0.0        3.6735e+08           2.2209e+08       1.4526e+08      0.0000e+00       0.0000e+00              0.0           0.0    0.0000e+00               0.0               0.0                   0.0                0.0   3.6735e+08          0.0          0.0            0.0      2022-05-30
 
        复制成功
    
注意：

当start_date == end_date时，取离end_date最近发布日期的一条数据， 当start_date < end_date时，取指定时间段的数据， 当start_date > end_date时，返回报错。

