#pragma once

#include <algorithm>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace shuati::utils {

// String transformation functions
inline std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

inline std::string to_upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return s;
}

// String joining
inline std::string join(const std::vector<std::string> &vec,
                        const std::string &delim) {
  if (vec.empty())
    return "";
  std::string result = vec[0];
  for (size_t i = 1; i < vec.size(); i++) {
    result += delim + vec[i];
  }
  return result;
}

template <typename Container, typename TransformFunc>
inline std::string join_transform(const Container &container,
                                  const std::string &delim,
                                  TransformFunc transform) {
  if (container.empty())
    return "";
  auto it = container.begin();
  std::string result = transform(*it);
  for (++it; it != container.end(); ++it) {
    result += delim + transform(*it);
  }
  return result;
}

// String splitting
inline std::vector<std::string> split(const std::string &str, char delim) {
  std::vector<std::string> result;
  std::stringstream ss(str);
  std::string item;
  while (std::getline(ss, item, delim)) {
    if (!item.empty())
      result.push_back(item);
  }
  return result;
}

inline std::vector<std::string> split(const std::string &str,
                                      const std::string &delim) {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = str.find(delim);
  while (end != std::string::npos) {
    if (end > start) {
      result.push_back(str.substr(start, end - start));
    }
    start = end + delim.length();
    end = str.find(delim, start);
  }
  if (start < str.length()) {
    result.push_back(str.substr(start));
  }
  return result;
}

// String trimming
inline std::string trim_left(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  return (start == std::string::npos) ? "" : s.substr(start);
}

inline std::string trim_right(const std::string &s) {
  size_t end = s.find_last_not_of(" \t\n\r");
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

inline std::string trim(const std::string &s) {
  return trim_left(trim_right(s));
}

// String replacement
inline std::string replace_all(std::string str, const std::string &from,
                               const std::string &to) {
  size_t pos = 0;
  while ((pos = str.find(from, pos)) != std::string::npos) {
    str.replace(pos, from.length(), to);
    pos += to.length();
  }
  return str;
}

// String contains
inline bool contains(const std::string &str, const std::string &substr) {
  return str.find(substr) != std::string::npos;
}

inline bool contains(const std::string &str, char c) {
  return str.find(c) != std::string::npos;
}

// String starts/ends with
inline bool starts_with(const std::string &str, const std::string &prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

inline bool ends_with(const std::string &str, const std::string &suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// HTML parsing helpers
inline std::string extract_title_from_html(const std::string &html) {
  std::regex re("<title>(.*?)</title>", std::regex::icase);
  std::smatch m;
  if (std::regex_search(html, m, re)) {
    return m[1].str();
  }
  return "";
}

inline std::string extract_regex_group(const std::string &text,
                                       const std::string &pattern,
                                       size_t group = 1) {
  std::regex re(pattern);
  std::smatch m;
  if (std::regex_search(text, m, re)) {
    return m[group].str();
  }
  return "";
}

inline std::string strip_html_tags(const std::string &html) {
  std::string out = std::regex_replace(
      html, std::regex(R"(<script[\s\S]*?</script>)", std::regex::icase), " ");
  out = std::regex_replace(
      out, std::regex(R"(<style[\s\S]*?</style>)", std::regex::icase), " ");
  out = std::regex_replace(out, std::regex(R"(<[^>]+>)"), " ");
  out = replace_all(out, "&nbsp;", " ");
  out = replace_all(out, "&lt;", "<");
  out = replace_all(out, "&gt;", ">");
  out = replace_all(out, "&amp;", "&");
  out = std::regex_replace(out, std::regex(R"(\s+)"), " ");
  return trim(out);
}

inline std::string extract_meta_content(const std::string &html,
                                        const std::string &key) {
  std::regex re1("<meta[^>]*(?:name|property)=\"" + key +
                     "\"[^>]*content=\"(.*?)\"[^>]*>",
                 std::regex::icase);
  std::smatch m;
  if (std::regex_search(html, m, re1))
    return trim(m[1].str());
  std::regex re2("<meta[^>]*content=\"(.*?)\"[^>]*(?:name|property)=\"" + key +
                     "\"[^>]*>",
                 std::regex::icase);
  if (std::regex_search(html, m, re2))
    return trim(m[1].str());
  return "";
}

// Sanitize filename
inline std::string sanitize_filename(std::string filename) {
  const std::string invalid_chars = R"(/\:*?"<>|)";
  for (char c : invalid_chars) {
    std::replace(filename.begin(), filename.end(), c, '_');
  }
  // Remove ".." for safety
  size_t pos;
  while ((pos = filename.find("..")) != std::string::npos) {
    filename.replace(pos, 2, "__");
  }
  return filename;
}

// Safe string to number conversion
template <typename T>
inline std::optional<T> try_parse_number(const std::string &str) {
  try {
    if constexpr (std::is_same_v<T, int>) {
      return std::stoi(str);
    } else if constexpr (std::is_same_v<T, long>) {
      return std::stol(str);
    } else if constexpr (std::is_same_v<T, long long>) {
      return std::stoll(str);
    } else if constexpr (std::is_same_v<T, double>) {
      return std::stod(str);
    } else if constexpr (std::is_same_v<T, float>) {
      return std::stof(str);
    }
  } catch (...) {
    return std::nullopt;
  }
  return std::nullopt;
}

} // namespace shuati::utils
