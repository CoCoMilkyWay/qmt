import tushare as ts

ts.set_token('439b79afc0af96f0abb32a3be27df99b9e8fe9fa83f8d555d66fba72')

pro = ts.pro_api()

df = pro.forecast_vip(period='20260331',fields='ts_code,ann_date,end_date,type,p_change_min,p_change_max,net_profit_min')
print(df)