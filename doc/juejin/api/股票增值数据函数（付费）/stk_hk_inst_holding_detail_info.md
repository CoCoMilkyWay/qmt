stk_hk_inst_holding_detail_info - 查询沪深港通标的港股机构持股明细数据
查询沪深港通标的港股机构持股明细数据

gm SDK 3.0.163 版本新增

交易所信息披露调整，数据最晚更新到2024.8.16，历史数据不受影响

函数原型：

stk_hk_inst_holding_detail_info(symbols=None, trade_date=None, df=False)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str	股票代码	N	None	输入标的代码，可输入多个. 采用 str 格式时，多个标的代码必须用英文逗号分割，如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002'] 默认None表示所有标的。
trade_date	str or datetime.date	交易日期	N	None	交易日期，支持str格式（%Y-%m-%d 格式）和 datetime.date 格式，默认None表示最新交易日期。
df	bool	返回格式	N	False	是否返回 dataframe 格式 ， 默认False返回 list[dict],列表每项的dict的key值为参数指定的 fields 。
返回值：

字段名	类型	中文名称	说明
trade_date	str	最新交易日期	最新交易日期
symbol	str	证券代码	证券代码
sec_name	str	证券简称	证券简称
participant_name	str	参与者名称	参与者名称
share_holding	int	持股量(股)	持股量(股)
shares_rate	float	占已发行股份(%)	占已发行股份(%)
示例：

stk_hk_inst_holding_detail_info(symbols='SHSE.600008', trade_date=None, df=False)
 
        复制成功
    
输出：

[{'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': 'CREDIT SUISSE SECURITIES (HONG KONG) LTD', 'share_holding': 374905, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': 'J.P. MORGAN BROKING (HONG KONG) LTD', 'share_holding': 6445488, 'shares_rate': 0.08},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': 'JPMORGAN CHASE BANK, NATIONAL ASSOCIATION', 'share_holding': 19630045, 'shares_rate': 0.26},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': 'MLFE LTD', 'share_holding': 2134425, 'shares_rate': 0.02},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': 'MORGAN STANLEY HONG KONG SECURITIES LTD', 'share_holding': 2962125, 'shares_rate': 0.04},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': 'Societe Generale', 'share_holding': 176637, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': 'UBS SECURITIES HONG KONG LTD', 'share_holding': 2238651, 'shares_rate': 0.03},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '上银证券有限公司', 'share_holding': 132000, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '东亚证券有限公司', 'share_holding': 7000, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '中信证券经纪(香港)有限公司', 'share_holding': 22900, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '中信里昂证券有限公司', 'share_holding': 158790, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '中国国际金融香港证券有限公司', 'share_holding': 821082, 'shares_rate': 0.01},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '中国建设银行(亚洲)股份有限公司', 'share_holding': 10600, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '中国银行(香港)有限公司', 'share_holding': 219800, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '中银国际证券有限公司', 'share_holding': 49824935, 'shares_rate': 0.67},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '元大证券(香港)有限公司', 'share_holding': 60000, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '创兴证券有限公司', 'share_holding': 13000, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '华盛资本证券有限公司', 'share_holding': 27800, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '国信证券(香港)经纪有限公司', 'share_holding': 400, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '国泰君安证券(香港)有限公司', 'share_holding': 280600, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '大华继显(香港)有限公司', 'share_holding': 141800, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '宝生证券有限公司', 'share_holding': 655900, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '富途证券国际(香港)有限公司', 'share_holding': 77300, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '广发证券(香港)经纪有限公司', 'share_holding': 14000, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '恒生证券有限公司', 'share_holding': 179700, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '永丰金证券(亚洲)有限公司', 'share_holding': 52000, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '法国巴黎银行', 'share_holding': 4579831, 'shares_rate': 0.06},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '渣打银行(香港)有限公司', 'share_holding': 13222494, 'shares_rate': 0.18},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '盈透证券香港有限公司', 'share_holding': 47930, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '耀才证券国际(香港)有限公司', 'share_holding': 3000, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '花旗银行', 'share_holding': 11468541, 'shares_rate': 0.15},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '辉立证券(香港)有限公司', 'share_holding': 337500, 'shares_rate': 0.0},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '香港上海汇丰银行有限公司', 'share_holding': 10059735, 'shares_rate': 0.13},
 {'symbol': 'SHSE.600008', 'trade_date': '2024-01-25', 'sec_name': '首创环保', 'participant_name': '高盛(亚洲)证券有限公司', 'share_holding': 5938226, 'shares_rate': 0.08}]

 
        复制成功
    
注意：

1. 数据日频更新，在交易日约20点更新当日数据。如果当前交易日数据尚未更新，调用时不指定trade_date会返回前一交易日的数据，调用时指定trade_date为当前交易日会返回空。

2. trade_date输入非交易日，会返回空。

