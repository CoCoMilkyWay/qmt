get_price 移动窗口

历史范围：2010年至今 凌晨3:00更新

学习材料：JQData 日分钟API清洗与处理规则

get_price(security, start_date=None, end_date=None, frequency='daily', fields=['open','close','low','high','volume','money','factor',
'high_limit','low_limit','avg','pre_close','paused'], skip_paused=False, fq='pre', count=None)

描述

当可以获取指定标的一天或一分钟行情数据，返回标的的开盘价、收盘价、最高价、最低价，成交的数量,时段中成交的金额、复权因子、时间段中的涨停价、时段中跌停价、时段中平均价，是否停牌，前一天收盘价等，同时也可以利用date数据查看所返回的数据是什么时刻的。

panel=True时且pandas<0.25时返回[pandas.Panel]对象 , panel= False 或 pandas>=0.25时返回[pandas.DataFrame]对象

获取一支或者多只标的的行情

注意

当天 09:00 ~ 15:00 的行情在 15:00 之后可以获取 ,当end_date指定为当天尚未结束的交易时间时，会自动填充为上一个交易日的盘后15点时间

注：期货当天 09:00 ~ 15:00 的行情在 15:00 之后可以获取 ,期货夜盘 21:00~次日02:30 的分钟行情在 02:30 之后可以获取。当end_date指定为当天尚未结束的交易时间时，会自动填充为上一个交易日的盘后(或夜盘结束)时间

获取一支或者多只标的1天或者1分钟行情；

frequency为非一天或者一分钟，请使用get_bars;

标识时间为09:32:00的1分钟k线，其数据时间为09:31:00至09:31:59；

关于停牌: 因为此API可以获取多只股票的数据, 可能有的股票停牌有的没有, 为了保持时间轴的一致

我们默认没有跳过停牌的日期, 停牌时使用停牌前的数据填充. 如想跳过, 请使用 skip_paused=True 参数, 注意当 panel=True 且获取多标的时不支持(panel结构需要索引对齐)

get_price 参数说明

参数名称	参数说明	默认值

security	指定标的，获取多个标的时需传入List	无默认值，必填项

start_date	开始时间	2015/1/1 0:00:00

end_date	结束时间	2015/12/31 0:00:00

count	表示获取 end_date 之前几个 frequency 的数据	无默认值，必填项

frequency	单位时间长度，即指定获取的时间频率	无默认值，必填项

fields	所获取数据的字段名称，即表头	['open','close','high','low','volume','money']

skip_paused	是否跳过不交易日期(含：停牌/未上市/退市后的日期)	False

fill_paused	对于停牌股票的价格处理	True

fq	复权选项	pre

panel	当本地pandas版本小于0.25时，指定返回的数据格式是否为panel	True

get_price参数补充说明

security
支持的标的种类为【股票，基金，指数，可转债，期货，期权】

start_date
不可与‘count’同时存在；当start_date没有指定时分秒的时间戳时，时分秒的默认值为00:00:00；

注：当frequency不为1d/1m时，将从start_date开始,从前向后将每X个的(1m/1d)的数据合并成一条,一直到取到end_date为止

end_date

当end_date没有指定时分秒的时间戳，时分秒的默认值为00:00:00，所以此时返回的数据不包括 end_date这天；

当end_date指定为当天尚未结束的交易时间时，会自动填充为上一个交易日的盘后时间。

注：当frequency不为1d/1m时，可与‘count’与‘start_date’分开搭配获得不同的使用效果

count

只与‘end_date’搭配使用；

注：当指定了count 时，从end_date开始, 从后向前将每X个的(1m/1d)的数据合并成一条,一直到取到足够的数量

frequency

支持[ 'Xm'(X分钟) , 'Xd'(X天) , 'Daily'(即1天,等于1d) , '1m'(即1分钟，等于1m)]，
可指定任意数量长度，即'X'可为任一大于0的自然数 ，例如'5d','3m','10m'等

注：指定单位为分钟时，会跨交易日合并分钟bar数据

skip_paused

如果不跳过, 停牌时会使用停牌前的数据填充(如fill_paused=True)，上市前或者退市后数据都为 nan。

fill_paused

True 表示用pre_close价格填充;

False 表示使用NAN填充停牌的股票价格。

fq

'pre'：前复权

'none'：不复权, 返回实际价格

'post'：后复权

注：fq:复权选项(仅对股票/基金品种生效，生效的fields（字段）包括['open','close','high','low','volume','factor','high_limit','low_limit','avg','pre_close']

panel

当本地pandas版本小于0.25时，指定panel=True 表示让数据可以返回panel格式；

当本地pandas版本>0.25 或 指定panel = False时，数据将返回dataframe格式；

fields字段说明

字段名称	中文名称	注释（特殊说明）

open	时间段开始时价格

close	时间段结束时价格

low	时间段中的最低价

high	时间段中的最高价

volume	时间段中的成交的标的数量

money	时间段中的成交的金额

factor	复权因子	复权选项仅针对股票和基金品种有效，对于指数而言，复权因子被固定设置为1/td>

high_limit	指定交易日的当日涨停价

low_limit	指定交易日的当日跌停价

avg	时间段中的平均价	成交额除以成交量；

pre_close	前一个单位时间结束时的价格,按天则是前一天的收盘价	前一个单位时间结束时的价格。
注：按天来获取的话，股票、指数、基金、可转债的是前一天的收盘价。（新股上市为IPO发行价，股票出现分红，则pre close是指昨天收盘价带分红拆股调整后的价格）;
期货/期权是前一天的结算价；

paused	bool值,股票是否停牌;	当paused=1，即停牌时 open/close/low/high/pre_close 都等于停牌前的收盘价, volume=money=0

open_interest	持仓量	期货/期权品种的持仓量

示例：

返回dataframe

df =get_price(['JQ0001.SPI','JQ0002.SPI'], start_date= '2024-01-29 09:30:00',end_date='2024-01-29 09:32:00', frequency='1m', fields=['open','close','low','high','volume','money','high_limit','low_limit','avg','pre_close'])
print(df)

time        code       open      close        low       high  \
0 2024-01-29 09:31:00  JQ0001.SPI  2666.3434  2665.2402  2664.5472  2668.9695
1 2024-01-29 09:32:00  JQ0001.SPI  2665.6406  2674.3320  2665.6406  2674.3320
2 2024-01-29 09:31:00  JQ0002.SPI  3852.0857  3848.8644  3847.8467  3855.6814
3 2024-01-29 09:32:00  JQ0002.SPI  3849.6311  3862.6013  3849.6311  3862.6072

volume         money  high_limit  low_limit        avg  pre_close
0  3.670972e+09  3.482636e+10   2932.9777  2399.7091  2032.3485  2660.6427
1  1.612269e+09  1.677370e+10   2932.9777  2399.7091  2670.9946  2665.2402
2  1.842943e+09  1.532560e+10   4237.2943  3466.8771  2931.1753  3843.4875
3  7.824228e+08  6.945955e+09   4237.2943  3466.8771  3857.7282  3848.8644
