stk_get_symbol_industry - 查询股票的所属行业
查询指定股票所属的行业

函数原型：

stk_get_symbol_industry(symbols, source="zjh2012", level=1, date="")
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	多个代码可用 ，使用时参考symbol 采用 str 格式时，多个标的代码必须用英文逗号分割如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
source	str	行业来源	N	'zjh2012'	'zjh2012'- 证监会行业分类 2012（默认）， 'sw2021'- 申万行业分类 2021, 查看行业分类
level	int	行业分级	N	1	1 - 一级行业（默认），2 - 二级行业，3 - 三级行业
date	str	查询日期	N	""	查询行业分类的指定日期，%Y-%m-%d 格式，默认""表示最新时间
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	股票代码	exchange.sec_id
sec_name	str	股票名称	symbol 对应的股票名称
industry_code	str	行业代码	指定行业来源下，symbol 所属的行业代码
industry_name	str	行业名称	指定行业来源下，symbol 所属的行业名称
示例：

stk_get_symbol_industry(symbols='SHSE.600000, SZSE.000002', source="zjh2012", level=1, date="")
 
        复制成功
    
输出：

        symbol sec_name industry_code industry_name
0  SHSE.600000     浦发银行             J           金融业
1  SZSE.000002      万科A             K          房地产业
 
        复制成功
    
注意：

1. 证监会行业分类 2012 没有三级行业，若输入source='zjh2012', level=3则参数无效，返回空dataframe

