#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace zpp {
namespace init {
struct compile_env {
    enum class ZppVersion {
        Zpp24
    } target_source_version_;

    std::filesystem::path source_path_;

    compile_env() : target_source_version_(ZppVersion::Zpp24), source_path_{} {}

    friend std::ostream& operator<<(std::ostream& os, const compile_env& self) noexcept {
        switch (self.target_source_version_) {
        case ZppVersion::Zpp24:
            std::cout << "Zpp Version: Zpp24,\n";
        default:
            ;
        }
        std::cout << "Source Path: " << self.source_path_.string();
        return os;
    }
};
}

namespace pre_init {
// cmdline
struct cl {
    std::vector<std::string> argv_;

    cl(int c, char** v) noexcept : argv_{ v, v + c } {}

    bool is_help() const noexcept {
        return std::ranges::any_of(argv_,
           [](const std::string& s) -> bool {
               return s == "-h";
           });
    }

    // let only source file doesn't start with - (flag prefix)
    bool has_source() const noexcept {
        return std::ranges::any_of(argv_,
           [](const std::string& s) -> bool {
               return s[0] != '-';
           });
    }

    std::expected<init::compile_env, std::exception>
    export_compile_envs(wchar_t* zpp_pth) noexcept {
        init::compile_env env{};

        if (!has_source()) {
            env.source_path_ = std::filesystem::path(zpp_pth);
            free(zpp_pth); // allocated from main
        }
        else {
            env.source_path_ = *argv_.begin();
            argv_.erase(argv_.begin()); // the flag is used, so drop it
        }
        // language version parsing
        if (const auto r = std::ranges::find_if(argv_,
            [](const auto& s) { return s.starts_with("-std="); }); r != argv_.end()) {
            // has version flag
            const auto ver = r->substr(strlen("-std="));

            if(ver == "Zpp24")
                env.target_source_version_ = init::compile_env::ZppVersion::Zpp24;
            else
                return std::unexpected<std::exception>("unknown language version");
        }

        ; // other options parsing here...

        return env;
    }
};

} // ns pre_init

namespace tok {
enum class Token {
    Unknown,
    Eof,
    Identifier,
    Literal,
    Operator,
    Separator, // ::
    TypeOf, // :
    Paren, // ()
    Bracket, // {}
    Comma,
    From // from
};

constexpr auto stringify_tok(Token t) noexcept -> std::string
{
    switch(t)
    {
    case Token::Unknown:
        return "Token::Unknown";
    case Token::Eof:
        return "Token::Eof";
    case Token::Identifier:
        return "Token::Identifier";
    case Token::Literal:
        return "Token::Literal";
    case Token::Operator:
        return "Token::Operator";
    case Token::Separator:
        return "Token::Separator";
    case Token::TypeOf:
        return "Token::TypeOf";
    case Token::Paren:
        return "Token::Paren";
    case Token::Bracket:
        return "Token::Bracket";
    case Token::Comma:
        return "Token::Comma";
    case Token::From:
        return "Token::From";
    default:
        ;
    }
    return "Token::<error-type>";
}

namespace details {
auto _readWord(std::ifstream& ifs)
    -> std::pair<tok::Token, std::string> {
    using tok::Token;

    static std::string buf{}, ret;
    ret = {};
CHK_BUF:
    if (buf.empty()) {
        std::string _s;
        std::getline(ifs, _s);
        buf = _s;
    
        if (ifs.eof())
            return { Token::Eof, "" };

        goto CHK_BUF;
    }

    std::size_t it{};
    while (std::isspace(buf[it])) ++it;
    if (it) buf.erase(0, it);

    if (buf.empty()) goto CHK_BUF;

    if (buf[0] == '#') {
        buf = {};
        goto CHK_BUF;
    }

    if (buf[0] == '\"') {
        while (++it && buf[it] == '\"' || buf[it] == EOF) {}
        if (buf[it] == EOF) {
            ret = std::move(buf);
            // buf.erase(); // already moved
            return { Token::Literal, ret };
        }
        it += 1;
        ret = buf.substr(0, it);
        buf.erase(0, it);
        return { Token::Literal, ret };
    }

    it = 0;
    while (std::isdigit(buf[it])) ++it;

    // digit
    if (it && !std::isalpha(buf[it])) {
        ret = buf.substr(0, it);
        buf.erase(0, it);
        return { Token::Literal, ret };
    }

    // identifier
    while (std::isalnum(buf[it]) || buf[it] == '_') ++it;
    if (it) {
        ret = buf.substr(0, it);
        buf.erase(0, it);
        if (ret == "from") return { Token::From, "from" };
        return { Token::Identifier, ret };
    }

    // it = 0; // and should be
    if (!buf.empty()) {
        if (buf[0] == ':') {
            if (buf.size() >= 2 && buf[1] == ':') {
                buf.erase(0, 2);
                return { Token::Separator, "::" };
            }
            buf.erase(0, 1);
            return { Token::TypeOf, ":" };
        }
        if (buf[0] == '(' || buf[0] == ')') {
            auto c = buf[0];
            buf.erase(0, 1);
            return { Token::Paren, {c} };
        }
        if (buf[0] == '{' || buf[0] == '}') {
            auto c = buf[0];
            buf.erase(0, 1);
            return { Token::Bracket, {c} };
        }
        if (buf[0] == ',') {
            auto c = buf[0];
            buf.erase(0, 1);
            return { Token::Comma, {c} };
        }

        ret += buf[0];
        buf.erase(0, 1);
        auto [t, w] = _readWord(ifs);
        return { Token::Unknown,
            t == Token::Unknown ? ret + w : ret };
    }
    return _readWord(ifs);
}
} // ns details

auto tokenize_file(const std::filesystem::path& file_path) noexcept ->
std::expected<std::vector<std::pair<Token, std::string>>, std::exception> {
    std::ifstream ifs(file_path, std::ios::in);
    std::string s;
    
    if (!ifs.is_open())
        return std::unexpected<std::exception>(
            ("Failed to open file, " + file_path.string() + '\n').c_str());

    auto readWord = [&]{ return details::_readWord(ifs); };

    std::vector<std::pair<Token, std::string>> toks{};
TOKENIZE_LOOP:
    if (auto [t, w] = readWord(); t != Token::Eof) {
        toks.emplace_back(t, w);
        // std::cout << w << '\n';
        goto TOKENIZE_LOOP;
    }
    return toks;
}
} // ns tok

namespace code {

class CodeBlock {};

class AST {
public:
    AST() noexcept = default;
    AST(auto&&) noexcept {}
    virtual ~AST() noexcept = default;

    AST& operator=(this auto&&, auto&&) noexcept {
        return *this;
    }

    virtual CodeBlock gen_code() noexcept { return {}; }
};

class Expr : public AST {
public:
    ~Expr() override {}
    template <typename T>
    Expr& operator=(T&& rhs) noexcept {
        return std::forward<T>(*this);
    }

    CodeBlock gen_code() noexcept override {
        return CodeBlock{};
    }
};

class Function : public AST {
public:
    using farg_t = std::vector<std::pair<std::string, std::string>>;

    const std::string name_;
    const std::string ret_ty_;
    const farg_t farg_;
    Function(std::string&& name, std::string&& ret_ty, farg_t&& args)
        : AST{}, name_(std::move(name)), ret_ty_(std::move(ret_ty)), farg_(std::move(args))
    {}
    ~Function() override {}

    CodeBlock gen_code() noexcept override {
        return CodeBlock{};
    }
};

class Namespace : public AST {
public:
    ~Namespace() override {}

    CodeBlock gen_code() noexcept override {
        return CodeBlock{};
    }
};

////

#define __MK_EXC(str) std::exception{(str).c_str()}
#define EXPECTED(t1, t2) std::unexpected(__MK_EXC("Expected " + zpp::tok::stringify_tok(t1) + ", but " + zpp::tok::stringify_tok(t2)))

////

template <typename E>
class LookUp {
    std::vector<E> r_;

    typename std::vector<E>::iterator i_;
public:

    LookUp(std::vector<E>&& v) : r_{ std::move(v) }, i_{ r_.begin() } {}

    bool empty() const noexcept {
        return i_ == r_.end();
    }

    std::optional<E> look() const noexcept {
        if (i_ == r_.end()) return {};
        return *i_;
    }

    E drop() noexcept {
        return *i_++;
    }
};

auto make_codeblocks(auto&& tokens) noexcept
    -> std::expected<std::vector<std::unique_ptr<AST>>, std::exception> {
    using namespace zpp::tok;

    using ve_t = std::pair<Token, std::string>;

    LookUp<ve_t> lookUp{ std::move(tokens) };

    // no auto, for language server's easy process
    auto _expect = [&lookUp](Token e = Token::Unknown) noexcept
        -> std::expected<LookUp<ve_t>, std::exception> {
        ;
        if (e == Token::Unknown) {
            if (lookUp.empty())
                return std::unexpected(std::exception{ "EOF" });
            return lookUp;
        }
        if (auto l = lookUp.look(); l && l->first != e)
            return EXPECTED(e, l->first);
        return lookUp;
    };

    auto expect = [&_expect](Token e = Token::Unknown) noexcept
        -> std::expected<ve_t, std::exception> {
        auto v = _expect(e);
        return v.has_value() ? std::expected<ve_t, std::exception>(v->drop()) : std::unexpected(v.error());
    };

    //constexpr auto expect_farg = [&](auto& buf) noexcept
    //    -> std::expected<std::pair<std::string, std::string>, std::exception>
    //    {
    //        std::string name, type;
    //        buf = expect();
    //        if (!buf.has_value()) return buf.error();
    //        name = *buf;

    //        buf = expect(Token::TypeOf);
    //        if (!buf.has_value()) return buf.error();
    //        type = *buf;
    //        return std::pair{ name, type };
    //    };

    auto expect_fargs = [&expect](auto& buf) noexcept
        -> std::expected<
        std::vector<std::pair<std::string, std::string>>,
        std::exception> {
        // func ( arg : ty , ... )
        // 1~^ 2^ 3~^ 4 5^ 6 7~^ 8

        std::vector<std::pair<std::string, std::string>> ret{};
        std::pair<std::string, std::string> pbuf;

        //// 2
        //buf = expect();
        //if (!buf->has_value()) return buf->error();
        //if (buf->first == Token::Paren)
        //    if (buf->second == ")")
        //        return std::unexpected(std::exception{ "Expected '('" });

        buf = expect();
        if (!buf.has_value()) return std::unexpected(buf.error());
        if (buf->first == Token::Paren) { // 8
            if (buf->second == "(")
                return std::unexpected(std::exception{ "Expected ')'" });
            return {}; // non-argument function
        }
        // 7
    PARSE_FARG:
        pbuf = {};
        // 3
        if (buf->first != Token::Identifier)
            return EXPECTED(Token::Identifier, buf->first);
        pbuf.first = std::move(buf->second);

        // 4
        buf = expect(Token::TypeOf);
        if (!buf.has_value()) return std::unexpected(buf.error());

        // 5
        buf = expect(Token::Identifier);
        if (!buf.has_value()) return std::unexpected(buf.error());
        pbuf.second = std::move(buf->second);
        ret.push_back(std::move(pbuf));

        // 6
        buf = expect();
        if (!buf.has_value()) return std::unexpected(buf.error());

        if (buf->first == Token::Comma) {
            buf = expect(Token::Identifier);
            goto PARSE_FARG;
        }
        else {
            if (buf->first == Token::Paren) {
                if (buf->second == "(")
                    return std::unexpected(std::exception{ "Expected ')'" });
                return ret;
            }
            return std::unexpected(
                std::exception{ ("Unexpected " + stringify_tok(buf->first) + ", expected ')'").c_str() });
        }
    };

    auto expect_type = [&](auto& buf) noexcept
    -> std::expected<std::pair<Token, std::string>, std::exception> {
        return expect(Token::Identifier);
    };

    auto glob_ns = code::Namespace {};

    // namespace or class or function
    auto buf = expect(Token::Identifier);
    if (!buf.has_value()) return std::unexpected(buf.error());

    std::string name = buf->second;

    buf = expect();
    if (!buf.has_value()) return std::unexpected(buf.error());
    if(buf->first == Token::Paren) {
        if (buf->second != "(")
            return std::unexpected(std::exception{ "Unexpected ')'" });
        auto ve = expect_fargs(buf);
        if (!ve.has_value()) return std::unexpected(ve.error());
        auto args = std::move(*ve);

        buf = expect(Token::TypeOf);
        if (!buf.has_value()) return std::unexpected(buf.error());

        buf = expect_type(buf);
        if (!buf.has_value()) return std::unexpected(buf.error());

        Function c_func{ std::move(name), std::move(buf->second), std::move(args) };

        // parse function body

        buf = expect(Token::Bracket);
        if (!buf.has_value()) return std::unexpected(buf.error());
        if (buf->second != "{") return std::unexpected(std::exception{ "Expected '{'" });

        buf = expect();
        if (!buf.has_value()) return std::unexpected(buf.error());

        // empty function
        if (auto l = lookUp.look(); l && l->first == Token::Bracket && l->second == "}") {
            std::cout << "Empty function\n";
        }
        else {
            std::cout << stringify_tok(l->first) << ": " << l->second << '\n';
        }
    }
    if(buf->first == Token::Separator) {
        std::string ns = std::move(name);

        buf = expect();
        if (!buf.has_value()) return std::unexpected(buf.error());
    PARSE_NS:
        if(buf->first == Token::Identifier) {
            ns += ';' + std::move(buf->second);
            buf = expect(Token::Separator);
            if (!buf.has_value()) return std::unexpected(buf.error());
            goto PARSE_NS;
        }
        if (buf->first == Token::Bracket)
            if (buf->second != "{")
                return std::unexpected(std::exception{ "Expected '{'" });

        // parse namespaces
        std::cout << "NAMESPACE: " << ns << '\n';
    }

    if(buf->first == Token::From || buf->first == Token::Bracket) {
        // parse class
        std::cout << "CLASS: " << name << '\n';
    }

    return {};
}

} // ns code
namespace init {

// not meaning the function does compile
int compile_zpp(const compile_env& env) noexcept {
    auto toks = tok::tokenize_file(env.source_path_);

    if(toks.has_value()) {
        //std::cout << toks.value().begin()->second << '\n';
        auto codes = code::make_codeblocks(std::move(toks.value()));
        if (!codes.has_value()) std::cout << codes.error().what() << '\n';
        return 0;
    }
    else {
        std::cerr << toks.error().what() << '\n';
        return -1;
    }

    return 0;
}

int run_build_conf(const init::compile_env& env) noexcept {
    // TODO: someday.

    return 0;
}
} // ns init

int parse_zpp(const init::compile_env& env) noexcept {
    using namespace std::literals;
    using namespace zpp::init;

    const auto& pth = env.source_path_;

    if (std::ranges::starts_with(std::views::reverse(pth.string()), std::views::reverse("build.zpp"sv)))
        return run_build_conf(env);

    return compile_zpp(env);
}
} // ns zpp

#include <Windows.h>
#include <tchar.h> // _T

int main(int c, char** v) {

    // exclude the program call
    zpp::pre_init::cl cmd{ c-1, v+1 };

    if (cmd.is_help() || c == 1) {
        std::cout <<
            "usage: zpp [SOURCE] [OPTIONS]\n"
            "[SOURCE]       : Either run build.zpp or compile *.zpp\n"
            "[OPTIONS]\n"
            "-h             : Show zpp compiler usage\n"
            "-std={VERSION} : Set the zpp compiler version\n"
            "Zpp Versions:\n"
            "   Zpp24\n"
            ;
        return 0;
    }

    wchar_t* data = (wchar_t*)malloc(sizeof(wchar_t) * MAX_PATH);

    if (!cmd.has_source()) {
        std::cout << "zpp source file is not given\n";

        const auto hInstance = GetModuleHandle(nullptr);

        WCHAR crDir[MAX_PATH]{};
        GetCurrentDirectory(MAX_PATH, (LPWSTR)crDir);

        OPENFILENAME ofn = {
            .lStructSize = sizeof(OPENFILENAME),
            .hwndOwner = nullptr,
            .hInstance = hInstance,
            .lpstrFilter = _T("build.zpp or any zpp file\0build.zpp;*.zpp\0\0"),
            .nFilterIndex = 1,
            .lpstrFile = (LPWSTR)data,
            .nMaxFile = (DWORD)wcslen(data),
            .lpstrFileTitle = nullptr,
            .lpstrInitialDir = crDir,
            .Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
        };
        ofn.lpstrFile[0] = '\0';
        if (GetOpenFileName(&ofn) != TRUE) {
            std::cerr << "Failed to open file\n";
            return -1;
        }
    }

    auto result = cmd.export_compile_envs(data);

    if (result.has_value()) {
        std::cout << result.value() << '\n';
        return zpp::parse_zpp(*result);
    }
    std::cerr << "Failed to parse arguments: " << result.error().what() << '\n';
    return -1;
}