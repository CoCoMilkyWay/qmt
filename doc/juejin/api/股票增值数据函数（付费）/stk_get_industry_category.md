stk_get_industry_category - 查询行业分类
查询指定行业来源的行业列表

函数原型：

stk_get_industry_category(source='zjh2012', level=1)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
source	str	行业来源	N	'zjh2012'	'zjh2012'- 证监会行业分类 2012（默认）， 'sw2021'- 申万行业分类 2021, 查看行业分类
level	int	行业分级	N	1	1 - 一级行业（默认），2 - 二级行业，3 - 三级行业
返回值：dataframe

字段名	类型	中文名称	说明
industry_code	str	行业代码	所选行业来源，对应的行业代码
industry_name	str	行业名称	所选行业来源，对应的行业名称
示例：

industry_category = stk_get_industry_category(source='sw2021', level=2)
 
        复制成功
    
输出：

    industry_code industry_name
0          110100           种植业
1          110200            渔业
2          110300           林业Ⅱ
3          110400            饲料
4          110500         农产品加工
..            ...           ...
129        760100          环境治理
130        760200         环保设备Ⅱ
131        770100          个护用品
132        770200           化妆品
133        770300          医疗美容
[134 rows x 2 columns]
 
        复制成功
    
注意：

1. 证监会行业分类 2012 没有三级行业，若输入source='zjh2012', level=3则参数无效，返回空dataframe

