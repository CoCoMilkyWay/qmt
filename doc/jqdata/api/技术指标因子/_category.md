技术指标

from jqdatasdk.technical_analysis import *

更新方式

技术指标 —— 2005至今,支持设置复权基准日（fq_ref_date 参数），实时计算

详细介绍：函数计算公式、API 调用方法，用法注释， 输入输出值详情请见:[数据 - 技术分析指标]

技术指标汇总

类型	指标	类型	指标	类型	指标	类型	指标

超买超卖型	ACCER-幅度涨速
ADTM-动态买卖气指标
ATR-真实波幅
BIAS-乖离率
BIAS_QL-乖离率_传统版
BIAS_36-三六乖离
CCI-商品路径指标
CYF-市场能量
DKX-多空线
KD-随机指标KD
KDJ-随机指标
SKDJ-慢速随机指标
MFI-资金流量指标
MTM-动量线
ROC-变动率指标
RSI-相对强弱指标
MARSI-相对强弱平均线
OSC-变动速率线
UDL-引力线
WR-威廉指标
LWR-LWR威廉指标
TAPI-加权指数成交值
FSL-分水岭
趋势型	CHO-佳庆指标
CYE-市场趋势
DBQR-对比强弱
DMA-平均差
DMI - 趋向指标
DPO-区间震荡线
EMV-简易波动指标
GDX-鬼道线
JLHB-绝路航标
JS-加速线
MACD-平滑异同平均
QACD-快速异同平均
QR-强弱指标
TRIX-终极指标
UOS-终极指标
VMACD-量平滑移动平均
VPT-量价曲线
WVAD-威廉变异离散量
能量型	BRAR-情绪指标
CR-带状能量线
CYR-市场强弱
MASS-梅斯线
PCNT-幅度比
PSY-心理线
VR-成交量变异率
成交量型	AMO-成交金额
CCL-持仓量（适用于期货）
DBLB-对比量比
DBQRV-对比强弱量
OBV-累积能量线
VOL-成交量
VRSI-相对强弱量
LB-量比

均线型	AMV-成本价均线
ALLIGAT-鳄鱼线
BBI-多空均线
EXPMA-指数平均线
BBIBOLL-多空布林线
MA-均线
HMA-高价平均线
LMA-低价平均线
VMA-变异平均线
路径型	BOLL-布林线
ENE-轨道线
MIKE-麦克支撑压力
PBX-瀑布线
XS-薛斯通道
XS2-薛斯通道2
其他型	EMA-指数移动平均
SMA-移动平均
BDZX-波段之星
CDP-STD-逆势操作
CJDX-超级短线
CYHT-财运亨通
JAX-济安线
JFZX-飓风智能中线
JYJL-交易参考均量
LHXJ-猎狐先觉
LYJH-猎鹰歼狐
TBP-STD-趋势平衡点
ZBCD-准备抄底
特色型	AROON-阿隆指标
CFJT-财富阶梯
ZSDB-指数对比

神系	SG-SMX-生命线
SG-LB-量比
SG-PF-强势股评分
XDT-心电图
龙系	ZLMM-主力买卖
RAD-威力雷达
SHT-龙系短线
鬼系	CYW-主力控盘
CYS-市场盈亏
图表型	ZX-重心线
PUCU-逆时钟曲线

示例：

# 导入技术分析指标库
from jqdatasdk import *
from jqdatasdk.technical_analysis import *

# 定义股票池列表
security_list1 = '000001.XSHE'
security_list2 = ['000001.XSHE','000002.XSHE','601211.XSHG','603177.XSHG']

# 计算并输出 security_list1 的 GDX 值，分别返回：济安线、压力线和支撑线的值。
gdx_jax, gdx_ylx, gdx_zcx = GDX(security_list1,check_date='2017-01-04', N = 30, M = 9)
print (gdx_jax[security_list1])
print (gdx_ylx[security_list1])
print (gdx_zcx[security_list1])

# 输出 security_list2 的 GDX 值
gdx_jax, gdx_ylx, gdx_zcx = GDX(security_list2,check_date='2017-01-04', N = 30, M = 9)
for stock in security_list2:
print (gdx_jax[stock])
print (gdx_ylx[stock])
print (gdx_zcx[stock])

8.29159727600139
9.037841030841516
7.545353521161265
8.29159727600139
9.037841030841516
7.545353521161265
15.528491778666732
16.92605603874674
14.130927518586727
15.359770622508778
16.74214997853457
13.977391266482988
nan
nan
nan
