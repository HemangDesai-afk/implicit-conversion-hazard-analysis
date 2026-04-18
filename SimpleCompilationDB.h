#ifndef SIMPLE_COMPILATION_DB_H
#define SIMPLE_COMPILATION_DB_H

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <vector>
#include <string>

/// A simple in-memory compilation database for fallback use
class SimpleCompilationDatabase : public clang::tooling::CompilationDatabase {
public:
    SimpleCompilationDatabase(const std::vector<std::string>& source_files,
                               const std::vector<std::string>& default_flags) {
        for (const auto& file : source_files) {
            clang::tooling::CompileCommand cmd;
            cmd.Directory = ".";
            cmd.Filename = file;
            cmd.CommandLine = {"clang"};
            cmd.CommandLine.insert(cmd.CommandLine.end(),
                                    default_flags.begin(),
                                    default_flags.end());
            cmd.CommandLine.push_back(file);
            commands_.push_back(cmd);
        }
    }

    std::vector<clang::tooling::CompileCommand>
    getCompileCommands(clang::StringRef) const override {
        return commands_;
    }

    std::vector<std::string> getAllFiles() const override {
        std::vector<std::string> files;
        for (const auto& cmd : commands_) {
            files.push_back(cmd.Filename);
        }
        return files;
    }

private:
    std::vector<clang::tooling::CompileCommand> commands_;
};

#endif // SIMPLE_COMPILATION_DB_H
