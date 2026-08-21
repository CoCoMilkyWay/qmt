#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "wxmd/assert.hpp"
#include "wxmd/wxmd.hpp"

namespace {

void print_usage() {
  std::cout
      << "用法:\n"
         "  wxmd <文章链接>            抓取并输出 Markdown\n"
         "  wxmd -f <本地 html 文件>   解析本地 HTML（离线，用于回归）\n"
         "\n"
         "可选:\n"
         "  -o <输出文件>              写入文件，默认写到标准输出\n"
         "  --meta                     在 Markdown 前附加标题/作者等元信息\n"
         "  --html                     输出规范化后的中间态 HTML，不转 "
         "Markdown\n";
}

std::string read_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  WXMD_ASSERT(input.is_open(), "无法打开文件: " + path);

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void write_file(const std::string &path, const std::string &content) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  WXMD_ASSERT(output.is_open(), "无法写入文件: " + path);
  output << content;
}

} // namespace

int main(int argc, char **argv) {
  std::string source;
  bool from_file = false;
  bool with_meta = false;
  bool html_only = false;
  std::string output_path;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    }
    if (arg == "-f") {
      WXMD_ASSERT(i + 1 < argc, "-f 后缺少文件路径");
      from_file = true;
      source = argv[++i];
      continue;
    }
    if (arg == "-o") {
      WXMD_ASSERT(i + 1 < argc, "-o 后缺少输出路径");
      output_path = argv[++i];
      continue;
    }
    if (arg == "--meta") {
      with_meta = true;
      continue;
    }
    if (arg == "--html") {
      html_only = true;
      continue;
    }
    source = arg;
  }

  if (source.empty()) {
    print_usage();
    return 1;
  }

  const std::string raw_html =
      from_file ? read_file(source) : wxmd::fetch_article(source);

  if (html_only) {
    const std::string rendered = wxmd::render_article_html(raw_html);
    if (output_path.empty()) {
      std::cout << rendered;
    } else {
      write_file(output_path, rendered);
      std::cerr << "已写入: " << output_path << " (" << rendered.size()
                << " 字节)\n";
    }
    return 0;
  }

  const wxmd::Article article = wxmd::parse_article(raw_html);

  std::string out;
  if (with_meta) {
    out += "# " + article.title + "\n\n";
    out += "- 公众号: " + article.account + "\n";
    out += "- 作者: " + article.author + "\n";
    out += "- 发布时间: " + article.publish_time + "\n";
    out += "- 原文: " + article.link + "\n\n---\n\n";
  }
  out += article.markdown;
  out += "\n";

  if (output_path.empty()) {
    std::cout << out;
  } else {
    write_file(output_path, out);
    std::cerr << "已写入: " << output_path << " (" << out.size() << " 字节)\n";
  }
  return 0;
}
