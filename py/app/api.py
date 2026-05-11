import tushare as ts

ts.set_token('439b79afc0af96f0abb32a3be27df99b9e8fe9fa83f8d555d66fba72')

pro = ts.pro_api()

df = pro.namechange(ts_code='600806.SH', fields='ts_code,name,start_date,end_date,change_reason')
print(df)
