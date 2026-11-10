// SINGULAR Ω v24 causal_machine — restricted YAML subset reader.
// Supports block maps, block lists ("- key: value" items) and plain scalars.
// No flow style, anchors, multi-line strings or quoting rules beyond stripping
// one pair of surrounding double quotes. Enough for isa/omega_isa.yaml.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace omega_yaml {

struct Node {
    enum Kind { Scalar, Map, List } kind = Scalar;
    std::string value;
    std::vector<std::pair<std::string, Node>> map;
    std::vector<Node> list;

    const Node& at(const std::string& key) const {
        for (const auto& kv : map)
            if (kv.first == key) return kv.second;
        throw std::runtime_error("yaml: missing key '" + key + "'");
    }
    bool has(const std::string& key) const {
        for (const auto& kv : map)
            if (kv.first == key) return true;
        return false;
    }
    const std::string& str(const std::string& key) const {
        const Node& n = at(key);
        if (n.kind != Scalar) throw std::runtime_error("yaml: key '" + key + "' is not a scalar");
        return n.value;
    }
    uint64_t u64(const std::string& key) const {
        const std::string& s = str(key);
        char* end = nullptr;
        unsigned long long v = std::strtoull(s.c_str(), &end, 0);
        if (end == s.c_str() || *end != '\0') throw std::runtime_error("yaml: key '" + key + "' is not an integer: " + s);
        return v;
    }
};

struct Line {
    int indent;
    std::string text;
    int number;
};

inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

inline std::vector<Line> tokenize(const std::string& text) {
    std::vector<Line> out;
    std::istringstream in(text);
    std::string raw;
    int number = 0;
    while (std::getline(in, raw)) {
        ++number;
        if (raw.find('\t') != std::string::npos) throw std::runtime_error("yaml: tab character on line " + std::to_string(number));
        std::string t = trim(raw);
        if (t.empty() || t[0] == '#') continue;
        size_t hash = raw.find(" #");
        if (hash != std::string::npos) raw = raw.substr(0, hash);
        int indent = static_cast<int>(raw.find_first_not_of(' '));
        out.push_back({indent, trim(raw), number});
    }
    return out;
}

inline std::string unquote(std::string v) {
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return v;
}

inline Node parse_block(std::vector<Line>& lines, size_t& i, int indent);

inline Node parse_map(std::vector<Line>& lines, size_t& i, int indent) {
    Node n;
    n.kind = Node::Map;
    while (i < lines.size() && lines[i].indent == indent) {
        const Line& ln = lines[i];
        if (ln.text.rfind("- ", 0) == 0) break;
        size_t colon = ln.text.find(':');
        if (colon == std::string::npos) throw std::runtime_error("yaml: expected 'key: value' on line " + std::to_string(ln.number));
        std::string key = trim(ln.text.substr(0, colon));
        std::string val = trim(ln.text.substr(colon + 1));
        ++i;
        if (!val.empty()) {
            Node s;
            s.value = unquote(val);
            n.map.emplace_back(key, std::move(s));
        } else {
            if (i >= lines.size() || lines[i].indent <= indent)
                throw std::runtime_error("yaml: empty block for key '" + key + "' on line " + std::to_string(ln.number));
            n.map.emplace_back(key, parse_block(lines, i, lines[i].indent));
        }
    }
    if (i < lines.size() && lines[i].indent > indent)
        throw std::runtime_error("yaml: unexpected indentation on line " + std::to_string(lines[i].number));
    return n;
}

inline Node parse_list(std::vector<Line>& lines, size_t& i, int indent) {
    Node n;
    n.kind = Node::List;
    while (i < lines.size() && lines[i].indent == indent && lines[i].text.rfind("- ", 0) == 0) {
        Line& ln = lines[i];
        ln.text = trim(ln.text.substr(2));
        ln.indent = indent + 2;
        if (ln.text.find(':') == std::string::npos) {
            Node s;
            s.value = unquote(ln.text);
            n.list.push_back(std::move(s));
            ++i;
        } else {
            n.list.push_back(parse_map(lines, i, indent + 2));
        }
    }
    return n;
}

inline Node parse_block(std::vector<Line>& lines, size_t& i, int indent) {
    if (lines[i].text.rfind("- ", 0) == 0) return parse_list(lines, i, indent);
    return parse_map(lines, i, indent);
}

inline Node parse_text(const std::string& text) {
    std::vector<Line> lines = tokenize(text);
    if (lines.empty()) throw std::runtime_error("yaml: empty document");
    size_t i = 0;
    Node root = parse_block(lines, i, lines[0].indent);
    if (i != lines.size()) throw std::runtime_error("yaml: trailing content on line " + std::to_string(lines[i].number));
    return root;
}

inline Node parse_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("yaml: cannot open " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return parse_text(ss.str());
}

} // namespace omega_yaml
