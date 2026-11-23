#include "componeng/renderer/shader/include_expander.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace componeng::renderer::shader {

namespace fs = std::filesystem;

namespace {

struct Expander {
  const std::vector<std::string> &roots;
  const std::string &topName;
  std::set<std::string> included;
  std::string out;
  std::vector<std::string> errors;

  // The quoted name of an `#include "name"` line, or empty if it is not one.
  static std::string includeTarget(const std::string &line) {
    std::size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos || line.compare(i, 8, "#include") != 0) {
      return {};
    }
    i = line.find_first_not_of(" \t", i + 8);
    if (i == std::string::npos || line[i] != '"') {
      return {};
    }
    const std::size_t end = line.find('"', i + 1);
    if (end == std::string::npos) {
      return {};
    }
    return line.substr(i + 1, end - i - 1);
  }

  std::string resolve(const std::string &target,
                      const std::string &fromFile) const {
    std::error_code ec;
    const fs::path dir = fs::path(fromFile).parent_path();
    if (!dir.empty()) {
      const fs::path p = dir / target;
      if (fs::is_regular_file(p, ec)) {
        return p.string();
      }
    }
    for (const std::string &root : roots) {
      const fs::path p = fs::path(root) / target;
      if (fs::is_regular_file(p, ec)) {
        return p.string();
      }
    }
    return {};
  }

  void emitLine(int line, const std::string &file) {
    out += "#line " + std::to_string(line) + " \"" + file + "\"\n";
  }

  void expand(const std::string &text, const std::string &file, int firstLine,
              int depth) {
    std::istringstream in(text);
    std::string line;
    int lineNo = firstLine;
    while (std::getline(in, line)) {
      const std::string target = includeTarget(line);
      if (target.empty()) {
        out += line;
        out += '\n';
        ++lineNo;
        continue;
      }

      const std::string where = file + ":" + std::to_string(lineNo);
      const std::string path = resolve(target, file);
      if (path.empty()) {
        errors.push_back("include \"" + target + "\" not found (from " + where +
                         ")");
        out += "#error include \"" + target + "\" not found\n";
      } else if (depth >= kMaxIncludeDepth) {
        errors.push_back("includes nested deeper than " +
                         std::to_string(kMaxIncludeDepth) + " at \"" + target +
                         "\" (from " + where + ")");
        out += "#error includes nested too deeply\n";
      } else {
        std::error_code ec;
        const std::string canonical = fs::weakly_canonical(path, ec).string();
        if (included.insert(canonical).second) {
          std::ifstream f(path);
          std::stringstream body;
          body << f.rdbuf();
          emitLine(1, path);
          expand(body.str(), path, 1, depth + 1);
          emitLine(lineNo + 1, file);
        }
      }
      ++lineNo;
    }
  }
};

} // namespace

ExpandedSource expandIncludes(const std::string &source,
                              const std::string &name,
                              const std::vector<std::string> &roots,
                              int firstLine) {
  Expander e{roots, name, {}, {}, {}};
  std::error_code ec;
  if (fs::is_regular_file(name, ec)) {
    e.included.insert(fs::weakly_canonical(name, ec).string());
  }
  e.expand(source, name, firstLine, 0);
  return {std::move(e.out), std::move(e.errors)};
}

} // namespace componeng::renderer::shader
