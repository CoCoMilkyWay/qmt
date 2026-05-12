fut_get_contract_info - 查询期货标准品种信息
查询交易所披露的期货标准品种的合约规格/合约文本

函数原型：

fut_get_contract_info(product_codes, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
product_codes	str or list	品种代码	Y	无	必填，交易品种代码，如：IF，AL 多个代码可用 ， 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'IF, AL' 采用 list 格式时，多个标的代码示例：['IF', 'AL']
df	bool	返回格式	N	False	是否返回 dataframe 格式，默认False返回字典格式，返回 list[dict]，列表每项的 dict 的 key 值见返回字段名
返回值：dataframe或list[dict]

字段名	类型	中文名称	说明
product_name	str	交易品种	交易品种名称，如：沪深 300 指数，铝
product_code	str	交易代码	交易品种代码，如：IF，AL
underlying_symbol	str	合约标的	如：SHSE.000300， AL
multiplier	int	合约乘数	如：200，5
trade_unit	str	交易单位	如：每点人民币 200 元，5 吨/手
price_unit	str	报价单位	如：指数点，元（人民币）/吨
price_tick	str	价格最小变动单位	如：0.2 点，5 元/吨
delivery_month	str	合约月份	如："当月、下月及随后两个季月"，"1 ～ 12 月"
trade_time	str	交易时间	如："9:30-11:30，13:00-15:00"， "上午 9:00－11:30 ，下午 1:30－3:00 和交易所规定的其他交易时间"
price_range	str	涨跌停板幅度	每日价格最大波动限制，如："上一个交易日结算价的 ±10%"， "上一交易日结算价 ±3%"
minimum_margin	str	最低交易保证金	交易所公布的最低保证金比例，如："合约价值的 8%"，"合约价值的 5%"
last_trade_date	str	最后交易日	如："合约到期月份的第三个星期五，遇国家法定假日顺延"， "合约月份的 15 日（遇国家法定节假日顺延，春节月份等最后交易日交易所可另行调整并通知）"
delivery_date	str	交割日	如："同最后交易日"，"最后交易日后连续三个工作日"
delivery_method	str	交割方式	如："现金交割"，"实物交割"
exchange_name	str	交易所名称	上市交易所名称，如："中国金融期货交易所"，"上海期货交易所"
exchange	str	交易所代码	交易品种名称，如："沪深 300 指数"，"铝"
示例：

fut_get_contract_info(product_codes='IF')
 
        复制成功
    
输出：

[{'product_name': '沪深300股指期货', 'product_code': 'IF', 'underlying_symbol': 'SHSE.000300', 'multiplier': 300, 'trade_unit': '每点300元', 'price_unit': '指数点', 'price_tick': '0.2点', 'delivery_month': '当月、下月及随后两个季月', 'trade_time': '上午9:30-11:30,下午13:00-15:00', 'price_range': '上一个交易日结算价的±10%', 'minimum_margin': '合约价值的8%', 'last_trade_date': '合约到期月份的第三个周五,遇国家法定假日顺延', 'delivery_date': '同最后交易日', 'delivery_method': '现金交割', 'exchange_name': '中国金融期货交易所', 'exchange': 'CFFEX'}]
 
        复制成功
    
