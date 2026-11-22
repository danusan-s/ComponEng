#pragma once

#include <string>
#include <vector>

namespace componeng::renderer::shader {

constexpr int kMaxIncludeDepth = 16;

struct ExpandedSource {
  std::string text;
  std::vector<std::string> errors;
};

/**
 * Replaces each `#include "file"` line in `source` with the file's contents,
 * wrapped in `#line` directives so compiler messages name the real file (the
 * compiler enables GL_GOOGLE_cpp_style_line_directive for this).
 *
 * A file is searched for next to the file that includes it, then in each of
 * `roots`. Every file is included at most once per call. `name` is the path
 * of `source` for messages and for the first search; `firstLine` is the
 * line number of the first line of `source`.
 *
 * A missing file, or nesting deeper than kMaxIncludeDepth, adds an entry to
 * `errors` and leaves an `#error` line in the text.
 */
ExpandedSource expandIncludes(const std::string &source,
                              const std::string &name,
                              const std::vector<std::string> &roots,
                              int firstLine = 1);

} // namespace componeng::renderer::shader
