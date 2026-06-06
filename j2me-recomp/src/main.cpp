#include <iostream>
#include <string>
#include <filesystem>
#include "jar_loader.h"
#include "toml_config.h"
#include "bytecode_lifter.h"
#include "cpp_emitter.h"
#include "patch_applier.h"

namespace fs = std::filesystem;

static void usage(const char* argv0) {
    std::cerr <<
        "Usage: " << argv0 << "\n"
        "  --jar    <path/to/game.jar>\n"
        "  --main   <com/example/Main>\n"
        "  --toml   <path/to/game.toml>\n"
        "  --output <output_directory>\n"
        "  [--verbose]\n";
}

int main(int argc, char** argv) {
    std::string jar_path, main_class, toml_path, output_dir;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--jar"     && i+1 < argc) jar_path    = argv[++i];
        else if (a == "--main"    && i+1 < argc) main_class  = argv[++i];
        else if (a == "--toml"    && i+1 < argc) toml_path   = argv[++i];
        else if (a == "--output"  && i+1 < argc) output_dir  = argv[++i];
        else if (a == "--verbose")               verbose      = true;
        else { usage(argv[0]); return 1; }
    }

    if (jar_path.empty() || main_class.empty() || output_dir.empty()) {
        usage(argv[0]); return 1;
    }

    // ── Load config ───────────────────────────────────────────────────────────
    TomlConfig cfg;
    if (!toml_path.empty()) {
        if (!cfg.load(toml_path)) {
            std::cerr << "[recomp] Failed to load config: " << toml_path << "\n";
            return 1;
        }
    }

    // ── Load JAR ──────────────────────────────────────────────────────────────
    std::cout << "[recomp] Loading JAR: " << jar_path << "\n";
    JarLoader loader;
    if (!loader.load(jar_path)) {
        std::cerr << "[recomp] Failed to load JAR\n";
        return 1;
    }

    fs::create_directories(output_dir);

    // ── Apply patches & stubs from TOML ───────────────────────────────────────
    PatchApplier patcher(cfg);

    // ── Lift + emit each class ────────────────────────────────────────────────
    int emitted = 0;
    int skipped = 0;
    for (auto& cls : loader.classes()) {
        std::cout << "  [lift] " << cls.name
                  << " (" << cls.methods.size() << " methods)\n";
        std::cout.flush();

        try {
            patcher.apply(cls);
        } catch (const std::exception& e) {
            std::cerr << "  [patch error] " << cls.name << ": " << e.what() << "\n";
        }

        std::vector<IRMethod> ir;
        try {
            BytecodeLifter lifter(cls);
            ir = lifter.lift();
        } catch (const std::exception& e) {
            std::cerr << "  [lift error] " << cls.name << ": " << e.what() << " — skipping\n";
            ++skipped; continue;
        } catch (...) {
            std::cerr << "  [lift crash] " << cls.name << " — skipping\n";
            ++skipped; continue;
        }

        std::cout << "  [lifted ok] " << cls.name << "\n"; std::cout.flush();

        if (verbose) {
            for (auto& m : ir)
                std::cout << "    [method] " << m.method_name
                          << " instrs=" << m.instrs.size() << "\n";
            std::cout.flush();
        }

        std::string out_path = output_dir + "/" + cls.flat_name() + ".cpp";

        std::cout << "  [emitting] " << cls.name << "\n"; std::cout.flush();
        try {
            CppEmitter emitter(ir, cfg);
            if (!emitter.emit(out_path)) {
                std::cerr << "  [emit error] " << cls.name << "\n";
                ++skipped; continue;
            }
        } catch (const std::exception& e) {
            std::cerr << "  [emit crash] " << cls.name << ": " << e.what() << " — skipping\n";
            ++skipped; continue;
        } catch (...) {
            std::cerr << "  [emit crash] " << cls.name << " — skipping\n";
            ++skipped; continue;
        }

        std::cout << "  [ok] " << cls.name << " → " << out_path << "\n";
        std::cout.flush();
        ++emitted;
    }

    // ── Emit vtable dispatch + runtime glue header ────────────────────────────
    CppEmitter::emit_glue(output_dir, loader.classes(), cfg);

    std::cout << "[recomp] Done. Emitted " << emitted << " class(es)";
    if (skipped) std::cout << ", skipped " << skipped << " (see errors above)";
    std::cout << " → " << output_dir << "\n";
    return 0;
}
