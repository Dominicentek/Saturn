#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>

extern void saturn_update();

#include "pc/platform.h"
#include "saturn/saturn_version.h"
#include "saturn/imgui/saturn_imgui_dynos.h"

#ifdef _WIN32
#include <io.h>
#define TERMINAL_CHECK getenv("MSYSTEM")
#else
#include <unistd.h>
#define TERMINAL_CHECK isatty(fileno(stdin))
#endif

int orig_stdout_fileno;
FILE* logfile;

void init_logger() {
    if (TERMINAL_CHECK) return;
    char filepath[1024];
    snprintf(filepath, 1024, "%s/latest.log", sys_user_path());
    logfile = freopen(filepath, "w", stdout);
}

void close_logger() {
    if (TERMINAL_CHECK) return;
    fclose(logfile);
}

// Crash handler implementation
// Based on https://github.com/AloUltraExt/sm64ex-alo/blob/master/src/pc/crash/crash_handler.c

#ifndef OSX_BUILD

#define ARRSIZE(x) (sizeof(x) / sizeof(*(x)))
#define PTR long long unsigned int)(uintptr_t
#define ALLOC(x) ((x*)memset(malloc(sizeof(x)), 0, sizeof(x)))
#define STRUCT(x) typedef struct x x; struct x
#define MEMSTR(x) x, sizeof(x) 

#define max(a, b) ((a) > (b) ? (a) : (b))

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
#else
#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>
#endif
#include <dlfcn.h>

#ifdef _WIN32
struct {
    int code;
    const char* error;
    const char* message;
} error_messages[] = {
    { EXCEPTION_ACCESS_VIOLATION,       "Segmentation Fault",       "The game tried to %s at address 0x%016llX." },
    { EXCEPTION_ARRAY_BOUNDS_EXCEEDED,  "Array Out Of Bounds",      "The game tried to access an element out of the array bounds." },
    { EXCEPTION_DATATYPE_MISALIGNMENT,  "Data Misalignment",        "The game tried to access misaligned data." },
    { EXCEPTION_BREAKPOINT,             "Breakpoint",               "The game reached a breakpoint." },
    { EXCEPTION_FLT_DENORMAL_OPERAND,   "Float Denormal Operand",   "The game tried to perform a floating point operation with a denormal operand." },
    { EXCEPTION_FLT_DIVIDE_BY_ZERO,     "Float Division By Zero",   "The game tried to divide a floating point number by zero." },
    { EXCEPTION_FLT_INEXACT_RESULT,     "Float Inexact Result",     "The game couldn't represent the result of a floating point operation as a decimal fraction." },
    { EXCEPTION_FLT_INVALID_OPERATION,  "Float Invalid Operation",  "The game tried to perform an invalid floating point operation." },
    { EXCEPTION_FLT_OVERFLOW,           "Float Overflow",           "An overflow occurred with a floating point number." },
    { EXCEPTION_FLT_STACK_CHECK,        "Float Stack Overflow",     "The game performed a floating point operation resulting in a stack overflow." },
    { EXCEPTION_FLT_UNDERFLOW,          "Float Underflow",          "An underflow occurred with a floating point number." },
    { EXCEPTION_ILLEGAL_INSTRUCTION,    "Illegal Instruction",      "The game tried to execute an invalid instruction." },
    { EXCEPTION_IN_PAGE_ERROR,          "Page Error",               "The game tried to %s at address 0x%016llX." },
    { EXCEPTION_INT_DIVIDE_BY_ZERO,     "Integer Division By Zero", "The game tried to divide an integer by zero." },
    { EXCEPTION_INT_OVERFLOW,           "Integer Overflow",         "An overflow occurred with an integer." },
    { EXCEPTION_PRIV_INSTRUCTION,       "Instruction Not Allowed",  "The game tried to execute an invalid instruction." },
    { EXCEPTION_STACK_OVERFLOW,         "Stack Overflow",           "The game performed an operation resulting in a stack overflow." },
    { 0,                                "Unknown Exception",        "An unknown exception occurred." },
};
#else
struct {
    int code;
    const char* error;
    const char* message;
} error_messages[] = {
    { SIGBUS,  "Bad Memory Access",    "The game tried to access memory out of bounds." },
    { SIGFPE,  "Floating Point Error", "The game tried to perform illegal arithmetic." },
    { SIGILL,  "Illegal Instruction",  "The game tried to execute an invalid instruction." },
    { SIGSEGV, "Segmentation Fault",   "The game tried to %s at address 0x%016llX." },
    { 0,       "Unknown Exception",    "An unknown exception occured." }
};
#endif

#ifdef _WIN32
STRUCT(Symbol) {
    struct Symbol* prev;
    struct Symbol* next;
             void* addr;
             char  name[128];
};

static int last_char_at(const char* str, char c) {
	int len = strlen(str);
	for (int i = len - 1; i >= 0; i--) {
		if (str[i] == c) return i; 
	}
	return -1;
}

static Symbol* load_symbols() {
    char symbol_path[1024];
    GetModuleFileName(NULL, symbol_path, 1023);
	int delim = last_char_at(symbol_path, '\\');
	if (delim == -1) delim = last_char_at(symbol_path, '/');
	symbol_path[delim] = 0;
	snprintf(symbol_path, 1024, "%s/symbols.map", symbol_path);
    FILE* f = fopen(symbol_path, "r");
    if (!f) return NULL;
    char buf[1024];
    Symbol* symbols = ALLOC(Symbol);
    Symbol* head = symbols;
    const char global_func_id[] = "(sec1)(fl0x00)(ty20)(scl2)(nx0)0x";
    const char static_func_id[] = "(sec1)(fl0x00)(ty20)(scl3)(nx0)0x";
    memcpy(symbols->name, MEMSTR("REF-ADDR"));
    while (fgets(buf, 1024, f)) {
        char no_space[1024];
        int ptr = 0;
        for (int i = 0; i < 1024 && buf[i]; i++) {
            if (buf[i] <= ' ') continue;
            no_space[ptr++] = buf[i];
        }
        no_space[ptr++] = 0;
        char* gid = strstr(no_space, global_func_id);
        char* sid = strstr(no_space, static_func_id);
        if (!gid && !sid) continue;
        Symbol* symbol = ALLOC(Symbol);
        head->next = symbol;
        symbol->prev = head;
        head = symbol;
        char* symbol_str = max(gid + sizeof(global_func_id) - 1, sid + sizeof(static_func_id) - 1);
        sscanf(symbol_str, "%016llX", (unsigned long long int*)&head->addr);
        snprintf(head->name, 128, "%s", symbol_str + 16);
        if (memcmp(head->name, MEMSTR("saturn_update")) == 0) {
            symbols->addr = saturn_update - (uintptr_t)head->addr;
        }
    }
    fclose(f);
    return symbols;
}

static void free_symbols(Symbol* symbols) {
    Symbol* head = symbols;
    while (head) {
        Symbol* next = head->next;
        free(head);
        head = next;
    }
}

static int _dladdr(void* addr, Dl_info* info, Symbol* symbols) {
    int return_code = dladdr(addr, info);
    if ((!return_code || !info->dli_sname) && symbols) {
        uintptr_t offset = (uintptr_t)symbols->addr;
        uintptr_t symbol_addr = (uintptr_t)(addr - offset);
        uintptr_t closest = UINTPTR_MAX;
        Symbol* head = symbols->next;
        while (head) {
            uintptr_t dist = symbol_addr - (uintptr_t)head->addr;
            if (closest > dist) {
                closest = dist;
                info->dli_saddr = (uintptr_t)head->addr - symbol_addr + addr;
                info->dli_sname = head->name;
            }
            head = head->next;
        }
    }
    return return_code;
}
#else
#define _dladdr(addr, info, symbols) dladdr(addr, info)
#endif

#ifdef _WIN32
static LONG WINAPI crash_handler(EXCEPTION_POINTERS* exception)
#else
static void crash_handler(int signal, siginfo_t* info, ucontext_t* context)
#endif
{
    printf("=== SATURN CRASH REPORT ===\n");
#if defined(GIT_HASH) && defined(GIT_BRANCH)
    printf(GIT_BRANCH " " GIT_HASH "\n");
#endif
    // this HAS to stay unmodified otherwise the updater tool will break
    // this also has to be the ONLY instance of the "Version: " string in the entire executable
    printf("Version: " SATURN_VERSION "\n");
#ifdef _WIN32
    if (exception && exception->ExceptionRecord) {
        PEXCEPTION_RECORD err = exception->ExceptionRecord;
        for (int i = 0; i < ARRSIZE(error_messages); i++) {
            if (error_messages[i].code == err->ExceptionCode || error_messages[i].code == 0) {
                printf("%s - ", error_messages[i].error);
                if (err->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || err->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
                    printf(error_messages[i].message, (err->ExceptionInformation[0] ? "write" : "read"), (PTR)err->ExceptionInformation[1]);
                }
                else printf("%s", error_messages[i].message);
                break;
            }
        }
    }
#else
    if (signal != 0 && info != NULL) {
        for (int i = 0; i < ARRSIZE(error_messages); i++) {
            if (error_messages[i].code == signal || error_messages[i].code == 0) {
                printf("%s - ", error_messages[i].error);
                if (signal == SIGSEGV) {
                    printf(error_messages[i].message, ((context->uc_mcontext.gregs[REG_ERR] & 0x2) != 0 ? "write" : "read"), (PTR)info->si_addr);
                }
                else printf("%s", error_messages[i].message);
                break;
            }
        }
    }
#endif
    else {
        printf("Unable to get error info");
    }
#ifdef _WIN32
    if (exception && exception->ContextRecord)
#else
    if (context->uc_mcontext.gregs[REG_RSP] != 0)
#endif
    {
#ifdef _WIN32
        PCONTEXT reg = exception->ContextRecord;
#else
        greg_t* reg = context->uc_mcontext.gregs;
#endif
        printf(
            "\nRegisters:\n"
            "RSP  0x%016llX   RBP  0x%016llX   RIP  0x%016llX\n"
            "RAX  0x%016llX   RBX  0x%016llX   RCX  0x%016llX\n"
            "RDX  0x%016llX   R08  0x%016llX   R09  0x%016llX\n"
            "R10  0x%016llX   R11  0x%016llX   R12  0x%016llX\n"
            "R13  0x%016llX   R14  0x%016llX   R15  0x%016llX\n"
            "RSI  0x%016llX   RDI  0x%016llX\n",
#ifdef _WIN32
            (PTR)reg->Rsp, (PTR)reg->Rsp, (PTR)reg->Rip,
            (PTR)reg->Rax, (PTR)reg->Rbx, (PTR)reg->Rcx,
            (PTR)reg->Rdx, (PTR)reg->R8,  (PTR)reg->R9,
            (PTR)reg->R10, (PTR)reg->R11, (PTR)reg->R12,
            (PTR)reg->R13, (PTR)reg->R14, (PTR)reg->R15,
            (PTR)reg->Rsi, (PTR)reg->Rdi
#else
            reg[REG_RSP], reg[REG_RBP], reg[REG_RIP],
            reg[REG_RAX], reg[REG_RBX], reg[REG_RCX],
            reg[REG_RDX], reg[REG_R8 ], reg[REG_R9 ],
            reg[REG_R10], reg[REG_R11], reg[REG_R12],
            reg[REG_R13], reg[REG_R14], reg[REG_R15],
            reg[REG_RSI], reg[REG_RDI]
#endif
        );
    }
    else {
        printf("\nUnable to get register info\n");
    }
#ifdef _WIN32
    Symbol* symbols = load_symbols();
    if (!symbols) printf("Unable to find debug symbols, some info may be missing\n");
	printf("Address of saturn_update: 0x%016llX\n", (void*)saturn_update);
    void* stacktrace[256];
    USHORT num_frames;
    num_frames = CaptureStackBackTrace(0, 256, stacktrace, NULL);
    if (num_frames > 0)
#else
    void *stacktrace[256];
    int num_frames = backtrace(stacktrace, 256);
    if (num_frames > 0)
#endif
    {
        printf("Backtrace:");
        for (int i = 0; i < num_frames; i++) {
            printf("\n#%-3d ", i);
            Dl_info info;
            if (_dladdr(stacktrace[i], &info, symbols) && info.dli_sname) {
                printf("%s", info.dli_sname);
                if (info.dli_saddr != 0) printf(" + 0x%lX", stacktrace[i] - info.dli_saddr);
            }
            else {
                printf("??? [0x%p]", stacktrace[i]);
            }
        }
        printf("\n");
    }
    else {
        printf("Unable to get stack trace\n");
    }
#ifdef _WIN32
    free_symbols(symbols);
#endif
    close_logger();
    saturn_show_crash_dialog();
    exit(1);
#ifdef _WIN32
    return 0;
#else
    return;
#endif
}

void init_crash_handler() {
#ifdef _WIN32
    SetUnhandledExceptionFilter(crash_handler);
#else
    struct sigaction signal_handler;
    signal_handler.sa_handler = (void*)crash_handler;
    sigemptyset(&signal_handler.sa_mask);
    signal_handler.sa_flags = SA_SIGINFO;
    sigaction(SIGBUS, &signal_handler, NULL);
    sigaction(SIGFPE, &signal_handler, NULL);
    sigaction(SIGILL, &signal_handler, NULL);
    sigaction(SIGSEGV, &signal_handler, NULL);
#endif
}

#else
void init_crash_handler() {}
#endif