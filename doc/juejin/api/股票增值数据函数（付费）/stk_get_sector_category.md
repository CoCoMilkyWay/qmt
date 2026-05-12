stk_get_sector_category - 查询板块分类
查询指定类型的板块列表

函数原型：

stk_get_sector_category(sector_type)
 
        复制成功
    
参数：

参数名	类型	中文名称	必填	默认值	参数用法说明
sector_type	str	板块类型	Y	无	只能选择一种类型，可选择 1001:市场类 1002:地域类 1003:概念类, 查看板块分类
返回值：dataframe

字段名	类型	中文名称	说明
sector_code	str	板块代码	所选板块类型的板块代码
sector_name	str	板块名称	所选板块类型的板块名称
示例：

stk_get_sector_category(sector_type='1003')
 
        复制成功
    
输出：

   sector_code      sector_name
0        007001          军工
1        007003         煤化工
2        007004         新能源
3        007005        节能环保
4        007007         AB股
..          ...         ...
420      007499        存储芯片
421      007500        液冷概念
422      007501         中特估
423      007502        央企改革
424      007503        混合现实
[425 rows x 2 columns]
