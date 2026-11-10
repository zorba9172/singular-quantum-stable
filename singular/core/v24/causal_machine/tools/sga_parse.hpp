// SINGULAR Ω v24 causal_machine — .sga (Singular Assembly) front end.
// Lexes .sga source and normalizes both the symbolic and the ASCII form into
// canonical statements {mnemonic, args}. Grammar: OMEGA_SYMBOLIC_ASSEMBLY_GRAMMAR.md.
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "rv64_isa.hpp"

namespace sga {

struct Stmt {
    std::string op;                 // canonical mnemonic, pseudo, label definition (":name") or directive (".data")
    std::vector<std::string> args;  // registers, immediates, label names, "imm(reg)" memory operands
    int line = 0;
};

struct Error : std::runtime_error {
    Error(int line, const std::string& m) : std::runtime_error("line " + std::to_string(line) + ": " + m) {}
};

inline std::vector<std::string> split_tokens(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == ',' || c == '\r') {
            if (!cur.empty()) out.push_back(cur), cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

inline bool is_int(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    if (s.size() > i + 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        for (size_t k = i + 2; k < s.size(); ++k)
            if (!isxdigit(static_cast<unsigned char>(s[k]))) return false;
        return true;
    }
    for (size_t k = i; k < s.size(); ++k)
        if (!isdigit(static_cast<unsigned char>(s[k]))) return false;
    return true;
}

inline long long to_int(const std::string& s, int line) {
    if (!is_int(s)) throw Error(line, "expected integer, got '" + s + "'");
    bool neg = s[0] == '-';
    std::string body = (s[0] == '-' || s[0] == '+') ? s.substr(1) : s;
    unsigned long long v = std::stoull(body, nullptr, 0);
    return neg ? -static_cast<long long>(v) : static_cast<long long>(v);
}

inline bool is_reg(const std::string& s) { return rv64::parse_reg(s) >= 0; }

// Maps the symbolic binary operator of "xD ← xA OP B" to {R-type, I-type} mnemonics.
inline bool binop(const std::string& op, std::string& r, std::string& i) {
    static const char* t[][3] = {{"+", "add", "addi"}, {"-", "sub", "addi"}, {"&", "and", "andi"},
                                 {"|", "or", "ori"},   {"^", "xor", "xori"}, {"<<", "sll", "slli"},
                                 {">>", "srl", "srli"}, {"*", "mul", ""}};
    for (auto& e : t)
        if (op == e[0]) {
            r = e[1];
            i = e[2];
            return true;
        }
    return false;
}

inline std::string branch_for(const std::string& rel) {
    if (rel == "≠" || rel == "!=") return "bne";
    if (rel == "=" || rel == "==") return "beq";
    if (rel == "<") return "blt";
    if (rel == "≥" || rel == ">=") return "bge";
    if (rel == "<u") return "bltu";
    if (rel == "≥u" || rel == ">=u") return "bgeu";
    return "";
}

// "[xS + imm]" or "[xS]" (already split into tokens) -> "imm(xS)".
inline std::string mem_operand(const std::vector<std::string>& t, size_t& k, int line) {
    if (t[k].empty() || t[k][0] != '[') throw Error(line, "expected memory operand '[reg + imm]'");
    std::string reg = t[k].substr(1), imm = "0";
    bool closed = !reg.empty() && reg.back() == ']';
    if (closed) reg.pop_back();
    ++k;
    if (!closed) {
        if (k + 1 < t.size() && (t[k] == "+" || t[k] == "-") && !t[k + 1].empty() && t[k + 1].back() == ']') {
            imm = (t[k] == "-" ? "-" : "") + t[k + 1].substr(0, t[k + 1].size() - 1);
            k += 2;
        } else {
            throw Error(line, "malformed memory operand");
        }
    }
    if (!is_reg(reg) || !is_int(imm)) throw Error(line, "malformed memory operand");
    return imm + "(" + reg + ")";
}

inline Stmt normalize_symbolic(const std::vector<std::string>& t, int line) {
    static const std::string arrow = "←";
    size_t k = 0;
    if (t[0] == "↩") return {"halt", {}, line};
    if (t[0] == "↷") {
        if (t.size() != 2) throw Error(line, "'↷ label' expects one label");
        return {"j", {t[1]}, line};
    }
    if (t[0] == "↻") {
        if (t.size() != 6 || t[2] != "if") throw Error(line, "'↻ label if xA REL xB' expected");
        std::string b = branch_for(t[4]);
        if (b.empty()) throw Error(line, "unknown relation '" + t[4] + "'");
        return {b, {t[3], t[5], t[1]}, line};
    }
    if (t[0][0] == '[') {  // store: [xS + imm] ← xT
        std::string m = mem_operand(t, k, line);
        if (k + 2 != t.size() || t[k] != arrow) throw Error(line, "store form is '[xS + imm] ← xT'");
        return {"sd", {t[k + 1], m}, line};
    }
    if (t.size() < 3 || t[1] != arrow) throw Error(line, "expected 'xD ← expression'");
    const std::string& rd = t[0];
    std::vector<std::string> r(t.begin() + 2, t.end());
    if (const omega_insn_desc_t* d = rv64::find_omega(r[0])) {
        std::vector<std::string> a = {rd};
        a.insert(a.end(), r.begin() + 1, r.end());
        return {d->mnemonic, a, line};
    }
    if (r[0][0] == '[') {
        size_t kk = 0;
        std::string m = mem_operand(r, kk, line);
        if (kk != r.size()) throw Error(line, "trailing tokens after load operand");
        return {"ld", {rd, m}, line};
    }
    if (r.size() == 1) {
        if (is_reg(r[0])) return {"mv", {rd, r[0]}, line};
        if (is_int(r[0])) return {"li", {rd, r[0]}, line};
        if (r[0][0] == '&') return {"la", {rd, r[0].substr(1)}, line};
        throw Error(line, "cannot interpret '" + r[0] + "'");
    }
    if (r.size() == 3) {
        std::string rop, iop;
        if (!binop(r[1], rop, iop)) throw Error(line, "unknown operator '" + r[1] + "'");
        if (is_reg(r[2])) return {rop, {rd, r[0], r[2]}, line};
        if (is_int(r[2]) && !iop.empty()) {
            std::string imm = r[2];
            if (r[1] == "-") imm = std::to_string(-to_int(imm, line));
            return {iop, {rd, r[0], imm}, line};
        }
        throw Error(line, "operator '" + r[1] + "' does not accept '" + r[2] + "'");
    }
    throw Error(line, "unsupported expression");
}

inline std::vector<Stmt> parse(const std::string& src) {
    std::vector<Stmt> out;
    size_t pos = 0;
    int line = 0;
    while (pos <= src.size()) {
        size_t nl = src.find('\n', pos);
        std::string raw = src.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? src.size() + 1 : nl + 1;
        ++line;
        size_t sc = raw.find(';');
        if (sc != std::string::npos) raw = raw.substr(0, sc);
        std::vector<std::string> t = split_tokens(raw);
        while (!t.empty() && t[0].size() > 1 && t[0].back() == ':') {
            std::string name = t[0].substr(0, t[0].size() - 1);
            out.push_back({":" + name, {}, line});
            t.erase(t.begin());
        }
        if (t.empty()) continue;
        if (t[0][0] == '.') {
            out.push_back({t[0], std::vector<std::string>(t.begin() + 1, t.end()), line});
            continue;
        }
        bool symbolic = t[0] == "↩" || t[0] == "↷" || t[0] == "↻" || t[0][0] == '[' || (t.size() > 1 && t[1] == "←");
        if (symbolic) {
            out.push_back(normalize_symbolic(t, line));
        } else {
            Stmt s{t[0], std::vector<std::string>(t.begin() + 1, t.end()), line};
            if (const omega_insn_desc_t* d = rv64::find_omega(s.op)) s.op = d->mnemonic;
            out.push_back(s);
        }
    }
    return out;
}

} // namespace sga
