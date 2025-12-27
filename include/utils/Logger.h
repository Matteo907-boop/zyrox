#ifndef LOGGER_H
#define LOGGER_H

#include <cstdlib>
#include <format>
#include <llvm/Demangle/Demangle.h>

#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/Debug.h>

using namespace llvm;

class Logger
{

  public:
    template <typename... _Args>
    static void Info(const std ::format_string<_Args...> fmt, _Args &&...args)
    {
        errs() << "[INFO] " << std ::format(fmt, std ::forward<_Args>(args)...)
               << '\n';
    }

    template <typename... _Args>
    __attribute__((noreturn)) static void
    Error(const std ::format_string<_Args...> fmt, _Args &&...args)
    {
        errs() << "[ERR] " << std ::format(fmt, std ::forward<_Args>(args)...)
               << '\n';
        exit(1);
    }

    template <typename... _Args>
    static void Warn(const std ::format_string<_Args...> fmt, _Args &&...args)
    {
        errs() << "[WARN] " << std ::format(fmt, std ::forward<_Args>(args)...)
               << '\n';
    }
};

#endif