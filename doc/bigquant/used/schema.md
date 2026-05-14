
========== trading_days ==========
   column_name   column_type null   key default extra
0         date  TIMESTAMP_NS  YES  None    None  None
1  market_code       VARCHAR  YES  None    None  None

========== holidays ==========
   column_name   column_type null   key default extra
0         date  TIMESTAMP_NS  YES  None    None  None
1  market_code       VARCHAR  YES  None    None  None

========== cn_stock_instruments ==========
  column_name   column_type null   key default extra
0        date  TIMESTAMP_NS  YES  None    None  None
1  instrument       VARCHAR  YES  None    None  None
2        name       VARCHAR  YES  None    None  None
3        type       VARCHAR  YES  None    None  None

========== cn_index_instruments ==========
  column_name   column_type null   key default extra
0        date  TIMESTAMP_NS  YES  None    None  None
1  instrument       VARCHAR  YES  None    None  None
2        name       VARCHAR  YES  None    None  None

========== cn_fund_instruments ==========
  column_name   column_type null   key default extra
0        date  TIMESTAMP_NS  YES  None    None  None
1  instrument       VARCHAR  YES  None    None  None
2        name       VARCHAR  YES  None    None  None

========== cn_future_instruments ==========
    column_name   column_type null   key default extra
0          date  TIMESTAMP_NS  YES  None    None  None
1    instrument       VARCHAR  YES  None    None  None
2          name       VARCHAR  YES  None    None  None
3  trading_code       VARCHAR  YES  None    None  None
4  product_code       VARCHAR  YES  None    None  None

========== cn_cbond_instruments ==========
  column_name   column_type null   key default extra
0        date  TIMESTAMP_NS  YES  None    None  None
1  instrument       VARCHAR  YES  None    None  None
2        name       VARCHAR  YES  None    None  None

========== cn_stock_industry_component ==========
             column_name   column_type null   key default extra
0                   date  TIMESTAMP_NS  YES  None    None  None
1             instrument       VARCHAR  YES  None    None  None
2               industry       VARCHAR  YES  None    None  None
3          industry_name       VARCHAR  YES  None    None  None
4    industry_instrument       VARCHAR  YES  None    None  None
5   industry_level1_code       VARCHAR  YES  None    None  None
6   industry_level1_name       VARCHAR  YES  None    None  None
7   industry_level2_code       VARCHAR  YES  None    None  None
8   industry_level2_name       VARCHAR  YES  None    None  None
9   industry_level3_code       VARCHAR  YES  None    None  None
10  industry_level3_name       VARCHAR  YES  None    None  None

========== cn_stock_industry_bar1d ==========
     column_name   column_type null   key default extra
0           date  TIMESTAMP_NS  YES  None    None  None
1     instrument       VARCHAR  YES  None    None  None
2         method       TINYINT  YES  None    None  None
3      pre_close        DOUBLE  YES  None    None  None
4           open        DOUBLE  YES  None    None  None
5          close        DOUBLE  YES  None    None  None
6           high        DOUBLE  YES  None    None  None
7            low        DOUBLE  YES  None    None  None
8         volume        BIGINT  YES  None    None  None
9    deal_number       INTEGER  YES  None    None  None
10        amount        DOUBLE  YES  None    None  None
11  change_ratio        DOUBLE  YES  None    None  None
12          turn        DOUBLE  YES  None    None  None

========== cn_stock_industry_valuation ==========
      column_name   column_type null   key default extra
0            date  TIMESTAMP_NS  YES  None    None  None
1      instrument       VARCHAR  YES  None    None  None
2        industry       VARCHAR  YES  None    None  None
3  industry_level       TINYINT  YES  None    None  None
4   industry_code       VARCHAR  YES  None    None  None
5   industry_name       VARCHAR  YES  None    None  None
6  component_nums       TINYINT  YES  None    None  None
7     pe_trailing        DOUBLE  YES  None    None  None
8          pe_ttm        DOUBLE  YES  None    None  None
9              pb        DOUBLE  YES  None    None  None

========== cn_stock_basic_info ==========
         column_name   column_type null   key default extra
0         instrument       VARCHAR  YES  None    None  None
1               name       VARCHAR  YES  None    None  None
2          full_name       VARCHAR  YES  None    None  None
3            en_name       VARCHAR  YES  None    None  None
4          used_name       VARCHAR  YES  None    None  None
5       instrument_b       VARCHAR  YES  None    None  None
6             name_b       VARCHAR  YES  None    None  None
7      instrument_hk       VARCHAR  YES  None    None  None
8            name_hk       VARCHAR  YES  None    None  None
9           exchange       VARCHAR  YES  None    None  None
10       list_sector       TINYINT  YES  None    None  None
11     security_type       VARCHAR  YES  None    None  None
12          industry       VARCHAR  YES  None    None  None
13       office_addr       VARCHAR  YES  None    None  None
14      regster_addr       VARCHAR  YES  None    None  None
15  register_captial        DOUBLE  YES  None    None  None
16    nums_employees        DOUBLE  YES  None    None  None
17     nums_managers        DOUBLE  YES  None    None  None
18          law_firm       VARCHAR  YES  None    None  None
19      account_firm       VARCHAR  YES  None    None  None
20           profile       VARCHAR  YES  None    None  None
21        estab_date  TIMESTAMP_NS  YES  None    None  None
22         list_date  TIMESTAMP_NS  YES  None    None  None
23       delist_date  TIMESTAMP_NS  YES  None    None  None
24       online_list  TIMESTAMP_NS  YES  None    None  None
25         ipo_price        DOUBLE  YES  None    None  None
26          ipo_nums        DOUBLE  YES  None    None  None
27            ipo_pe        DOUBLE  YES  None    None  None
28      ipo_parvalue        DOUBLE  YES  None    None  None
29  net_amount_funds        DOUBLE  YES  None    None  None
30      offline_dtor        DOUBLE  YES  None    None  None
31              dtor        DOUBLE  YES  None    None  None
32            f_open        DOUBLE  YES  None    None  None
33            f_high        DOUBLE  YES  None    None  None
34           f_close        DOUBLE  YES  None    None  None
35        f_turnover        DOUBLE  YES  None    None  None
36       corp_nature       VARCHAR  YES  None    None  None
37        corp_scale       VARCHAR  YES  None    None  None

========== cn_stock_capital ==========
       column_name   column_type null   key default extra
0       instrument       VARCHAR  YES  None    None  None
1     publish_date  TIMESTAMP_NS  YES  None    None  None
2      change_date  TIMESTAMP_NS  YES  None    None  None
3           reason       VARCHAR  YES  None    None  None
4     total_shares        DOUBLE  YES  None    None  None
5          float_a        DOUBLE  YES  None    None  None
6          float_b        DOUBLE  YES  None    None  None
7     restricted_a        DOUBLE  YES  None    None  None
8     restricted_b        DOUBLE  YES  None    None  None
9    prefer_shares        DOUBLE  YES  None    None  None
10       shares_hk        DOUBLE  YES  None    None  None
11   shares_aboard        DOUBLE  YES  None    None  None
12  shares_a_total        DOUBLE  YES  None    None  None
13  shares_b_total        DOUBLE  YES  None    None  None
14     float_total        DOUBLE  YES  None    None  None
15      rest_total        DOUBLE  YES  None    None  None

========== cn_stock_dividend ==========
       column_name   column_type null   key default extra
0             date  TIMESTAMP_NS  YES  None    None  None
1       instrument       VARCHAR  YES  None    None  None
2      report_date  TIMESTAMP_NS  YES  None    None  None
3     publish_date  TIMESTAMP_NS  YES  None    None  None
4       bonus_rate        DOUBLE  YES  None    None  None
5   conversed_rate        DOUBLE  YES  None    None  None
6  cash_before_tax        DOUBLE  YES  None    None  None
7   cash_after_tax        DOUBLE  YES  None    None  None
8    register_date  TIMESTAMP_NS  YES  None    None  None
9          ex_date  TIMESTAMP_NS  YES  None    None  None

========== cn_stock_allotment ==========
        column_name   column_type null   key default extra
0              date  TIMESTAMP_NS  YES  None    None  None
1        instrument       VARCHAR  YES  None    None  None
2      publish_date  TIMESTAMP_NS  YES  None    None  None
3   allotment_price        DOUBLE  YES  None    None  None
4    allotment_rate        DOUBLE  YES  None    None  None
5  allotment_shares        BIGINT  YES  None    None  None
6     register_date  TIMESTAMP_NS  YES  None    None  None
7      exright_date  TIMESTAMP_NS  YES  None    None  None
8    allot_listdate  TIMESTAMP_NS  YES  None    None  None

========== cn_stock_margin_trading_detail ==========
                              column_name   column_type null   key default extra
0                                    date  TIMESTAMP_NS  YES  None    None  None
1                              instrument       VARCHAR  YES  None    None  None
2                       financing_balance        DOUBLE  YES  None    None  None
3                      financing_quantity        DOUBLE  YES  None    None  None
4                 financing_balance_ratio        DOUBLE  YES  None    None  None
5                      financing_purchase        DOUBLE  YES  None    None  None
6             financing_purchase_quantity        DOUBLE  YES  None    None  None
7                     financing_repayment        DOUBLE  YES  None    None  None
8            financing_repayment_quantity        DOUBLE  YES  None    None  None
9                  financing_net_purchase        DOUBLE  YES  None    None  None
10        financing_net_purchase_quantity        DOUBLE  YES  None    None  None
11             securities_lending_balance        DOUBLE  YES  None    None  None
12            securities_lending_quantity        DOUBLE  YES  None    None  None
13               securities_lending_sales        DOUBLE  YES  None    None  None
14      securities_lending_sales_quantity        DOUBLE  YES  None    None  None
15           securities_lending_repayment        DOUBLE  YES  None    None  None
16  securities_lending_repayment_quantity        DOUBLE  YES  None    None  None
17           securities_lending_net_sales        DOUBLE  YES  None    None  None
18  securities_lending_net_sales_quantity        DOUBLE  YES  None    None  None
19                 margin_trading_balance        DOUBLE  YES  None    None  None
20                        lendable_shares        DOUBLE  YES  None    None  None
21                   lending_margin_stock        DOUBLE  YES  None    None  None

========== cn_stock_margin_trading_market ==========
                              column_name   column_type null   key default extra
0                                    date  TIMESTAMP_NS  YES  None    None  None
1                                  method       VARCHAR  YES  None    None  None
2                       financing_balance        DOUBLE  YES  None    None  None
3                      financing_quantity        DOUBLE  YES  None    None  None
4                 financing_balance_ratio        DOUBLE  YES  None    None  None
5                      financing_purchase        DOUBLE  YES  None    None  None
6             financing_purchase_quantity        DOUBLE  YES  None    None  None
7                     financing_repayment        DOUBLE  YES  None    None  None
8            financing_repayment_quantity        DOUBLE  YES  None    None  None
9                  financing_net_purchase        DOUBLE  YES  None    None  None
10        financing_net_purchase_quantity        DOUBLE  YES  None    None  None
11             securities_lending_balance        DOUBLE  YES  None    None  None
12            securities_lending_quantity        DOUBLE  YES  None    None  None
13               securities_lending_sales        DOUBLE  YES  None    None  None
14      securities_lending_sales_quantity        DOUBLE  YES  None    None  None
15           securities_lending_repayment        DOUBLE  YES  None    None  None
16  securities_lending_repayment_quantity        DOUBLE  YES  None    None  None
17           securities_lending_net_sales        DOUBLE  YES  None    None  None
18  securities_lending_net_sales_quantity        DOUBLE  YES  None    None  None
19                 margin_trading_balance        DOUBLE  YES  None    None  None

========== cn_stock_shareholder ==========
                              column_name   column_type null   key default extra
0                            publish_date  TIMESTAMP_NS  YES  None    None  None
1                              instrument       VARCHAR  YES  None    None  None
2                                end_date  TIMESTAMP_NS  YES  None    None  None
3                       total_shareholder        DOUBLE  YES  None    None  None
4                   total_shareholder_chg        DOUBLE  YES  None    None  None
5                           a_shareholder        DOUBLE  YES  None    None  None
6                       a_shareholder_chg        DOUBLE  YES  None    None  None
7             avg_share_per_account_total        DOUBLE  YES  None    None  None
8         avg_share_per_account_total_chg        DOUBLE  YES  None    None  None
9       avg_share_ratio_per_account_total        DOUBLE  YES  None    None  None
10  avg_share_ratio_per_account_total_chg        DOUBLE  YES  None    None  None
11            avg_share_per_account_float        DOUBLE  YES  None    None  None
12        avg_share_per_account_float_chg        DOUBLE  YES  None    None  None
13      avg_share_ratio_per_account_float        DOUBLE  YES  None    None  None
14  avg_share_ratio_per_account_float_chg        DOUBLE  YES  None    None  None

========== cn_stock_shares ==========
          column_name   column_type null   key default extra
0                date  TIMESTAMP_NS  YES  None    None  None
1          instrument       VARCHAR  YES  None    None  None
2        total_shares        DOUBLE  YES  None    None  None
3      a_float_shares        DOUBLE  YES  None    None  None
4   free_float_shares        DOUBLE  YES  None    None  None
5  total_float_shares        DOUBLE  YES  None    None  None

========== cn_stock_status ==========
          column_name   column_type null   key default extra
0                date  TIMESTAMP_NS  YES  None    None  None
1          instrument       VARCHAR  YES  None    None  None
2           st_status       TINYINT  YES  None    None  None
3     is_risk_warning       TINYINT  YES  None    None  None
4           suspended       TINYINT  YES  None    None  None
5  price_limit_status       TINYINT  YES  None    None  None
6                exdr       TINYINT  YES  None    None  None

========== cn_stock_suspend ==========
      column_name   column_type null   key default extra
0            date  TIMESTAMP_NS  YES  None    None  None
1      instrument       VARCHAR  YES  None    None  None
2  suspend_period        BIGINT  YES  None    None  None
3  suspend_reason       VARCHAR  YES  None    None  None

========== cn_stock_name_change ==========
  column_name   column_type null   key default extra
0  instrument       VARCHAR  YES  None    None  None
1  start_date  TIMESTAMP_NS  YES  None    None  None
2    end_date  TIMESTAMP_NS  YES  None    None  None
3        name       VARCHAR  YES  None    None  None

========== cn_stock_dragon_list ==========
          column_name   column_type null   key default extra
0                date  TIMESTAMP_NS  YES  None    None  None
1          instrument       VARCHAR  YES  None    None  None
2              reason       VARCHAR  YES  None    None  None
3               close        DOUBLE  YES  None    None  None
4        price_change        DOUBLE  YES  None    None  None
5      net_buy_amount        DOUBLE  YES  None    None  None
6          buy_amount        DOUBLE  YES  None    None  None
7         sell_amount        DOUBLE  YES  None    None  None
8         deal_amount        DOUBLE  YES  None    None  None
9   total_deal_amount        DOUBLE  YES  None    None  None
10      net_buy_ratio        DOUBLE  YES  None    None  None
11  deal_amount_ratio        DOUBLE  YES  None    None  None
12               turn        DOUBLE  YES  None    None  None
13   float_market_cap        DOUBLE  YES  None    None  None
14    price_change_1d        DOUBLE  YES  None    None  None
15    price_change_2d        DOUBLE  YES  None    None  None
16    price_change_5d        DOUBLE  YES  None    None  None

========== cn_stock_bar1d ==========
      column_name   column_type null   key default extra
0            date  TIMESTAMP_NS  YES  None    None  None
1      instrument       VARCHAR  YES  None    None  None
2            name       VARCHAR  YES  None    None  None
3   adjust_factor        DOUBLE  YES  None    None  None
4       pre_close        DOUBLE  YES  None    None  None
5            open        DOUBLE  YES  None    None  None
6           close        DOUBLE  YES  None    None  None
7            high        DOUBLE  YES  None    None  None
8             low        DOUBLE  YES  None    None  None
9          volume        BIGINT  YES  None    None  None
10    deal_number       INTEGER  YES  None    None  None
11         amount        DOUBLE  YES  None    None  None
12   change_ratio        DOUBLE  YES  None    None  None
13           turn        DOUBLE  YES  None    None  None
14    upper_limit        DOUBLE  YES  None    None  None
15    lower_limit        DOUBLE  YES  None    None  None

========== cn_stock_limit_price ==========
   column_name   column_type null   key default extra
0         date  TIMESTAMP_NS  YES  None    None  None
1   instrument       VARCHAR  YES  None    None  None
2  upper_limit        DOUBLE  YES  None    None  None
3  lower_limit        DOUBLE  YES  None    None  None

========== cn_stock_financial_changedate ==========
      column_name   column_type null   key default extra
0      instrument       VARCHAR  YES  None    None  None
1     report_date  TIMESTAMP_NS  YES  None    None  None
2      changedate       VARCHAR  YES  None    None  None
3  statement_type       VARCHAR  YES  None    None  None

========== cn_stock_financial_income_general_pit ==========
                                             column_name   column_type null   key default extra
0                                                   date  TIMESTAMP_NS  YES  None    None  None
1                                             instrument       VARCHAR  YES  None    None  None
2                                            report_date  TIMESTAMP_NS  YES  None    None  None
3                                       fs_quarter_index       TINYINT  YES  None    None  None
4                                            change_type       TINYINT  YES  None    None  None
5                        continuing_operation_net_profit        DOUBLE  YES  None    None  None
6                      discontinued_operation_net_profit        DOUBLE  YES  None    None  None
7                            othcom_income_cannt_reclass        DOUBLE  YES  None    None  None
8                                  othcom_income_reclass        DOUBLE  YES  None    None  None
9   income_derecognition_of_fin_assets_at_amortized_cost        DOUBLE  YES  None    None  None
10                        own_credit_risk_fair_value_chg        DOUBLE  YES  None    None  None
11                           expense_on_policy_dividends        DOUBLE  YES  None    None  None
12                                credit_impairment_loss        DOUBLE  YES  None    None  None
13                                   fair_value_chg_gain        DOUBLE  YES  None    None  None
14                                   other_cannt_reclass        DOUBLE  YES  None    None  None
15                                         other_reclass        DOUBLE  YES  None    None  None
16           credit_impairment_of_other_debt_investments        DOUBLE  YES  None    None  None
17                 other_debt_investments_fair_value_chg        DOUBLE  YES  None    None  None
18                                          other_income        DOUBLE  YES  None    None  None
19               other_equity_instruments_fair_value_chg        DOUBLE  YES  None    None  None
20                                  income_othcom_income        DOUBLE  YES  None    None  None
21                                            net_profit        DOUBLE  YES  None    None  None
22                             totbal_diff_of_net_profit        DOUBLE  YES  None    None  None
23                               spec_diff_of_net_profit        DOUBLE  YES  None    None  None
24                              net_income_of_open_hedge        DOUBLE  YES  None    None  None
25                           reinsurance_premium_expense        DOUBLE  YES  None    None  None
26                                        interest_costs        DOUBLE  YES  None    None  None
27                                       interest_income        DOUBLE  YES  None    None  None
28                                          total_profit        DOUBLE  YES  None    None  None
29                           totbal_diff_of_total_profit        DOUBLE  YES  None    None  None
30                             spec_diff_of_total_profit        DOUBLE  YES  None    None  None
31          available_for_sale_fin_assets_fair_value_chg        DOUBLE  YES  None    None  None
32                                             eps_basic        DOUBLE  YES  None    None  None
33           income_translation_diff_of_foreign_currency        DOUBLE  YES  None    None  None
34                    invest_income_of_jv_and_associates        DOUBLE  YES  None    None  None
35                                net_profit_to_minority        DOUBLE  YES  None    None  None
36                              insurance_premium_income        DOUBLE  YES  None    None  None
37                             othcom_income_to_minority        DOUBLE  YES  None    None  None
38                     net_profit_to_parent_shareholders        DOUBLE  YES  None    None  None
39     total_comprehensive_income_to_parent_shareholders        DOUBLE  YES  None    None  None
40                  othcom_income_to_parent_shareholders        DOUBLE  YES  None    None  None
41                                    income_tax_expense        DOUBLE  YES  None    None  None
42                              fee_and_commission_costs        DOUBLE  YES  None    None  None
43                             fee_and_commission_income        DOUBLE  YES  None    None  None
44                                         invest_income        DOUBLE  YES  None    None  None
45                       gains_or_losses_from_htm_to_afs        DOUBLE  YES  None    None  None
46                       net_amount_of_insurance_reserve        DOUBLE  YES  None    None  None
47       othcom_income_cannt_reclass_under_equity_method        DOUBLE  YES  None    None  None
48             othcom_income_reclass_under_equity_method        DOUBLE  YES  None    None  None
49                                         exchange_gain        DOUBLE  YES  None    None  None
50                                cashflow_hedge_reserve        DOUBLE  YES  None    None  None
51        effective_of_gains_or_losses_on_cashflow_hedge        DOUBLE  YES  None    None  None
52                      research_and_development_expense        DOUBLE  YES  None    None  None
53                                           eps_diluted        DOUBLE  YES  None    None  None
54                                      taxes_and_levies        DOUBLE  YES  None    None  None
55                                administrative_expense        DOUBLE  YES  None    None  None
56                            total_comprehensive_income        DOUBLE  YES  None    None  None
57                                      operating_profit        DOUBLE  YES  None    None  None
58                       totbal_diff_of_operating_profit        DOUBLE  YES  None    None  None
59                         spec_diff_of_operating_profit        DOUBLE  YES  None    None  None
60                                    nonoperating_costs        DOUBLE  YES  None    None  None
61                                   nonoperating_income        DOUBLE  YES  None    None  None
62                                 total_operating_costs        DOUBLE  YES  None    None  None
63                        totbal_diff_of_operating_costs        DOUBLE  YES  None    None  None
64                          spec_diff_of_operating_costs        DOUBLE  YES  None    None  None
65                               total_operating_revenue        DOUBLE  YES  None    None  None
66                      totbal_diff_of_operating_revenue        DOUBLE  YES  None    None  None
67                        spec_diff_of_operating_revenue        DOUBLE  YES  None    None  None
68                                       operating_costs        DOUBLE  YES  None    None  None
69                                     operating_revenue        DOUBLE  YES  None    None  None
70                                       finance_expense        DOUBLE  YES  None    None  None
71                                   fin_interest_income        DOUBLE  YES  None    None  None
72                                  fin_interest_expense        DOUBLE  YES  None    None  None
73                                 asset_impairment_loss        DOUBLE  YES  None    None  None
74                                 asset_disposal_income        DOUBLE  YES  None    None  None
75                             net_insurance_claims_paid        DOUBLE  YES  None    None  None
76                                            surrenders        DOUBLE  YES  None    None  None
77                                 chg_by_remeasurements        DOUBLE  YES  None    None  None
78              othcom_income_from_reclass_of_fin_assets        DOUBLE  YES  None    None  None
79                                        selling_epense        DOUBLE  YES  None    None  None
80                           noncurr_assets_dispose_gain        DOUBLE  YES  None    None  None
81                           noncurr_assets_dispose_loss        DOUBLE  YES  None    None  None

========== cn_stock_financial_cashflow_general_pit ==========
                                           column_name   column_type null   key default extra
0                                                 date  TIMESTAMP_NS  YES  None    None  None
1                                           instrument       VARCHAR  YES  None    None  None
2                                          report_date  TIMESTAMP_NS  YES  None    None  None
3                                     fs_quarter_index       TINYINT  YES  None    None  None
4                                          change_type       TINYINT  YES  None    None  None
5                            conv_corp_bonds_within_1y        DOUBLE  YES  None    None  None
6              netinc_in_insurance_deposits_and_invest        DOUBLE  YES  None    None  None
7             credit_impairment_loss_in_cashflow_sheet        DOUBLE  YES  None    None  None
8                             debt_transfer_to_capital        DOUBLE  YES  None    None  None
9                         cash_paid_for_debt_repayment        DOUBLE  YES  None    None  None
10                            loss_from_fair_value_chg        DOUBLE  YES  None    None  None
11                            others_in_cashflow_sheet        DOUBLE  YES  None    None  None
12           cash_paid_for_dividends_profits_interests        DOUBLE  YES  None    None  None
13                       cash_received_from_bond_issue        DOUBLE  YES  None    None  None
14                       cash_received_from_borrowings        DOUBLE  YES  None    None  None
15                 cash_paid_by_acquiring_subsidiaries        DOUBLE  YES  None    None  None
16                                return_on_investment        DOUBLE  YES  None    None  None
17              netinc_in_borrowings_from_central_bank        DOUBLE  YES  None    None  None
18         netinc_in_loans_from_other_fin_institutions        DOUBLE  YES  None    None  None
19                      capital_contributions_received        DOUBLE  YES  None    None  None
20                   netinc_in_repurchase_transactions        DOUBLE  YES  None    None  None
21                          depreciation_of_fa_oga_pba        DOUBLE  YES  None    None  None
22                  loss_from_scraping_of_fixed_assets        DOUBLE  YES  None    None  None
23                       netinc_in_disposal_fin_assets        DOUBLE  YES  None    None  None
24         net_cash_received_from_disposal_filt_assets        DOUBLE  YES  None    None  None
25                     loss_from_disposal_of_fa_ia_lta        DOUBLE  YES  None    None  None
26        net_cash_received_from_disposal_subsidiaries        DOUBLE  YES  None    None  None
27         cash_received_by_subsidiaries_from_minority        DOUBLE  YES  None    None  None
28               cash_paid_by_subsidiaries_to_minority        DOUBLE  YES  None    None  None
29              netinc_deposits_central_bank_interbank        DOUBLE  YES  None    None  None
30                             decrease_in_inventories        DOUBLE  YES  None    None  None
31                                  netinc_in_deposits        DOUBLE  YES  None    None  None
32                        netinc_in_loans_and_advances        DOUBLE  YES  None    None  None
33                                         invest_loss        DOUBLE  YES  None    None  None
34                           cash_paid_for_investments        DOUBLE  YES  None    None  None
35                                           net_cffia        DOUBLE  YES  None    None  None
36                            totbal_diff_of_net_cffia        DOUBLE  YES  None    None  None
37                              spec_diff_of_net_cffia        DOUBLE  YES  None    None  None
38                                      subtotal_cifia        DOUBLE  YES  None    None  None
39                                totbal_diff_of_cifia        DOUBLE  YES  None    None  None
40                                  spec_diff_of_cifia        DOUBLE  YES  None    None  None
41                                   subtotal_of_cofia        DOUBLE  YES  None    None  None
42                                totbal_diff_of_cofia        DOUBLE  YES  None    None  None
43                                  spec_diff_of_cofia        DOUBLE  YES  None    None  None
44     netinc_in_loans_from_banks_and_fin_institutions        DOUBLE  YES  None    None  None
45                      cash_paid_for_policy_dividends        DOUBLE  YES  None    None  None
46                       cash_paid_for_other_investing        DOUBLE  YES  None    None  None
47                       cash_paid_for_other_financing        DOUBLE  YES  None    None  None
48                                         other_cofoa        DOUBLE  YES  None    None  None
49        cash_paid_for_interests_fees_and_commissions        DOUBLE  YES  None    None  None
50                                cash_paid_for_claims        DOUBLE  YES  None    None  None
51                      cash_paid_for_taxes_and_levies        DOUBLE  YES  None    None  None
52                             cash_paid_for_employees        DOUBLE  YES  None    None  None
53                  cash_received_from_other_investing        DOUBLE  YES  None    None  None
54                  cash_received_from_other_financing        DOUBLE  YES  None    None  None
55                  cash_received_from_other_operating        DOUBLE  YES  None    None  None
56                  net_cash_received_from_reinsurance        DOUBLE  YES  None    None  None
57                         cash_received_from_premiums        DOUBLE  YES  None    None  None
58                            taxes_and_levies_rebates        DOUBLE  YES  None    None  None
59   cash_received_from_interests_fess_and_commissions        DOUBLE  YES  None    None  None
60             cash_received_from_disposal_investments        DOUBLE  YES  None    None  None
61                    amorization_of_intangible_assets        DOUBLE  YES  None    None  None
62                                       cce_beginning        DOUBLE  YES  None    None  None
63                                          cce_ending        DOUBLE  YES  None    None  None
64                       effect_of_exchange_chg_on_cce        DOUBLE  YES  None    None  None
65                                       netinc_in_cce        DOUBLE  YES  None    None  None
66                        net_profit_in_cashflow_sheet        DOUBLE  YES  None    None  None
67                  finance_expenses_in_cashflow_sheet        DOUBLE  YES  None    None  None
68                              cash_balance_beginning        DOUBLE  YES  None    None  None
69                                 cash_balance_ending        DOUBLE  YES  None    None  None
70                               cce_balance_beginning        DOUBLE  YES  None    None  None
71                                  cce_balance_ending        DOUBLE  YES  None    None  None
72                        totbal_diff_of_netinc_in_cce        DOUBLE  YES  None    None  None
73                          spec_diff_of_netinc_in_cce        DOUBLE  YES  None    None  None
74                                           net_cfffa        DOUBLE  YES  None    None  None
75                            totbal_diff_of_net_cfffa        DOUBLE  YES  None    None  None
76                              spec_diff_of_net_cfffa        DOUBLE  YES  None    None  None
77                                      subtotal_ciffa        DOUBLE  YES  None    None  None
78                                totbal_diff_of_ciffa        DOUBLE  YES  None    None  None
79                                  spec_diff_of_ciffa        DOUBLE  YES  None    None  None
80                                   subtotal_of_coffa        DOUBLE  YES  None    None  None
81                                totbal_diff_of_coffa        DOUBLE  YES  None    None  None
82                                  spec_diff_of_coffa        DOUBLE  YES  None    None  None
83                      increase_in_operating_payables        DOUBLE  YES  None    None  None
84                   decrease_in_operating_receivables        DOUBLE  YES  None    None  None
85                                           net_cffoa        DOUBLE  YES  None    None  None
86                            totbal_diff_of_net_cffoa        DOUBLE  YES  None    None  None
87                              spec_diff_of_net_cffoa        DOUBLE  YES  None    None  None
88                                      subtotal_cifoa        DOUBLE  YES  None    None  None
89                                totbal_diff_of_cifoa        DOUBLE  YES  None    None  None
90                                  spec_diff_of_cifoa        DOUBLE  YES  None    None  None
91                                      subtotal_cofoa        DOUBLE  YES  None    None  None
92                                totbal_diff_of_cofoa        DOUBLE  YES  None    None  None
93                                  spec_diff_of_cofoa        DOUBLE  YES  None    None  None
94                              fin_lease_fixed_assets        DOUBLE  YES  None    None  None
95                              netinc_in_pledge_loans        DOUBLE  YES  None    None  None
96                    cash_paid_for_goods_and_services        DOUBLE  YES  None    None  None
97                           cash_paid_for_filt_assets        DOUBLE  YES  None    None  None
98                            asset_impairment_reserve        DOUBLE  YES  None    None  None
99                increase_in_deferred_tax_liabilities        DOUBLE  YES  None    None  None
100                    decrease_in_deferred_tax_assets        DOUBLE  YES  None    None  None
101              cash_received_from_sales_and_services        DOUBLE  YES  None    None  None
102         amortization_of_longterm_deferred_expenses        DOUBLE  YES  None    None  None
103                             netinc_in_cce_indirect        DOUBLE  YES  None    None  None
104              totbal_diff_of_netinc_in_cce_indirect        DOUBLE  YES  None    None  None
105                spec_diff_of_netinc_in_cce_indirect        DOUBLE  YES  None    None  None
106                                 net_cffoa_indirect        DOUBLE  YES  None    None  None
107                  totbal_diff_of_net_cffoa_indirect        DOUBLE  YES  None    None  None
108                    spec_diff_of_net_cffoa_indirect        DOUBLE  YES  None    None  None

========== cn_stock_financial_balance_general_pit ==========
                                           column_name   column_type null   key default extra
0                                                 date  TIMESTAMP_NS  YES  None    None  None
1                                           instrument       VARCHAR  YES  None    None  None
2                                          report_date  TIMESTAMP_NS  YES  None    None  None
3                                     fs_quarter_index       TINYINT  YES  None    None  None
4                                          change_type       TINYINT  YES  None    None  None
5                    noncurr_liabilities_due_within_1y        DOUBLE  YES  None    None  None
6                         noncurr_assets_due_within_1y        DOUBLE  YES  None    None  None
7                                      general_reserve        DOUBLE  YES  None    None  None
8                                     specific_reserve        DOUBLE  YES  None    None  None
9                                    specific_payables        DOUBLE  YES  None    None  None
10                   fin_assets_purchased_under_resale        DOUBLE  YES  None    None  None
11                            tradable_fin_liabilities        DOUBLE  YES  None    None  None
12                                 tradable_fin_assets        DOUBLE  YES  None    None  None
13                             acting_trading_payables        DOUBLE  YES  None    None  None
14                               underwriting_payables        DOUBLE  YES  None    None  None
15                            fin_assets_by_fair_value        DOUBLE  YES  None    None  None
16                        fin_assets_by_amortized_cost        DOUBLE  YES  None    None  None
17                                   preference_shares        DOUBLE  YES  None    None  None
18                                 right_of_use_assets        DOUBLE  YES  None    None  None
19                         insurance_contract_reserves        DOUBLE  YES  None    None  None
20                                    debt_investments        DOUBLE  YES  None    None  None
21              preference_of_other_equity_instruments        DOUBLE  YES  None    None  None
22                              other_debt_investments        DOUBLE  YES  None    None  None
23                                      other_payables        DOUBLE  YES  None    None  None
24                                  other_payables_sum        DOUBLE  YES  None    None  None
25                                   other_receivables        DOUBLE  YES  None    None  None
26                               other_receivables_sum        DOUBLE  YES  None    None  None
27                            other_equity_instruments        DOUBLE  YES  None    None  None
28                            other_equity_investments        DOUBLE  YES  None    None  None
29                           other_current_liabilities        DOUBLE  YES  None    None  None
30                                other_current_assets        DOUBLE  YES  None    None  None
31                               balance_othcom_income        DOUBLE  YES  None    None  None
32                           other_noncurr_liabilities        DOUBLE  YES  None    None  None
33                                other_noncurr_assets        DOUBLE  YES  None    None  None
34                            other_noncurr_fin_assets        DOUBLE  YES  None    None  None
35                        fin_assets_sold_under_resale        DOUBLE  YES  None    None  None
36                                  loans_and_advances        DOUBLE  YES  None    None  None
37                       available_for_sale_fin_assets        DOUBLE  YES  None    None  None
38                                contract_liabilities        DOUBLE  YES  None    None  None
39                                     contract_assets        DOUBLE  YES  None    None  None
40                         borrowing_from_central_bank        DOUBLE  YES  None    None  None
41             deposits_from_banks_and_fin_instiutions        DOUBLE  YES  None    None  None
42                                            goodwill        DOUBLE  YES  None    None  None
43                                        fixed_assets        DOUBLE  YES  None    None  None
44                                    fixed_assets_sum        DOUBLE  YES  None    None  None
45                               fixed_assets_disposal        DOUBLE  YES  None    None  None
46                            construction_in_progress        DOUBLE  YES  None    None  None
47                        construction_in_progress_sum        DOUBLE  YES  None    None  None
48        balance_translation_diff_of_foreign_currency        DOUBLE  YES  None    None  None
49                                         inventories        DOUBLE  YES  None    None  None
50                                       share_capital        DOUBLE  YES  None    None  None
51                                  minority_interests        DOUBLE  YES  None    None  None
52                                   project_materials        DOUBLE  YES  None    None  None
53                                     treasury_shares        DOUBLE  YES  None    None  None
54                            taxes_and_levies_payable        DOUBLE  YES  None    None  None
55                                       bonds_payable        DOUBLE  YES  None    None  None
56                                reinsurance_payables        DOUBLE  YES  None    None  None
57                                    interest_payable        DOUBLE  YES  None    None  None
58                        fees_and_commissions_payable        DOUBLE  YES  None    None  None
59                             shortterm_bonds_payable        DOUBLE  YES  None    None  None
60                                       notes_payable        DOUBLE  YES  None    None  None
61                          notes_and_accounts_payable        DOUBLE  YES  None    None  None
62                           employee_benefits_payable        DOUBLE  YES  None    None  None
63                                   dividends_payable        DOUBLE  YES  None    None  None
64                                    accounts_payable        DOUBLE  YES  None    None  None
65                                 premiums_receivable        DOUBLE  YES  None    None  None
66             receivable_reinsurance_contract_reserve        DOUBLE  YES  None    None  None
67                             reinsurance_receivables        DOUBLE  YES  None    None  None
68                                 interest_receivable        DOUBLE  YES  None    None  None
69                               receivables_financing        DOUBLE  YES  None    None  None
70                                    notes_receivable        DOUBLE  YES  None    None  None
71                       notes_and_accounts_receivable        DOUBLE  YES  None    None  None
72                                dividends_receivable        DOUBLE  YES  None    None  None
73                                 accounts_receivable        DOUBLE  YES  None    None  None
74                                   development_costs        DOUBLE  YES  None    None  None
75                 total_equity_to_parent_shareholders        DOUBLE  YES  None    None  None
76                                  total_owner_equity        DOUBLE  YES  None    None  None
77                                 investment_property        DOUBLE  YES  None    None  None
78               loans_from_banks_and_fin_institutions        DOUBLE  YES  None    None  None
79                 loans_to_banks_and_fin_institutions        DOUBLE  YES  None    None  None
80                           liabilities_held_for_sale        DOUBLE  YES  None    None  None
81                                assets_held_for_sale        DOUBLE  YES  None    None  None
82                         held_to_maturity_invesments        DOUBLE  YES  None    None  None
83                                   intangible_assets        DOUBLE  YES  None    None  None
84                                undistributed_profit        DOUBLE  YES  None    None  None
85                                     perpetual_bonds        DOUBLE  YES  None    None  None
86                                  oil_and_gas_assets        DOUBLE  YES  None    None  None
87                           total_current_liabilities        DOUBLE  YES  None    None  None
88                  totbal_diff_of_current_liabilities        DOUBLE  YES  None    None  None
89                    spec_diff_of_current_liabilities        DOUBLE  YES  None    None  None
90                                total_current_assets        DOUBLE  YES  None    None  None
91                       totbal_diff_of_current_assets        DOUBLE  YES  None    None  None
92                         spec_diff_of_current_assets        DOUBLE  YES  None    None  None
93                        productive_biological_assets        DOUBLE  YES  None    None  None
94                                     surplus_reserve        DOUBLE  YES  None    None  None
95                                shortterm_borrowings        DOUBLE  YES  None    None  None
96                                   lease_liabilities        DOUBLE  YES  None    None  None
97                                  settlment_reserves        DOUBLE  YES  None    None  None
98                    spec_diff_of_shareholders_equity        DOUBLE  YES  None    None  None
99                  totbal_diff_of_shareholders_equity        DOUBLE  YES  None    None  None
100                        derivatives_fin_liabilities        DOUBLE  YES  None    None  None
101                             derivatives_fin_assets        DOUBLE  YES  None    None  None
102  totbal_diff_of_liabilities_and_shareholder_equity        DOUBLE  YES  None    None  None
103    spec_diff_of_liabilities_and_shareholder_equity        DOUBLE  YES  None    None  None
104                                  total_liabilities        DOUBLE  YES  None    None  None
105                 total_liabilities_and_owner_equity        DOUBLE  YES  None    None  None
106                   totbal_diff_of_total_liabilities        DOUBLE  YES  None    None  None
107                     spec_diff_of_total_liabilities        DOUBLE  YES  None    None  None
108                                   moneytary_assets        DOUBLE  YES  None    None  None
109                        totbal_diff_of_total_assets        DOUBLE  YES  None    None  None
110                          spec_diff_of_total_assets        DOUBLE  YES  None    None  None
111                                       total_assets        DOUBLE  YES  None    None  None
112                                   capital_reserves        DOUBLE  YES  None    None  None
113                           deferred_tax_liabilities        DOUBLE  YES  None    None  None
114                                deferred_tax_assets        DOUBLE  YES  None    None  None
115                deferred_income_current_liabilities        DOUBLE  YES  None    None  None
116                deferred_income_noncurr_liabilities        DOUBLE  YES  None    None  None
117                                longterm_borrowings        DOUBLE  YES  None    None  None
118                                  longterm_payables        DOUBLE  YES  None    None  None
119                              longterm_payables_sum        DOUBLE  YES  None    None  None
120                         longterm_employee_benefits        DOUBLE  YES  None    None  None
121                               longterm_receivables        DOUBLE  YES  None    None  None
122                           longterm_prepaid_expense        DOUBLE  YES  None    None  None
123                        longterm_equity_investments        DOUBLE  YES  None    None  None
124                          total_noncurr_liabilities        DOUBLE  YES  None    None  None
125                 totbal_diff_of_noncurr_liabilities        DOUBLE  YES  None    None  None
126                   spec_diff_of_noncurr_liabilities        DOUBLE  YES  None    None  None
127                               total_noncurr_assets        DOUBLE  YES  None    None  None
128                      totbal_diff_of_noncurr_assets        DOUBLE  YES  None    None  None
129                        spec_diff_of_noncurr_assets        DOUBLE  YES  None    None  None
130                                        prepayments        DOUBLE  YES  None    None  None
131                                           advances        DOUBLE  YES  None    None  None
132                                         provisions        DOUBLE  YES  None    None  None

========== cn_stock_financial_ttm_shift ==========
                                                  column_name   column_type null   key default extra
0                                                        date  TIMESTAMP_NS  YES  None    None  None
1                                                  instrument       VARCHAR  YES  None    None  None
2                                                 report_date  TIMESTAMP_NS  YES  None    None  None
3                                                       shift       TINYINT  YES  None    None  None
4                                 total_operating_revenue_ttm        DOUBLE  YES  None    None  None
5                                       operating_revenue_ttm        DOUBLE  YES  None    None  None
6                                         interest_income_ttm        DOUBLE  YES  None    None  None
7                                insurance_premium_income_ttm        DOUBLE  YES  None    None  None
8                               fee_and_commission_income_ttm        DOUBLE  YES  None    None  None
9                          spec_diff_of_operating_revenue_ttm        DOUBLE  YES  None    None  None
10                       totbal_diff_of_operating_revenue_ttm        DOUBLE  YES  None    None  None
11                                  total_operating_costs_ttm        DOUBLE  YES  None    None  None
12                                        operating_costs_ttm        DOUBLE  YES  None    None  None
13                                         interest_costs_ttm        DOUBLE  YES  None    None  None
14                               fee_and_commission_costs_ttm        DOUBLE  YES  None    None  None
15                                             surrenders_ttm        DOUBLE  YES  None    None  None
16                              net_insurance_claims_paid_ttm        DOUBLE  YES  None    None  None
17                        net_amount_of_insurance_reserve_ttm        DOUBLE  YES  None    None  None
18                            expense_on_policy_dividends_ttm        DOUBLE  YES  None    None  None
19                            reinsurance_premium_expense_ttm        DOUBLE  YES  None    None  None
20                                       taxes_and_levies_ttm        DOUBLE  YES  None    None  None
21                                         selling_epense_ttm        DOUBLE  YES  None    None  None
22                                 administrative_expense_ttm        DOUBLE  YES  None    None  None
23                       research_and_development_expense_ttm        DOUBLE  YES  None    None  None
24                                        finance_expense_ttm        DOUBLE  YES  None    None  None
25                                   fin_interest_expense_ttm        DOUBLE  YES  None    None  None
26                                    fin_interest_income_ttm        DOUBLE  YES  None    None  None
27                                  asset_impairment_loss_ttm        DOUBLE  YES  None    None  None
28                                 credit_impairment_loss_ttm        DOUBLE  YES  None    None  None
29                           spec_diff_of_operating_costs_ttm        DOUBLE  YES  None    None  None
30                         totbal_diff_of_operating_costs_ttm        DOUBLE  YES  None    None  None
31                                    fair_value_chg_gain_ttm        DOUBLE  YES  None    None  None
32                                          invest_income_ttm        DOUBLE  YES  None    None  None
33                     invest_income_of_jv_and_associates_ttm        DOUBLE  YES  None    None  None
34   income_derecognition_of_fin_assets_at_amortized_cost_ttm        DOUBLE  YES  None    None  None
35                               net_income_of_open_hedge_ttm        DOUBLE  YES  None    None  None
36                                          exchange_gain_ttm        DOUBLE  YES  None    None  None
37                                  asset_disposal_income_ttm        DOUBLE  YES  None    None  None
38                                           other_income_ttm        DOUBLE  YES  None    None  None
39                          spec_diff_of_operating_profit_ttm        DOUBLE  YES  None    None  None
40                        totbal_diff_of_operating_profit_ttm        DOUBLE  YES  None    None  None
41                                       operating_profit_ttm        DOUBLE  YES  None    None  None
42                                    nonoperating_income_ttm        DOUBLE  YES  None    None  None
43                            noncurr_assets_dispose_gain_ttm        DOUBLE  YES  None    None  None
44                                     nonoperating_costs_ttm        DOUBLE  YES  None    None  None
45                            noncurr_assets_dispose_loss_ttm        DOUBLE  YES  None    None  None
46                              spec_diff_of_total_profit_ttm        DOUBLE  YES  None    None  None
47                            totbal_diff_of_total_profit_ttm        DOUBLE  YES  None    None  None
48                                           total_profit_ttm        DOUBLE  YES  None    None  None
49                                     income_tax_expense_ttm        DOUBLE  YES  None    None  None
50                                spec_diff_of_net_profit_ttm        DOUBLE  YES  None    None  None
51                              totbal_diff_of_net_profit_ttm        DOUBLE  YES  None    None  None
52                                             net_profit_ttm        DOUBLE  YES  None    None  None
53                        continuing_operation_net_profit_ttm        DOUBLE  YES  None    None  None
54                      discontinued_operation_net_profit_ttm        DOUBLE  YES  None    None  None
55                      net_profit_to_parent_shareholders_ttm        DOUBLE  YES  None    None  None
56                                 net_profit_to_minority_ttm        DOUBLE  YES  None    None  None
57                                              eps_basic_ttm        DOUBLE  YES  None    None  None
58                                            eps_diluted_ttm        DOUBLE  YES  None    None  None
59                                   income_othcom_income_ttm        DOUBLE  YES  None    None  None
60                   othcom_income_to_parent_shareholders_ttm        DOUBLE  YES  None    None  None
61                            othcom_income_cannt_reclass_ttm        DOUBLE  YES  None    None  None
62                                  chg_by_remeasurements_ttm        DOUBLE  YES  None    None  None
63        othcom_income_cannt_reclass_under_equity_method_ttm        DOUBLE  YES  None    None  None
64                                    other_cannt_reclass_ttm        DOUBLE  YES  None    None  None
65                other_equity_instruments_fair_value_chg_ttm        DOUBLE  YES  None    None  None
66                         own_credit_risk_fair_value_chg_ttm        DOUBLE  YES  None    None  None
67                                  othcom_income_reclass_ttm        DOUBLE  YES  None    None  None
68              othcom_income_reclass_under_equity_method_ttm        DOUBLE  YES  None    None  None
69           available_for_sale_fin_assets_fair_value_chg_ttm        DOUBLE  YES  None    None  None
70                        gains_or_losses_from_htm_to_afs_ttm        DOUBLE  YES  None    None  None
71         effective_of_gains_or_losses_on_cashflow_hedge_ttm        DOUBLE  YES  None    None  None
72            income_translation_diff_of_foreign_currency_ttm        DOUBLE  YES  None    None  None
73                                          other_reclass_ttm        DOUBLE  YES  None    None  None
74                  other_debt_investments_fair_value_chg_ttm        DOUBLE  YES  None    None  None
75               othcom_income_from_reclass_of_fin_assets_ttm        DOUBLE  YES  None    None  None
76            credit_impairment_of_other_debt_investments_ttm        DOUBLE  YES  None    None  None
77                                 cashflow_hedge_reserve_ttm        DOUBLE  YES  None    None  None
78                              othcom_income_to_minority_ttm        DOUBLE  YES  None    None  None
79                             total_comprehensive_income_ttm        DOUBLE  YES  None    None  None
80      total_comprehensive_income_to_parent_shareholders_ttm        DOUBLE  YES  None    None  None
81                  cash_received_from_sales_and_services_ttm        DOUBLE  YES  None    None  None
82                                     netinc_in_deposits_ttm        DOUBLE  YES  None    None  None
83                 netinc_in_borrowings_from_central_bank_ttm        DOUBLE  YES  None    None  None
84            netinc_in_loans_from_other_fin_institutions_ttm        DOUBLE  YES  None    None  None
85                            cash_received_from_premiums_ttm        DOUBLE  YES  None    None  None
86                     net_cash_received_from_reinsurance_ttm        DOUBLE  YES  None    None  None
87                netinc_in_insurance_deposits_and_invest_ttm        DOUBLE  YES  None    None  None
88                          netinc_in_disposal_fin_assets_ttm        DOUBLE  YES  None    None  None
89      cash_received_from_interests_fess_and_commissions_ttm        DOUBLE  YES  None    None  None
90        netinc_in_loans_from_banks_and_fin_institutions_ttm        DOUBLE  YES  None    None  None
91                      netinc_in_repurchase_transactions_ttm        DOUBLE  YES  None    None  None
92                               taxes_and_levies_rebates_ttm        DOUBLE  YES  None    None  None
93                     cash_received_from_other_operating_ttm        DOUBLE  YES  None    None  None
94                                     spec_diff_of_cifoa_ttm        DOUBLE  YES  None    None  None
95                                   totbal_diff_of_cifoa_ttm        DOUBLE  YES  None    None  None
96                                         subtotal_cifoa_ttm        DOUBLE  YES  None    None  None
97                       cash_paid_for_goods_and_services_ttm        DOUBLE  YES  None    None  None
98                           netinc_in_loans_and_advances_ttm        DOUBLE  YES  None    None  None
99                 netinc_deposits_central_bank_interbank_ttm        DOUBLE  YES  None    None  None
100                                  cash_paid_for_claims_ttm        DOUBLE  YES  None    None  None
101          cash_paid_for_interests_fees_and_commissions_ttm        DOUBLE  YES  None    None  None
102                        cash_paid_for_policy_dividends_ttm        DOUBLE  YES  None    None  None
103                               cash_paid_for_employees_ttm        DOUBLE  YES  None    None  None
104                        cash_paid_for_taxes_and_levies_ttm        DOUBLE  YES  None    None  None
105                                           other_cofoa_ttm        DOUBLE  YES  None    None  None
106                                    spec_diff_of_cofoa_ttm        DOUBLE  YES  None    None  None
107                                  totbal_diff_of_cofoa_ttm        DOUBLE  YES  None    None  None
108                                        subtotal_cofoa_ttm        DOUBLE  YES  None    None  None
109                                spec_diff_of_net_cffoa_ttm        DOUBLE  YES  None    None  None
110                              totbal_diff_of_net_cffoa_ttm        DOUBLE  YES  None    None  None
111                                             net_cffoa_ttm        DOUBLE  YES  None    None  None
112               cash_received_from_disposal_investments_ttm        DOUBLE  YES  None    None  None
113                                  return_on_investment_ttm        DOUBLE  YES  None    None  None
114           net_cash_received_from_disposal_filt_assets_ttm        DOUBLE  YES  None    None  None
115          net_cash_received_from_disposal_subsidiaries_ttm        DOUBLE  YES  None    None  None
116                    cash_received_from_other_investing_ttm        DOUBLE  YES  None    None  None
117                                    spec_diff_of_cifia_ttm        DOUBLE  YES  None    None  None
118                                  totbal_diff_of_cifia_ttm        DOUBLE  YES  None    None  None
119                                        subtotal_cifia_ttm        DOUBLE  YES  None    None  None
120                             cash_paid_for_filt_assets_ttm        DOUBLE  YES  None    None  None
121                             cash_paid_for_investments_ttm        DOUBLE  YES  None    None  None
122                                netinc_in_pledge_loans_ttm        DOUBLE  YES  None    None  None
123                   cash_paid_by_acquiring_subsidiaries_ttm        DOUBLE  YES  None    None  None
124                         cash_paid_for_other_investing_ttm        DOUBLE  YES  None    None  None
125                                    spec_diff_of_cofia_ttm        DOUBLE  YES  None    None  None
126                                  totbal_diff_of_cofia_ttm        DOUBLE  YES  None    None  None
127                                     subtotal_of_cofia_ttm        DOUBLE  YES  None    None  None
128                                spec_diff_of_net_cffia_ttm        DOUBLE  YES  None    None  None
129                              totbal_diff_of_net_cffia_ttm        DOUBLE  YES  None    None  None
130                                             net_cffia_ttm        DOUBLE  YES  None    None  None
131                        capital_contributions_received_ttm        DOUBLE  YES  None    None  None
132           cash_received_by_subsidiaries_from_minority_ttm        DOUBLE  YES  None    None  None
133                         cash_received_from_borrowings_ttm        DOUBLE  YES  None    None  None
134                         cash_received_from_bond_issue_ttm        DOUBLE  YES  None    None  None
135                    cash_received_from_other_financing_ttm        DOUBLE  YES  None    None  None
136                                    spec_diff_of_ciffa_ttm        DOUBLE  YES  None    None  None
137                                  totbal_diff_of_ciffa_ttm        DOUBLE  YES  None    None  None
138                                        subtotal_ciffa_ttm        DOUBLE  YES  None    None  None
139                          cash_paid_for_debt_repayment_ttm        DOUBLE  YES  None    None  None
140             cash_paid_for_dividends_profits_interests_ttm        DOUBLE  YES  None    None  None
141                 cash_paid_by_subsidiaries_to_minority_ttm        DOUBLE  YES  None    None  None
142                         cash_paid_for_other_financing_ttm        DOUBLE  YES  None    None  None
143                                    spec_diff_of_coffa_ttm        DOUBLE  YES  None    None  None
144                                  totbal_diff_of_coffa_ttm        DOUBLE  YES  None    None  None
145                                     subtotal_of_coffa_ttm        DOUBLE  YES  None    None  None
146                                spec_diff_of_net_cfffa_ttm        DOUBLE  YES  None    None  None
147                              totbal_diff_of_net_cfffa_ttm        DOUBLE  YES  None    None  None
148                                             net_cfffa_ttm        DOUBLE  YES  None    None  None
149                         effect_of_exchange_chg_on_cce_ttm        DOUBLE  YES  None    None  None
150                            spec_diff_of_netinc_in_cce_ttm        DOUBLE  YES  None    None  None
151                          totbal_diff_of_netinc_in_cce_ttm        DOUBLE  YES  None    None  None
152                                         netinc_in_cce_ttm        DOUBLE  YES  None    None  None
153                                         cce_beginning_ttm        DOUBLE  YES  None    None  None
154                                            cce_ending_ttm        DOUBLE  YES  None    None  None
155                          net_profit_in_cashflow_sheet_ttm        DOUBLE  YES  None    None  None
156                              asset_impairment_reserve_ttm        DOUBLE  YES  None    None  None
157                            depreciation_of_fa_oga_pba_ttm        DOUBLE  YES  None    None  None
158                      amorization_of_intangible_assets_ttm        DOUBLE  YES  None    None  None
159            amortization_of_longterm_deferred_expenses_ttm        DOUBLE  YES  None    None  None
160                       loss_from_disposal_of_fa_ia_lta_ttm        DOUBLE  YES  None    None  None
161                    loss_from_scraping_of_fixed_assets_ttm        DOUBLE  YES  None    None  None
162                              loss_from_fair_value_chg_ttm        DOUBLE  YES  None    None  None
163                    finance_expenses_in_cashflow_sheet_ttm        DOUBLE  YES  None    None  None
164                                           invest_loss_ttm        DOUBLE  YES  None    None  None
165                       decrease_in_deferred_tax_assets_ttm        DOUBLE  YES  None    None  None
166                  increase_in_deferred_tax_liabilities_ttm        DOUBLE  YES  None    None  None
167                               decrease_in_inventories_ttm        DOUBLE  YES  None    None  None
168                     decrease_in_operating_receivables_ttm        DOUBLE  YES  None    None  None
169                        increase_in_operating_payables_ttm        DOUBLE  YES  None    None  None
170                              others_in_cashflow_sheet_ttm        DOUBLE  YES  None    None  None
171                       spec_diff_of_net_cffoa_indirect_ttm        DOUBLE  YES  None    None  None
172                     totbal_diff_of_net_cffoa_indirect_ttm        DOUBLE  YES  None    None  None
173                                    net_cffoa_indirect_ttm        DOUBLE  YES  None    None  None
174                              debt_transfer_to_capital_ttm        DOUBLE  YES  None    None  None
175                             conv_corp_bonds_within_1y_ttm        DOUBLE  YES  None    None  None
176                                fin_lease_fixed_assets_ttm        DOUBLE  YES  None    None  None
177                                   cash_balance_ending_ttm        DOUBLE  YES  None    None  None
178                                cash_balance_beginning_ttm        DOUBLE  YES  None    None  None
179                                    cce_balance_ending_ttm        DOUBLE  YES  None    None  None
180                                 cce_balance_beginning_ttm        DOUBLE  YES  None    None  None
181                   spec_diff_of_netinc_in_cce_indirect_ttm        DOUBLE  YES  None    None  None
182                 totbal_diff_of_netinc_in_cce_indirect_ttm        DOUBLE  YES  None    None  None
183                                netinc_in_cce_indirect_ttm        DOUBLE  YES  None    None  None
184              credit_impairment_loss_in_cashflow_sheet_ttm        DOUBLE  YES  None    None  None

========== cn_stock_financial_notes_shift ==========
                                  column_name   column_type null   key default extra
0                                        date  TIMESTAMP_NS  YES  None    None  None
1                                  instrument       VARCHAR  YES  None    None  None
2                                 report_date  TIMESTAMP_NS  YES  None    None  None
3                                       shift       TINYINT  YES  None    None  None
4                          illiquid_assets_lf        DOUBLE  YES  None    None  None
5                                  taxfree_lf        DOUBLE  YES  None    None  None
6                        government_grants_lf        DOUBLE  YES  None    None  None
7                            occupancy_fee_lf        DOUBLE  YES  None    None  None
8                    fair_value_investment_lf        DOUBLE  YES  None    None  None
9              non_monetary_asset_exchange_lf        DOUBLE  YES  None    None  None
10                       manage_investment_lf        DOUBLE  YES  None    None  None
11                     debt_reorganization_lf        DOUBLE  YES  None    None  None
12               enterprise_reorganization_lf        DOUBLE  YES  None    None  None
13                      fair_value_trading_lf        DOUBLE  YES  None    None  None
14                                   merge_lf        DOUBLE  YES  None    None  None
15                     bussiness_unrelated_lf        DOUBLE  YES  None    None  None
16                  other_financial_income_lf        DOUBLE  YES  None    None  None
17          reversal_of_impairment_reserve_lf        DOUBLE  YES  None    None  None
18                  external_entruste_loan_lf        DOUBLE  YES  None    None  None
19   invested_realestate_fair_value_change_lf        DOUBLE  YES  None    None  None
20              influence_after_adjustment_lf        DOUBLE  YES  None    None  None
21                             trustee_fee_lf        DOUBLE  YES  None    None  None
22               other_nonoperating_income_lf        DOUBLE  YES  None    None  None
23                other_nonrecurring_items_lf        DOUBLE  YES  None    None  None
24                       income_tax_impact_lf        DOUBLE  YES  None    None  None
25            nonrecurring_income_to_owner_lf        DOUBLE  YES  None    None  None
26         nonrecurring_income_to_minority_lf        DOUBLE  YES  None    None  None
27                 nonrecurring_income_sum_lf        DOUBLE  YES  None    None  None
28                        illiquid_assets_mrq        DOUBLE  YES  None    None  None
29                                taxfree_mrq        DOUBLE  YES  None    None  None
30                      government_grants_mrq        DOUBLE  YES  None    None  None
31                          occupancy_fee_mrq        DOUBLE  YES  None    None  None
32                  fair_value_investment_mrq        DOUBLE  YES  None    None  None
33            non_monetary_asset_exchange_mrq        DOUBLE  YES  None    None  None
34                      manage_investment_mrq        DOUBLE  YES  None    None  None
35                    debt_reorganization_mrq        DOUBLE  YES  None    None  None
36              enterprise_reorganization_mrq        DOUBLE  YES  None    None  None
37                     fair_value_trading_mrq        DOUBLE  YES  None    None  None
38                                  merge_mrq        DOUBLE  YES  None    None  None
39                    bussiness_unrelated_mrq        DOUBLE  YES  None    None  None
40                 other_financial_income_mrq        DOUBLE  YES  None    None  None
41         reversal_of_impairment_reserve_mrq        DOUBLE  YES  None    None  None
42                 external_entruste_loan_mrq        DOUBLE  YES  None    None  None
43  invested_realestate_fair_value_change_mrq        DOUBLE  YES  None    None  None
44             influence_after_adjustment_mrq        DOUBLE  YES  None    None  None
45                            trustee_fee_mrq        DOUBLE  YES  None    None  None
46              other_nonoperating_income_mrq        DOUBLE  YES  None    None  None
47               other_nonrecurring_items_mrq        DOUBLE  YES  None    None  None
48                      income_tax_impact_mrq        DOUBLE  YES  None    None  None
49           nonrecurring_income_to_owner_mrq        DOUBLE  YES  None    None  None
50        nonrecurring_income_to_minority_mrq        DOUBLE  YES  None    None  None
51                nonrecurring_income_sum_mrq        DOUBLE  YES  None    None  None
52                        illiquid_assets_ttm        DOUBLE  YES  None    None  None
53                                taxfree_ttm        DOUBLE  YES  None    None  None
54                      government_grants_ttm        DOUBLE  YES  None    None  None
55                          occupancy_fee_ttm        DOUBLE  YES  None    None  None
56                  fair_value_investment_ttm        DOUBLE  YES  None    None  None
57            non_monetary_asset_exchange_ttm        DOUBLE  YES  None    None  None
58                      manage_investment_ttm        DOUBLE  YES  None    None  None
59                    debt_reorganization_ttm        DOUBLE  YES  None    None  None
60              enterprise_reorganization_ttm        DOUBLE  YES  None    None  None
61                     fair_value_trading_ttm        DOUBLE  YES  None    None  None
62                                  merge_ttm        DOUBLE  YES  None    None  None
63                    bussiness_unrelated_ttm        DOUBLE  YES  None    None  None
64                 other_financial_income_ttm        DOUBLE  YES  None    None  None
65         reversal_of_impairment_reserve_ttm        DOUBLE  YES  None    None  None
66                 external_entruste_loan_ttm        DOUBLE  YES  None    None  None
67  invested_realestate_fair_value_change_ttm        DOUBLE  YES  None    None  None
68             influence_after_adjustment_ttm        DOUBLE  YES  None    None  None
69                            trustee_fee_ttm        DOUBLE  YES  None    None  None
70              other_nonoperating_income_ttm        DOUBLE  YES  None    None  None
71               other_nonrecurring_items_ttm        DOUBLE  YES  None    None  None
72                      income_tax_impact_ttm        DOUBLE  YES  None    None  None
73           nonrecurring_income_to_owner_ttm        DOUBLE  YES  None    None  None
74        nonrecurring_income_to_minority_ttm        DOUBLE  YES  None    None  None
75                nonrecurring_income_sum_ttm        DOUBLE  YES  None    None  None

========== cn_stock_profit_estimate ==========
        column_name   column_type null   key default extra
0              date  TIMESTAMP_NS  YES  None    None  None
1        instrument       VARCHAR  YES  None    None  None
2        begin_date  TIMESTAMP_NS  YES  None    None  None
3          end_date  TIMESTAMP_NS  YES  None    None  None
4   fore_profit_min        DOUBLE  YES  None    None  None
5   fore_profit_max        DOUBLE  YES  None    None  None
6         fore_type       TINYINT  YES  None    None  None
7     pct_range_min        DOUBLE  YES  None    None  None
8     pct_range_max        DOUBLE  YES  None    None  None
9           news_id         FLOAT  YES  None    None  None
10     fore_eps_min        DOUBLE  YES  None    None  None
11     fore_eps_max        DOUBLE  YES  None    None  None
12     fore_eps_avg        DOUBLE  YES  None    None  None
13      fore_profit        DOUBLE  YES  None    None  None
14          ex_date  TIMESTAMP_NS  YES  None    None  None
15           remark       VARCHAR  YES  None    None  None

