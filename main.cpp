#include "ImplicitConversionVisitor.h"
#include "RiskScorer.h"
#include "FixItGenerator.h"
#include "ContextCollector.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include "SimpleCompilationDB.h"

#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>

#include <iostream>
#include <memory>
#include <filesystem>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Command-line options
static cl::OptionCategory HazardAnalyzerCategory("Implicit Conversion Hazard Analyzer options");

static cl::opt<int> RiskThreshold(
    "risk-threshold",
    cl::desc("Minimum risk score to report (0-100, default: 50)"),
    cl::init(50),
    cl::cat(HazardAnalyzerCategory));

static cl::opt<bool> SummaryOnly(
    "summary-only",
    cl::desc("Only print summary statistics, not individual findings"),
    cl::init(false),
    cl::cat(HazardAnalyzerCategory));

static cl::opt<bool> JsonOutput(
    "json",
    cl::desc("Output findings as JSON"),
    cl::init(false),
    cl::cat(HazardAnalyzerCategory));

static cl::opt<bool> ShowAll(
    "show-all",
    cl::desc("Show all conversions including low-risk ones"),
    cl::init(false),
    cl::cat(HazardAnalyzerCategory));

static cl::opt<std::string> CompileCommandsDir(
    "p",
    cl::desc("Build directory containing compile_commands.json"),
    cl::init("."),
    cl::cat(HazardAnalyzerCategory));

static cl::list<std::string> SourceFiles(
    cl::Positional,
    cl::desc("<source files...>"),
    cl::OneOrMore,
    cl::cat(HazardAnalyzerCategory));

/// ASTConsumer that runs our visitor on each translation unit
class HazardAnalyzerConsumer : public ASTConsumer {
public:
    HazardAnalyzerConsumer(ASTContext& context,
                            RiskScorer& scorer,
                            FixItGenerator& fixer,
                            int threshold)
        : visitor_(context, scorer, fixer, threshold) {}

    void HandleTranslationUnit(ASTContext& context) override {
        visitor_.TraverseDecl(context.getTranslationUnitDecl());

        if (JsonOutput) {
            printJson();
        } else {
            if (!SummaryOnly) {
                visitor_.printFindings();
            }
            visitor_.printSummary();
        }
    }

private:
    ImplicitConversionVisitor visitor_;

    void printJson() const {
        const auto& findings = visitor_.getFindings();
        std::cout << "[\n";
        for (size_t i = 0; i < findings.size(); ++i) {
            const auto& f = findings[i];
            std::cout << "  {\n";
            std::cout << "    \"file\": \"" << escapeJson(f.file) << "\",\n";
            std::cout << "    \"line\": " << f.line << ",\n";
            std::cout << "    \"column\": " << f.column << ",\n";
            std::cout << "    \"source_type\": \"" << escapeJson(f.source_type) << "\",\n";
            std::cout << "    \"target_type\": \"" << escapeJson(f.target_type) << "\",\n";
            std::cout << "    \"conversion_kind\": \"" << escapeJson(f.conversion_kind) << "\",\n";
            std::cout << "    \"risk_score\": " << f.risk_score << ",\n";
            std::cout << "    \"risk_level\": \"" << escapeJson(RiskScorer::levelLabel(f.risk_level)) << "\",\n";
            std::cout << "    \"explanation\": \"" << escapeJson(f.explanation) << "\",\n";
            std::cout << "    \"fix_suggestion\": \"" << escapeJson(f.fix_suggestion) << "\"\n";
            std::cout << "  }";
            if (i + 1 < findings.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "]\n";
    }

    static std::string escapeJson(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }
        return result;
    }
};

/// FrontendAction that creates our consumer
class HazardAnalyzerAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(
        CompilerInstance& CI, StringRef) override {
        scorer_ = std::make_unique<RiskScorer>();
        fixer_ = std::make_unique<FixItGenerator>();
        return std::make_unique<HazardAnalyzerConsumer>(
            CI.getASTContext(), *scorer_, *fixer_,
            ShowAll ? 0 : RiskThreshold);
    }

private:
    std::unique_ptr<RiskScorer> scorer_;
    std::unique_ptr<FixItGenerator> fixer_;
};

/// FrontendActionFactory to create our action
class HazardAnalyzerFactory : public FrontendActionFactory {
public:
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<HazardAnalyzerAction>();
    }
};

int main(int argc, const char** argv) {
    cl::HideUnrelatedOptions({&HazardAnalyzerCategory});
    cl::ParseCommandLineOptions(argc, argv,
        "Implicit Conversion Hazard Analyzer\n"
        "Analyzes C/C++ code for dangerous implicit type conversions.\n");

    // Resolve compile commands directory
    std::string build_path = CompileCommandsDir;

    // Load compile database
    std::string error_message;
    auto comp_db_ptr = JSONCompilationDatabase::loadFromDirectory(
        build_path, error_message);

    if (!comp_db_ptr) {
        // If no compile_commands.json, try creating a simple one
        // by using -xc -std=c11 defaults
        llvm::errs() << "Warning: Could not load compile_commands.json from "
                      << build_path << ": " << error_message << "\n";
        llvm::errs() << "Using default compilation flags: -xc -std=c11\n\n";
    }

    // Resolve source files to absolute paths
    std::vector<std::string> abs_sources;
    for (const auto& f : SourceFiles) {
        abs_sources.push_back(std::filesystem::absolute(f).string());
    }

    std::unique_ptr<clang::tooling::CompilationDatabase> comp_db;
    if (comp_db_ptr) {
        comp_db = std::move(comp_db_ptr);
    } else {
        // Fallback: use simple in-memory compilation database with default flags
        // This allows running on single files without compile_commands.json
        std::vector<std::string> flags = {
            "-xc", "-std=c11", "-Wall",
            "-resource-dir=" DEFAULT_RESOURCE_DIR,
            "-isystem", "/usr/include"
        };
        // Check if file extension suggests C++
        auto hasExtension = [](const std::string& s, const std::string& ext) {
            return s.size() >= ext.size() &&
                   s.compare(s.size() - ext.size(), ext.size(), ext) == 0;
        };
        for (const auto& src : abs_sources) {
            if (hasExtension(src, ".cpp") || hasExtension(src, ".cc") ||
                hasExtension(src, ".cxx") || hasExtension(src, ".hpp")) {
                flags = {
                    "-xc++", "-std=c++17", "-Wall",
                    "-resource-dir=" DEFAULT_RESOURCE_DIR,
                    "-isystem", "/usr/include"
                };
                break;
            }
        }
        comp_db = std::make_unique<SimpleCompilationDatabase>(abs_sources, flags);
    }

    ClangTool tool(*comp_db, abs_sources);

    // Inject resource-dir flag so ClangTool can find built-in headers
    // This is needed because our tool binary is not named 'clang' and
    // ClangTool derives resource dir from the executable path
    std::string resource_dir_arg = "-resource-dir=";
    resource_dir_arg += DEFAULT_RESOURCE_DIR;
    tool.appendArgumentsAdjuster(
        getInsertArgumentAdjuster(
            resource_dir_arg.c_str(),
            ArgumentInsertPosition::BEGIN));

    std::cerr << "=== Implicit Conversion Hazard Analyzer ===\n";
    std::cerr << "Risk threshold: " << (ShowAll ? 0 : RiskThreshold) << "\n";
    std::cerr << "Files to analyze: " << abs_sources.size() << "\n";
    std::cerr << "==========================================\n\n";

    HazardAnalyzerFactory factory;
    return tool.run(&factory);
}
