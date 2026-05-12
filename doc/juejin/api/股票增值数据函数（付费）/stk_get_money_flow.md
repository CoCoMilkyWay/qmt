stk_get_money_flow - 查询股票交易资金流向
查询股票每日交易的资金流向

gm SDK 3.0.172 版本新增

函数原型：

stk_get_money_flow(symbols, trade_date=None)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	必填，可输入多个，使用时参考symbol. 采用 str 格式时，多个标的代码必须用英文逗号分割（逗号中间不能有空格），如：'SHSE.600008,SZSE.000002'; 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
trade_date	str	交易日期	N	None	交易日期，支持str格式（%Y-%m-%d 格式），默认None表示最新交易日期。
返回值：

字段名	类型	中文名称	说明
symbol	str	股票代码	格式exchange.sec_id（SHSE.600000, SZSE.000001）
trade_date	str	交易日期	
main_in	float	主力流入资金	超大单加大单买入成交额之和
main_out	float	主力流出资金	超大单加大单卖出成交额之和
main_net_in	float	主力净流入资金	主力流入资金-主力流出资金
main_net_in_rate	float	主力资金净流入率	主力净流入资金/主力总成交额
super_in	float	超大单流入资金	大于等于50万股或者100万元的成交单买入成交额
super_out	float	超大单流出资金	大于等于50万股或者100万元的成交单卖出成交额
super_net_in	float	超大单净流入资金	超大单流入资金-超大单流出资金
super_net_in_rate	float	超大单净流入率	超大单净流入资金/超大单总成交额
large_in	float	大单流入资金	大于等于10万股或者20万元且小于50万股和100万元的成交单买入成交额
large_out	float	大单流出资金	大于等于10万股或者20万元且小于50万股和100万元的成交单卖出成交额
large_net_in	float	大单净流入资金	大单流入资金-大单流出资金
large_net_in_rate	float	大单净流入率	大单净流入资金/大单总成交额
mid_in	float	中单流入资金	大于等于2万股或者4万元且小于10万股和20万元的成交单买入成交额
mid_out	float	中单流出资金	大于等于2万股或者4万元且小于10万股和20万元的成交单卖出成交额
mid_net_in	float	中单净流入资金	中单流入资金-中单流出资金
mid_net_in_rate	float	中单净流入率	中单净流入资金/中单总成交额
small_in	float	小单流入资金	小于2万股和4万元的成交单买入成交额
small_out	float	小单流出资金	小于2万股和4万元的成交单卖出成交额
small_net_in	float	小单净流入资金	小单流入资金-小单流出资金
small_net_in_rate	float	小单净流入率	小单净流入资金/小单总成交额
示例：

stk_get_money_flow(symbols='SZSE.002583,SHSE.603955',trade_date='2024-11-20')
 
        复制成功
    
输出：

symbol                 trade_date       main_in      main_out  \
0  SHSE.603955  2024-11-20T00:00:00+08:00   275694357.0   242070831.0   
1  SZSE.002583  2024-11-20T00:00:00+08:00  5505519712.0  6385983392.0   
   main_net_in  main_net_in_rate      super_in     super_out  super_net_in  \
0   33623526.0          5.248040   128754495.0   101308759.0    27445736.0   
1 -880463680.0         -7.440565  3101942544.0  3378101408.0  -276158864.0   
   super_net_in_rate      large_in     large_out  large_net_in  \
0           4.283796   146939862.0   140762072.0     6177790.0   
1          -2.333745  2403577168.0  3007881984.0  -604304816.0   
   large_net_in_rate        mid_in       mid_out   mid_net_in  \
0           0.964244   160360769.0   176158944.0  -15798175.0   
1          -5.106820  3212696272.0  3014466672.0  198229600.0   
   mid_net_in_rate      small_in     small_out  small_net_in  \
0        -2.465817   111742406.0   129567758.0   -17825352.0   
1         1.675186  2781545824.0  2099311744.0   682234080.0   
   small_net_in_rate  
0          -2.782223  
1           5.765379  

 
        复制成功
    
注意：

1. 日频资金流向有效数据从2010-01-04开始

2. 订单大小具体定义：https://finance.eastmoney.com/a/20110101117172217.html

