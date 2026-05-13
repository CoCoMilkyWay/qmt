#include "common/csv.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>

namespace verify::csv {

namespace fs = std::filesystem;

int Table::idx(const std::string &name) const {
    auto it = col_idx.find(name);
    assert(it != col_idx.end() && "csv 列不存在");
    return it->second;
}

bool Table::has(const std::string &name) const {
    return col_idx.find(name) != col_idx.end();
}

const std::string &Table::at(std::size_t row, const std::string &col) const {
    assert(row < rows.size());
    int i = idx(col);
    assert(static_cast<std::size_t>(i) < rows[row].size());
    return rows[row][i];
}

namespace {

// 把整文件读进内存 (CSV 一般 <几百 MB).
// 然后逐字符状态机解析: 字段 = 普通文本 | 引号包裹.
// 行尾 \n 或 \r\n. 引号内的 \n 也算字段内容.
void parse_csv(const std::string &buf, Table &t) {
    enum State { CELL, QUOTED } st = CELL;
    std::vector<std::string> row;
    std::string cell;
    bool any = false;  // 当前 row 是否有任何字符 (避免空行)

    auto end_cell = [&] {
        row.push_back(std::move(cell));
        cell.clear();
    };
    auto end_row = [&] {
        if (!any && row.empty()) return;
        end_cell();
        if (t.header.empty()) {
            t.header = row;
            for (int i = 0; i < static_cast<int>(t.header.size()); ++i) {
                t.col_idx[t.header[i]] = i;
            }
        } else {
            // pandas to_csv 末尾有空行时, 我们已经在 any=false 时早返回了
            assert(row.size() == t.header.size() && "csv 行列数与 header 不一致");
            t.rows.push_back(std::move(row));
        }
        row.clear();
        any = false;
    };

    for (std::size_t i = 0; i < buf.size(); ++i) {
        char c = buf[i];
        any = any || (c != '\r' && c != '\n');
        if (st == CELL) {
            if (c == ',') {
                end_cell();
            } else if (c == '\n') {
                end_row();
            } else if (c == '\r') {
                // \r\n 或孤立 \r 都当行尾
                end_row();
                if (i + 1 < buf.size() && buf[i + 1] == '\n') ++i;
            } else if (c == '"' && cell.empty()) {
                st = QUOTED;
            } else {
                cell.push_back(c);
            }
        } else {  // QUOTED
            if (c == '"') {
                if (i + 1 < buf.size() && buf[i + 1] == '"') {
                    cell.push_back('"');
                    ++i;
                } else {
                    st = CELL;
                }
            } else {
                cell.push_back(c);
            }
        }
    }
    // 末尾无换行时收尾
    if (st == QUOTED) {
        // 文件结束在引号内 — 数据损坏
        assert(false && "csv 文件以未闭合引号结束");
    }
    if (!cell.empty() || !row.empty() || any) {
        end_row();
    }
}

} // namespace

Table read_csv(const fs::path &path) {
    std::ifstream f(path, std::ios::binary);
    assert(f.good() && "csv 文件无法读取");
    std::string buf((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    Table t;
    parse_csv(buf, t);
    return t;
}

} // namespace verify::csv
