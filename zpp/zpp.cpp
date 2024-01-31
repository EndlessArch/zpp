#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
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
        };
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

    std::expected<init::compile_env, std::exception> export_compile_envs(wchar_t* zpp_pth) noexcept
    {
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
    Identifier,
    Literal,
    Operator,
    Separator
};

auto tokenize_file(const std::filesystem::path& file_path) noexcept ->
std::expected<std::vector<Token>, std::exception> {
    std::ifstream ifs(file_path, std::ios::in);
    std::string s;
    
    if (!ifs.is_open())
        return std::unexpected<std::exception>(("Failed to open file, " + file_path.string() + '\n').c_str());
    while (std::getline(ifs, s)) {
        std::cout << s << '\n';
    }
}
} // ns tok

namespace init {

// not meaning the function does compile
void compile_zpp(const init::compile_env& env) noexcept {
    std::cout << "compile_zpp() called\n";
    auto toks = tok::tokenize_file(env.source_path_);
}

void run_build_conf(const init::compile_env& env) noexcept {
    // TODO: someday.
    std::cout << "run_build_conf() called\n";

    return;
}
} // ns init

void parse_zpp(const init::compile_env& env) noexcept {
    using namespace std::literals;

    const auto& pth = env.source_path_;
    constexpr auto cvt_sv = [](auto&& r) -> std::string_view { return std::string_view(r.begin(), r.end()); };

    if (std::ranges::starts_with(std::views::reverse(pth.string()), std::views::reverse("build.zpp"sv)))
        init::run_build_conf(env);
    else
        init::compile_zpp(env);
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

        auto hInstance = GetModuleHandle(0);

        WCHAR crDir[MAX_PATH]{};
        GetCurrentDirectory(MAX_PATH, (LPWSTR)crDir);

        OPENFILENAME ofn = {
            .lStructSize = sizeof(OPENFILENAME),
            .hwndOwner = 0,
            .hInstance = hInstance,
            .lpstrFilter = _T("build.zpp or any zpp file\0build.zpp;*.zpp\0\0"),
            .nFilterIndex = 1,
            .lpstrFile = (LPWSTR)data,
            .nMaxFile = (DWORD)wcslen(data),
            .lpstrFileTitle = 0,
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
        zpp::parse_zpp(*result);
    }
    else {
        std::cerr << "Failed to parse arguments: " << result.error().what() << '\n';
    }

    return 0;
}