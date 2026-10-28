#include "core/Crash.h"

#include <windows.h>
#include <dbghelp.h>

#include <sstream>

#include "core/Util.h"

namespace sk::crash {

namespace {

LONG WINAPI OnCrash(EXCEPTION_POINTERS* ep) {
    std::string stamp = NowStamp();
    fs::path dump = LocalDataDir() / Widen("crash-" + stamp + ".dmp");
    HANDLE f = CreateFileW(dump.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), ep, FALSE};
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(f);
    }

    std::ostringstream os;
    os << "PLANTAGE code=0x" << std::hex << ep->ExceptionRecord->ExceptionCode << " adresse=" << ep->ExceptionRecord->ExceptionAddress
       << std::dec << "\n";
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitializeW(proc, ExeDir().c_str(), TRUE);
    CONTEXT ctx = *ep->ContextRecord;
    STACKFRAME64 sf{};
    sf.AddrPC.Offset = ctx.Rip;
    sf.AddrFrame.Offset = ctx.Rbp;
    sf.AddrStack.Offset = ctx.Rsp;
    sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 32; ++i) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, GetCurrentThread(), &sf, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr) ||
            !sf.AddrPC.Offset)
            break;
        char buf[sizeof(SYMBOL_INFO) + 512] = {};
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        DWORD64 disp = 0;
        os << "  #" << i << " ";
        if (SymFromAddr(proc, sf.AddrPC.Offset, &disp, sym)) os << sym->Name;
        else os << "0x" << std::hex << sf.AddrPC.Offset << std::dec;
        IMAGEHLP_LINE64 line{sizeof line};
        DWORD ldisp = 0;
        if (SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &ldisp, &line))
            os << "  (" << fs::path(line.FileName).filename().string() << ":" << line.LineNumber << ")";
        os << "\n";
    }
    SymCleanup(proc);
    Log(os.str());
    return EXCEPTION_EXECUTE_HANDLER;
}

}

void Install() { SetUnhandledExceptionFilter(OnCrash); }

}
