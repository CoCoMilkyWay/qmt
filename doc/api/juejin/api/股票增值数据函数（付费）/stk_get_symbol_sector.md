stk_get_symbol_sector - 查询股票的所属板块
查询指定股票所属的板块

函数原型：

stk_get_symbol_sector(symbols, sector_type)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
symbols	str or list	股票代码	Y	无	多个代码可用 ，使用时参考symbol 采用 str 格式时，多个标的代码必须用英文逗号分割如：'SHSE.600008,SZSE.000002' 采用 list 格式时，多个标的代码示例：['SHSE.600008', 'SZSE.000002']
sector_type	str	板块类型	Y	无	只能选择一种类型，可选择 1001:市场类 1002:地域类 1003:概念类
返回值：dataframe

字段名	类型	中文名称	说明
symbol	str	股票代码	exchange.sec_id
sec_name	str	股票名称	symbol 对应的股票名称
sector_code	str	板块代码	指定板块类型下，symbol 所属的板块代码
sector_name	str	板块名称	指定板块类型下，symbol 所属的板块名称
示例：

stk_get_symbol_sector(symbols='SHSE.600008,SZSE.000002', sector_type='1002')
 
        复制成功
    
输出：

        symbol sec_name   sector_code sector_name
0  SHSE.600008     首创环保  006002001001         北京市
1  SZSE.000002      万科A  006006001015         深圳市
 