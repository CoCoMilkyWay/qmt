# ARCH — 多周期因子系统: 验收标准与算法

目标形态: 多层因子库 (1m / 15m / 1h / 隔夜 / 1d / 5d / 15d) → 少数独立账本 → 单一中央执行器, A 股现货纯多头.
本文定义三个**统一算法骨架**, 全系统所有层级共用, 层级间只有容量与参数的差异 (单独一节):

- **骨架 A — 因子验收协议**: 一切因子, 无论周期, 走同一套统计协议 (§2).
- **骨架 B — 预测 = 惩罚经验风险最小化**: 一切账本的预测模型是同一个 ERM 问题, 容量由有效样本量定 (§3).
- **骨架 C — 决策 = 凸成本近端问题**: 组合、聚合、执行是同一个"目标改进 vs 凸交易成本"的近端优化, 只是目标向量不同 (§4, §5).

---

## 1. 约束、原则与记号

**硬约束**: T+1 (当日买入不可卖出) / 涨跌停停牌 (意图 ≠ 成交) / 满仓 (无择时, alpha 纯截面) / 纯多头 (负 alpha 只能表达为不持有) / 100 股整数手.

**原则** (每个设计可追溯到其一):

- **P1** 参数预算跟*有效*样本量走, 不跟名义样本量走.
- **P2** 现状免费, 改变收费: 举证责任永远在"交易"一侧.
- **P3** 拟合与验收分离: 独立样本不足的结构只允许数据行使否决权, 不允许数据选参数.
- **P4** 周期聚合在仓位空间 (账本相加), 不在信号空间 (无统一多周期模型).
- **P5** 单一货币: 一切预测校准为 bps; rank/z 跨周期不可加, bps 可加.

**记号**:

| 记号                            | 含义                                                                 |
| ------------------------------- | -------------------------------------------------------------------- |
| $i \in \{1..N\}$, $t$, $k$, $b$ | 标的, 决策时点 (bar/日), 因子, 账本                                  |
| $z_{k,i}(t)$                    | 因子 k 的截面标准分 (因子值可 winsorize; 方向不预设, 由 IC 符号携带) |
| $y_i^{(h)}(t)$                  | 前瞻 h 期收益 label (见 §2.1, **永不 winsorize**)                    |
| $W_b$                           | 账本 b 的交易 horizon (= 其模型的 label 期)                          |
| $\mu_{b,i}(t)$                  | 账本 b 对 i 的校准后预期超额 (bps, horizon $W_b$)                    |
| $w_{b,i}$, $c_b$, $W_i$         | 账本目标权重, 账本资金配比, 聚合目标权重 $W_i=\sum_b c_b w_{b,i}$    |
| $\mathrm{ADV}_i$, $\sigma_i$    | 日均成交额, 日波动率                                                 |

---

## 2. 骨架 A — 因子验收协议

一切因子入库走同一协议, 产出一张档案卡. 协议的每一步都有明确的统计动机.

### 2.1 label 定义

$$
y_i^{(h)}(t) = \frac{P_i(t_{\mathrm{exec}}+h)}{P_i(t_{\mathrm{exec}})} - 1,
\qquad t_{\mathrm{exec}} = t \text{ 之后第一个可执行时点}
$$

三条纪律:

1. **含执行滞后**: 分钟级 $t_{\mathrm{exec}} = t+1$ bar (决策 bar 关闭后才可下单), 日级 = 决策日收盘. 消除同 bar 前视.
2. **永不 winsorize**: 本策略域的 alpha 大量由右尾跳跃承载 (长期横盘 + 突发重估); 截掉 label 右尾等于把要赚的钱从测量里删除. 稳健性由秩统计承担 (下条), 不由截尾承担.
3. **跳跃不单独建模** (P3): 收益过程视为漂移 + 到达时间随机的跳跃, $\mathbb{E}[y^{(h)}] \approx \mathrm{drift}(h) + p(h)\cdot J$. 独立跳跃样本太少, 只测无条件均值 (IC / 分位收益, 跳跃留在均值里), 不拟合 $p(h)$ 的条件模型.

### 2.2 IC 曲线与误差棒

$$
\mathrm{IC}_k(h) = \frac{1}{T}\sum_t \rho_S\!\left(z_{k,\cdot}(t),\, y_\cdot^{(h)}(t)\right)
\qquad h \in \mathcal{H} = \{1, 5, 20, 60\}\ \text{日 (日内库: 相应 bar 梯子)}
$$

$\rho_S$ = Spearman 截面秩相关 — 选秩而非 Pearson 的原因即 2.1(2): label 带重尾, 秩统计对跳跃稳健且无需截尾.

**误差棒 (处理时间自相关)**: 日度 IC 序列 $\{\mathrm{IC}_k(h; t)\}_t$ 高度自相关 (label 窗口重叠 + 因子持续), 朴素 $\mathrm{se} = \hat\sigma/\sqrt{T}$ 严重低估. 用循环块 bootstrap:

$$
\mathrm{se}_k(h) = \mathrm{std}_{B}\!\left[\overline{\mathrm{IC}}^{(b)}_k(h)\right],
\qquad \text{块长 } \ell = 2h,\ B \ge 1000
$$

### 2.3 收缩 (经验贝叶斯, 防全库多重检验)

层级模型 $\widehat{\mathrm{IC}}_k \sim \mathcal N(\theta_k, \mathrm{se}_k^2),\ \theta_k \sim \mathcal N(0, \tau^2)$, 后验均值:

$$
\mathrm{IC}^*_k(h) = \frac{\tau^2(h)}{\tau^2(h) + \mathrm{se}_k^2(h)}\;\mathrm{IC}_k(h),
\qquad
\hat\tau^2(h) = \max\!\Big(0,\ \mathrm{Var}_k[\mathrm{IC}_k(h)] - \overline{\mathrm{se}^2(h)}\Big)
$$

性质: 样本少 / 噪声大的因子被自动压向 0; 全库一起估 $\tau^2$, 隐式做了多重检验修正 — 挖出 500 个候选时, 单因子 t=2 的"显著"大部分会被收缩抹平, 这正是想要的.

### 2.4 增量性 (共线性在准入处解决)

候选 $z_k$ 对现有库 $Z_{-k}$ 做截面回归取残差:

$$
z_k^{\perp}(t) = z_k(t) - Z_{-k}(t)\,\hat\delta(t),
\qquad \hat\delta = \arg\min_\delta \| z_k - Z_{-k}\delta \|^2
$$

**准入检验作用在 $\mathrm{IC}^{\perp}_k$ (残差 IC) 上**: 与现有库高度共线的候选, 残差 IC 自动归零, 进不了库. 这是 spanning test 的截面版 — 比"入库后靠 $\Sigma^{-1}$ 消化共线"更早、更省: 库保持近正交, 下游回归条件数好, 归因干净.

### 2.5 持续性与换手先验

$$
\rho_k(h) = \mathbb{E}_t\!\left[\frac{|\mathrm{Top}_q(t) \cap \mathrm{Top}_q(t+h)|}{q}\right],
\qquad
\tau_k(h) \approx \frac{1-\rho_k(h)}{h} \ \text{(每日单边换手率)}
$$

半衰期由 $\mathrm{IC}_k(h) \approx \mathrm{IC}_k(1)\,e^{-h/H_k}$ 拟合. **$H_k$ 与 $\rho_k$ 只作路由先验与换手预算, 不进任何模型** — 半衰期是描述量不是参数.

### 2.6 horizon 匹配判据 (因子该被哪个账本消费)

因子 k 在持有期 h 下的费前/费后单位化收益:

$$
\bar\alpha_k(h) = \frac{1}{h}\sum_{s=1}^{h} \mathrm{IC}^*_k(s)\,\sigma_{CS}(s),
\qquad
\mathrm{netSR}_k(h) \propto \frac{\bar\alpha_k(h) - c\,\tau_k(h)}{\sigma_{\mathrm{top}}}
$$

($\sigma_{CS}(s)$ = s-期收益截面离散度, $c$ = 往返成本.) $h^*_k = \arg\max_h \mathrm{netSR}_k(h)$ 决定路由: 快衰减因子在费后曲线上峰值靠左且可能全程为负 (费前漂亮费后不可交易), 在准入阶段即暴露 — 解决"筛选层排序权在费前 h=1 手里"的系统性偏差.

### 2.7 准入线汇总

$$
\text{入库} \iff
\underbrace{\frac{|\mathrm{IC}^{*\perp}_k(W_b)|}{\mathrm{se}_k(W_b)} \ge 2}_{\text{目标账本 horizon 上, 残差+收缩后}}
\ \wedge\
\underbrace{\max_h \mathrm{netSR}_k(h) > 0}_{\text{费后存在可交易 horizon}}
\ \wedge\
\underbrace{\Delta\mathrm{PnL} \succ 0}_{\text{§6, 终审}}
$$

跳跃型因子附加人工审阅事件曲线 $\mathrm{CAR}_k(\tau) = \mathbb{E}[\text{入选后 } \tau \text{ 日累计超额}]$ (只审形态, 不设自动阈值 — P3).

---

## 3. 骨架 B — 预测: 统一的惩罚 ERM

所有账本的预测模型是同一个问题:

$$
\hat f_b = \arg\min_{f \in \mathcal{F}_b}\ \sum_{i,t} u_{it}\; L\!\big(y_i^{(W_b)}(t),\ f(\mathbf{z}_i(t))\big) \;+\; \Omega_b(f)
$$

四个组件全系统统一, 只有容量 $\mathcal{F}_b$ 分级:

**(1) 有效样本量决定容量 (P1 的定量形式)**. 截面相关 $\bar\rho$ 与 label 重叠使名义样本量虚高:

$$
n_{\mathrm{eff}} \approx \underbrace{\frac{N}{1+(N-1)\bar\rho}}_{N_{\mathrm{eff}}\text{: 截面}} \times \underbrace{\frac{T}{W_b}}_{\text{非重叠窗口数}}
$$

| $n_{\mathrm{eff}}$ | $\mathcal{F}_b$       | 对应账本      |
| ------------------ | --------------------- | ------------- |
| $\lesssim 10^4$    | 线性 (岭/GLS, 强收缩) | 5d / 15d 慢层 |
| $10^4 \sim 10^6$   | 线性或浅 GBDT         | 1d / 隔夜     |
| $\gtrsim 10^6$     | GBDT (全局一套超参)   | 1m / 15m / 1h |

**(2) 线性容量的闭式解 = "Grinold 是同一算法的最深先验版"**. 平方损失 + 截面标准化特征下, 岭回归闭式:

$$
\hat\beta = (R_z + \gamma I)^{-1}\, \mathbf{IC}^* \,\sigma_y,
\qquad
\hat\mu_i = \sigma_{CS}(W_b) \cdot \big[(R_z+\gamma I)^{-1}\mathbf{IC}^*\big]^\top \mathbf{z}_i
$$

($R_z$: 因子相关阵, Ledoit-Wolf 收缩.) 单因子正交特例退化为 $\alpha = \mathrm{IC}\cdot\sigma\cdot z$ — 即教科书 Grinold. 所以"Grinold vs 监督模型"是伪对立: 同一 ERM, 先验强度不同. 裸 Grinold (朴素相加) 等价于假设 $R_z = I$, 是错误的; $(R_z+\gamma I)^{-1}$ 是共线性在预测层的第二道防线 (第一道在 §2.4 准入).

**(3) 样本权重与 CV 协议 (自相关的全部处理, 与模型无关)**:

$$
u_{it} = \frac{1}{\#\{\,t' :\ [t', t'+W_b] \cap [t, t+W_b] \ne \emptyset\,\}}
\quad \text{(uniqueness 权重; 或等价地按 stride} = W_b \text{ 抽样)}
$$

purged walk-forward, 全层级同协议, 参数随 label 缩放:

$$
\underbrace{[t_0,\ t_1 - W_b)}_{\text{train}}\ \to\ \underbrace{[t_1-W_b,\ t_1)}_{\text{purge}}\ \to\ \underbrace{[t_1,\ t_2)}_{\text{test}}\ \to\ \underbrace{[t_2,\ t_2+e)}_{\text{embargo}}
$$

按时间整块分折 (同时段全截面同折, 禁随机打散). 分钟库上 uniqueness/stride 比 purge 更重要 — 相邻 bar 的 label 近乎全重叠, 不降权则 CV 分数虚高 1–2 个数量级, 这是高频层最大的隐性过拟合源.

**(4) bps 校准 (P5)**: 每账本滚动回归 $y = a + b\hat\mu$, 要求 $b \in [0.8, 1.2]$, 超界用 $\hat b$ 重标定. 校准后各账本输出同为"该 horizon 预期超额 bps", 跨账本可加可比.

**多周期聚合的正面回答**: 不需要期限核. 每账本 label 锚定自己实际交易的 $W_b$, 各速度特征全喂 (路由先验 $H_k \in [W_b/5,\ 20W_b]$ 筛), 让 ERM 自己学载荷; 跨周期在 §5 仓位空间求和. 统一多周期信号模型 (多任务/期限结构核) 理论上限更高, 但跨周期交互的增益买不回它的复杂度税与验收难度 — 这是 P4 的成本收益判断, 不是数学不可行.

---

## 4. 骨架 C — 决策: 统一的凸成本近端问题

组合 (账本内)、执行 (中央) 是**同一个优化问题**, 差别只在目标向量与约束集:

$$
\Delta^* = \arg\max_{\Delta}\ \ g^\top \Delta\ -\ \frac{\lambda}{2}\,\Delta^\top \Sigma\, \Delta\ -\ \underbrace{\sum_i \Big[c_{0,i}\,|\Delta_i| + c_{1,i}\,|\Delta_i|^{3/2}\Big]}_{C(\Delta):\ \text{线性费+平方根冲击}}\ -\ \underbrace{\kappa \sum_i \mathrm{se}_i\,|\Delta_i|}_{\text{不确定性罚}}
\quad \text{s.t.}\ \Delta \in \mathcal{K}
$$

- 组合层: $g_i = \mu_i - \lambda(\Sigma w)_i$ (边际效用), $\mathcal{K}$ = {多头, 满仓, 上限}.
- 执行层: $g_i = u_i \cdot \mathrm{dev}_i$ (紧迫度×目标偏差), $\mathcal{K}$ = {T+1 可卖库存, 手数, 涨跌停, 参与率}.

成本系数来自平方根冲击律 (全系统唯一成本模型, 实盘残差持续回归校准 $\eta$):

$$
\mathrm{cost}(Q) = \mathrm{fees}\cdot Q + \tfrac{\mathrm{spread}}{2}\, Q + \eta\,\sigma_i\,\sqrt{\tfrac{Q}{\mathrm{ADV}_i}}\cdot Q
\ \Rightarrow\
c_{0,i} = \mathrm{fees} + \tfrac{\mathrm{spread}_i}{2},\quad
c_{1,i} = \eta\,\sigma_i\sqrt{\tfrac{PV}{\mathrm{ADV}_i}}
$$

这个形式的三条一阶性质, 每条都是此前争点的解 (推导即答案):

**(i) 不交易带内生 (缓冲机制的严格化)**. $\Delta_i = 0$ 处的次梯度条件给出:

$$
|g_i| \le c_{0,i} + \kappa\,\mathrm{se}_i \ \Longrightarrow\ \Delta_i^* = 0
$$

滞回带 = 线性成本 + 不确定性, **不再是自由参数**: 截面预测离散度大时 $|g|$ 易越阈 (该动), 信号打架时 $\mathrm{se}$ 大 (自动粘滞现状, P2). 固定比例缓冲带 (如 rank 出带才卖) 是本式在"等权槽 + $g$ 用排名差近似"下的零阶特例.

**(ii) 部分调仓是普通输出**. 越阈后的内点一阶条件:

$$
|g_i| = c_{0,i} + \kappa\,\mathrm{se}_i + \tfrac{3}{2}c_{1,i}\,|\Delta_i^*|^{1/2}
\ \Longrightarrow\
|\Delta_i^*| = \left[\frac{2\,(|g_i| - c_{0,i} - \kappa\,\mathrm{se}_i)}{3\,c_{1,i}}\right]^{2}
$$

"高盈亏比高方差信号 vs 止盈信号打架时移一半"由三个机制共同给出: 净 $\mu$ 缩小 (信号相加)、$\mathrm{se}$ 增大 (分歧罚)、$c_1$ 凸性 (薄流动性拆单). 全有全无是等权槽离散化的伪像. 已知偏差: 对正偏跳跃仓位, $\Sigma$ 罚项惩罚上行离散度, MV 系统性过度削减 — 接受之, 因为偏度不可估 (P3), 修正它需要不存在的样本.

**(iii) 机会成本自动入式 (防"提前止盈切碎趋势")**. 持仓 A 的边际效用 $g_A$ 用 A **当前因子值**在剩余 horizon 上的 $\mu_A$ 计算, 与持有历史无关. 涨后排名回落但剩余 $\mu_A$ 仍厚的持仓, 无候选付得起 $\mu_B - \mu_A > $ 门槛, 自然续持. 短周期信号想触发卖出, 必须其对 $\mu_A$ 的负贡献在 $W$ 积分意义上压倒长端 — 短端积分小, 天然无权 (对应 $\Delta\mathrm{PnL}$ 语言: 信号按"相对不动的边际价值"计价).

**离散特例 (N 槽等权账本)**: 可行移动限于槽交换 $\Delta = \frac{1}{N}(e_B - e_A)$, 代入同一目标:

$$
\text{换 } A\!\to\!B \iff
\mu_B - \mu_A > N\big[C(\tfrac1N) \big] + \kappa\,(\mathrm{se}_A + \mathrm{se}_B)
$$

即"成本门槛 + 信心门槛"双条件交换规则 — 与连续版是同一个式子在不同约束集 $\mathcal{K}$ 上的解.

---

## 5. 聚合与资金配比

**聚合** (P4, 无参数): $W_i = \sum_b c_b\, w_{b,i}$. 账本互不可见; 反向意图在目标层抵消 (净额化), 不出市场不付成本. 成交与成本按净单贡献比例摊回各账本虚拟台账 (成本内化 — 每账本考核含自己引发的成本); 真实持仓 = Σ 台账 + 取整尾差, 尾差由台账吸收.

**资金配比** = 跨账本唯一自由向量, 账本收益空间的小型 MVO:

$$
c^* \propto \Omega^{-1}\mathbf{s}^+,
\qquad \mathbf{s}^+ = \max(0, \text{账本费后夏普 (shadow book 实测)}),\ \Omega = \text{账本收益相关阵}
$$

季度更新, 单次变动上限 ±20%, 新账本从 $c_b \le 5\%$ 起步 — 配比自身不允许成为过拟合面.

**T+1 下的层级分工** (从约束推出, 非设计偏好): 半衰期 < 1d 的信号买入侧必然承诺隔夜 ⇒ 它们不能独立开仓, 只能 (a) 在慢账本的交易日改善执行价, (b) 操作昨仓 (可卖库存) 做日内替换. 即日内账本的合法域 = 老库存, 这在 $\mathcal{K}$ 的 $x_i \ge -\mathrm{sellable}_i$ 约束里自动表达, 不需要额外规则.

---

## 6. 验收: ΔPnL 唯一准入 + 事件规则协议

**ΔPnL 终审** (一切组件 X — 因子/账本/规则/参数变更 — 的统一大门):

$$
\text{接纳 } X \iff
\mathrm{SR}_{\mathrm{net}}(\text{系统}+X) - \mathrm{SR}_{\mathrm{net}}(\text{系统}) > 0
\ \text{于 walk-forward, 块 bootstrap } p < 0.1
$$

IC / 单测夏普 / 胜率只是诊断量. 这道门专门挡"高 IC 负贡献"型信号 (把趋势切碎、换手费后归负、与存量冗余).

**shadow book 阶梯**: 慢 → +中 → +隔夜 → +日内 → +规则, 逐档边际夏普常开监控; 边际持续为负 ⇒ 降配比至下线.

**事件规则层** (可观测状态触发的执行 override, 如涨跌停处置): 参数由业务先验定死, 不搜索 (P3); 验收 = 事件对齐 CAR 的 on/off 对比 + 块 bootstrap 置信区间 + 全局 ΔPnL. 数据否决 ⇒ 规则下线; **不允许改参重试** (那是变相拟合). 规则不进模型不进因子 — 事件是条件策略, 不是截面信号.

**存续机制**: 因子滚动重估 $\mathrm{IC}^*$, 连续两季 $|\mathrm{IC}^*|/\mathrm{se} < 1$ 者经收缩自然趋零权重 (无手动 kill); 实盘 vs 回测的持仓偏差逐日分解为 {T+1 截断, 涨跌停, 流动性, 取整}, 出现无法归因的第五项即报警.

---

## 7. 层级差异表 (统一骨架下的全部差异)

|                        | 慢 (15d)               | 中 (1–5d)     | 隔夜          | 日内 (1m–1h)   |
| ---------------------- | ---------------------- | ------------- | ------------- | -------------- |
| label $W_b$            | 10–15d                 | 1–3d          | close→open    | 数 bar~数小时  |
| $n_{\mathrm{eff}}$     | $\sim 10^{2\text{-}3}$ | $\sim 10^{4}$ | $\sim 10^{5}$ | $\ge 10^{6}$   |
| $\mathcal{F}_b$ (B)    | 线性+双收缩            | 线性/浅 GBDT  | GBDT          | GBDT           |
| purge / stride (B)     | $W_b$ / $W_b$          | 同左          | 1d / 1d       | $W_b$ bar / 同 |
| $\Sigma$ 粒度 (C)      | 对角+行业块            | 同左          | 对角          | 对角           |
| $\mathcal{K}$ 特有 (C) | —                      | —             | 尾盘建仓窗    | 仅可卖库存     |
| 紧迫度 $u$ (执行)      | 多日 TWAP/被动         | 1–2 日        | 尾盘时段      | 当 bar 限价追  |
| 开仓权                 | 有                     | 有            | 有 (尾盘)     | **无** (T+1)   |

表外无其他差异: 验收协议 (A)、ERM 形式与 CV (B)、决策目标与成本模型 (C) 全层级同一.

---

## 8. 争点决议索引 (本文对先前设计问题的正面回应)

| 争点                           | 决议                                                                              | 机制所在    |
| ------------------------------ | --------------------------------------------------------------------------------- | ----------- |
| 多周期聚合要不要期限核         | 不要: label 锚定各账本 $W_b$, 仓位空间求和                                        | §3(尾), §5  |
| 缓冲带/exit 比例是否保留       | 机制保留、参数退役: 不交易带由成本+不确定性内生                                   | §4(i)       |
| 全换还是换一半                 | 内点解公式, 部分调仓是普通输出                                                    | §4(ii)      |
| 高方差信号 vs 止盈信号打架     | $\mathrm{se}$ 罚项 ⇒ 分歧自动粘滞现状; MV 对正偏彩票过度保守, 接受 (P3)           | §4(ii)      |
| 提前止盈切碎趋势               | 机会成本以持仓现值 $\mu_A$ 入边际效用, 短端积分无权                               | §4(iii)     |
| Grinold vs 监督模型            | 同一 ERM 的先验强度谱: 岭闭式 $\supset$ Grinold; 容量由 $n_{\mathrm{eff}}$ 定     | §3(1)(2)    |
| 共线性                         | 两道防线: 准入残差 IC (spanning) + $(R_z+\gamma I)^{-1}$                          | §2.4, §3(2) |
| 自相关 (30d 因子 per-day 有值) | 预测端不处理 (低换手是特性); 推断端块 bootstrap + uniqueness + purge              | §2.2, §3(3) |
| 跳跃/彩票收益                  | label 不截尾 + Spearman + 事件曲线人审; 不拟合到达时间                            | §2.1, §2.7  |
| 费前 h=1 筛选偏差              | 准入即测 $\mathrm{netSR}_k(h)$ 全曲线, 费后无可交易 horizon 者不入库              | §2.6        |
| 快信号在 T+1 多头下的地位      | 无开仓权, 由可卖库存约束自动表达; 变现 = 执行价 + 老仓替换                        | §5(尾), §7  |
| 过拟合总预算                   | $n_{\mathrm{eff}}$ 定容量 + 收缩 + ΔPnL 终审 + 全局参数 ≈ 15 个且 per-factor 为 0 | §3(1), §6   |

**全局自由参数**: $\gamma$ (岭), $\lambda$ (风险厌恶), $\kappa$ (信心门槛), $\eta$ (冲击, 自校准), $\pi$ (参与率), $c_b$ (公式+限速), IC 窗口, GBDT 全局超参一套. 新设计要求新增参数时, 必须在此登记并给出定法.

---

## 9. 落地顺序 (每阶段独立验收, 前序不返工)

1. **档案卡基建** (§2 全量跑存量因子): 纯测量, 零行为变更. 产出 IC(h)/ρ(h)/netSR(h)/事件曲线.
2. **慢账本对齐**: 权重评价 label 换到 $W$ 含成本罚; $(R_z+\gamma I)^{-1}\mathbf{IC}^*$ 闭式与既有稳健搜索互为对照, 方向应一致.
3. **决策规则升级**: §4 双门槛+内点数量替换固定带, 旧版留作 baseline, ΔPnL 验收.
4. **执行跟踪化**: 目标追踪 + 可卖库存台账 + 冲击模型冷启动 ($\eta = 0.5$), 实盘成交回填开始积累.
5. **隔夜账本**: 首个 GBDT 账本, $c_b \le 5\%$ 起步, 走完整 §6 准入.
6. **日内账本**: 最后; 依赖 4 的执行成熟与 5 的 CV 卫生经验; 合法域 = 老库存.
