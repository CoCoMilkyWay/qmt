import subprocess
import os
import sys
import json
import tempfile

import pyarrow.parquet as pq  # bq CLI 自带 (/bin/python3.11), 没有则直接 ImportError fail early

D = "2024-12-31"
_DF = {"date": [D, D]}

# BigQuant DAI 数据表列表; filters 用于分区收敛 (date 区间)
# 无 filters 的表为非分区或按 instrument 等其它键分区
TABLES = [
    ("trading_days", None),
    ("holidays", None),
    ("cn_stock_instruments", _DF),
    # ("cn_index_instruments", _DF),
    # ("cn_fund_instruments", _DF),
    # ("cn_future_instruments", _DF),
    # ("cn_cbond_instruments", _DF),
    ("cn_stock_industry_component", _DF),
    ("cn_stock_industry_change", _DF),
    ("cn_stock_industry_real_bar1d", _DF),
    ("cn_stock_industry_valuation", _DF),
    ("cn_stock_basic_info", None),
    ("cn_stock_capital", None),
    ("cn_stock_dividend", None),
    ("cn_stock_allotment", None),
    ("cn_stock_margin_trading_detail", _DF),
    ("cn_stock_margin_trading_market", _DF),
    ("cn_stock_shareholder", None),
    ("cn_stock_shares", _DF),
    ("cn_stock_status", _DF),
    ("cn_stock_suspend", _DF),
    ("cn_stock_name_change", None),
    ("cn_stock_dragon_list", _DF),
    ("cn_stock_real_bar1d", _DF),
    ("cn_stock_limit_price", _DF),
    ("cn_stock_financial_income_general_pit", _DF),
    ("cn_stock_financial_cashflow_general_pit", _DF),
    ("cn_stock_financial_balance_general_pit", _DF),
    ("cn_stock_financial_ttm_shift", _DF),
    ("cn_stock_financial_notes_shift", _DF),
    ("cn_stock_financial_changedate", None),
    ("cn_stock_static_data", _DF),
    ("cn_stock_valuation", _DF),
    ("cn_stock_profit_estimate", _DF),
    ("cn_stock_profit_exceed_appraisal", _DF),
    ("cn_stock_profit_exceed_expect", _DF),
    ("cn_stock_profit_below_expect", _DF),
    ("cn_stock_financial_changedate", None),
    ("cn_stock_financial_forecast_consensus_rolling", _DF),
    ("cn_stock_financial_profitability", _DF),
    ("cn_stock_financial_lf_shift", _DF),
]


def dump_parquet(table, filters, path):
    """跑一次 bq dai query 把 LIMIT 5 结果落到 parquet; 返回 (ok, err_msg)."""
    sql = f"SELECT * FROM {table} LIMIT 5"
    cmd = ["bq", "dai", "query", sql, "--output", path]
    if filters is not None:
        cmd += ["--filters", json.dumps(filters)]
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8")
    if r.returncode != 0:
        return False, f"Error (rc={r.returncode}):\n{r.stderr}"
    return True, ""


def _fmt_cell(v):
    """单元格 → 字符串, 对齐 bq 原本 markdown 输出风格 (NaT/nan/timestamp)."""
    if v is None:
        return ""
    # NaT / NaN
    try:
        if v != v:  # NaN
            return "nan"
    except TypeError:
        pass
    if hasattr(v, "strftime"):
        return v.strftime("%Y-%m-%d %H:%M:%S")
    return str(v)


def rows_to_md(headers, rows):
    """headers + rows(of str) → markdown 表 (左对齐, pad 到列最大宽)."""
    all_rows = [headers] + rows
    widths = [max(len(r[j]) for r in all_rows) for j in range(len(headers))]
    sep = "|" + "|".join("-" * (w + 2) for w in widths) + "|"
    def fmt(vals):
        return "| " + " | ".join(v.ljust(w) for v, w in zip(vals, widths)) + " |"
    return "\n".join([fmt(headers), sep] + [fmt(r) for r in rows]) + "\n"


def main():
    # 输出路径设定在 doc/bigquant/used/
    base_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(base_dir, "used")

    schema_file = os.path.join(output_dir, "schema.md")
    example_file = os.path.join(output_dir, "example.md")

    # 确保输出目录存在
    os.makedirs(output_dir, exist_ok=True)

    print(f"开始生成文档...")
    print(f"Schema: {schema_file}")
    print(f"Example: {example_file}")

    with open(schema_file, "w", encoding="utf-8") as f_schema, open(
        example_file, "w", encoding="utf-8"
    ) as f_example, tempfile.TemporaryDirectory() as tmpdir:

        for i, (table, filters) in enumerate(TABLES, 1):
            print(f"[{i}/{len(TABLES)}] 正在处理表: {table}")

            f_schema.write(f"========== {table} ==========\n")
            f_example.write(f"========== {table} ==========\n")

            # 单次 bq 调用 → parquet (同时含 schema + data), 本地 pyarrow 解析自拼 markdown
            parquet_path = os.path.join(tmpdir, f"{table}.parquet")
            ok, err = dump_parquet(table, filters, parquet_path)
            if not ok:
                f_schema.write(err)
                f_example.write(err)
            else:
                tbl = pq.read_table(parquet_path)
                cols = [f.name for f in tbl.schema]
                types = [str(f.type) for f in tbl.schema]
                f_schema.write(rows_to_md(cols, [types]))

                df = tbl.to_pandas()
                data = [[_fmt_cell(v) for v in df.iloc[k].tolist()] for k in range(len(df))]
                f_example.write(rows_to_md(cols, data))

            f_schema.write("\n\n")
            f_example.write("\n\n")
            f_schema.flush()
            f_example.flush()

    print("\n生成完成！")


if __name__ == "__main__":
    main()
