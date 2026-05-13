#include "common/fs.hpp"

#include <fstream>
#include <sstream>

namespace verify::fs_util {

namespace fs = std::filesystem;

std::string read_file_all(const fs::path &path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void atomic_write(const fs::path &path, const char *data, std::size_t len) {
    fs::create_directories(path.parent_path());
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f.write(data, static_cast<std::streamsize>(len));
    }
    fs::rename(tmp, path);
}

} // namespace verify::fs_util
