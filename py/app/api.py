import tushare as ts

ts.set_token('439b79afc0af96f0abb32a3be27df99b9e8fe9fa83f8d555d66fba72')

pro = ts.pro_api()

# df = pro.namechange(ts_code='600806.SH', fields='ts_code,name,start_date,end_date,change_reason')
# df = pro.namechange(ts_code='000632.SZ')
df = pro.namechange(start_date='19000101', end_date='20250501')
print(df)
