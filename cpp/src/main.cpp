#include "tushare/pipeline.hpp"
#include "tushare/spec.hpp"

int main() {
  tushare::update("20150101", tushare::today_yyyymmdd(), tushare::SPECS);
  return 0;
}
