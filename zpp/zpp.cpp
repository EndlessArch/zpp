#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
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

    std::expected<init::compile_env, std::exception> export_compile_envs() noexcept
    {
        init::compile_env env{};

        if (argv_.begin()->starts_with('-'))
            return std::unexpected<std::exception>("expected source directory");
        else {
            env.source_path_ = *argv_.begin();
            argv_.erase(argv_.begin());
        }
        // language version parsing
        if (const auto r = std::ranges::find_if(argv_,
            [](const auto& s) { return s.starts_with("-std="); }); r != argv_.end()) {
            // has version flag
            const auto ver = r->substr(strlen("-std="));
            std::cout << ver << '\n';

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
    auto toks = tok::tokenize_file(env.source_path_);
}

void run_build_conf(const std::filesystem::path& build_conf, const init::compile_env& env) noexcept {
    // TODO: someday.
}
} // ns init

void parse_zpp(const init::compile_env& env) noexcept {
    if (!std::filesystem::is_directory(env.source_path_)) {
        init::compile_zpp(env);
        return;
    }
    const auto build_zpp = env.source_path_ / "build.zpp";

    if (!std::filesystem::is_regular_file(build_zpp)) {
        std::cout << "Expected \'build.zpp\' at " << env.source_path_ << '\n';
        return;
    }

    init::run_build_conf(build_zpp, env);
}
} // ns zpp

int main(int c, char** v) {

    // exclude the program call
    zpp::pre_init::cl cmd{ c-1, v+1 };

    if (cmd.is_help() || c == 1) {
        std::cout <<
            "usage: zpp [PROJECT DIR] [OPTIONS]\n"
            "[OPTIONS]\n"
            "-h             : Show zpp compiler usage\n"
            "-std={VERSION} : Set the zpp compiler version\n"
            "Zpp Versions:\n"
            "   Zpp24\n"
            ;
        return 0;
    }

    if (!cmd.has_source()) {
        std::cout << "zpp source file is not given\n";
        return -1;
    }

    auto result = cmd.export_compile_envs();

    if (result.has_value()) {
        // std::cout << result.value() << '\n';
        zpp::parse_zpp(*result);
    }
    else {
        std::cerr << "Failed to parse arguments: " << result.error().what() << '\n';
    }

    return 0;
}