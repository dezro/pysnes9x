#ifndef _WIGGLER_H_
#define _WIGGLER_H_

#ifdef HAVE_STDINT_H
# undef HAVE_STDINT_H //rid myself of an annoying warning
#endif

#ifdef __MACOSX__
# include <Python/Python.h>
#else
# include <Python.h>
#endif

#include <map>
#include <vector>
#include <stack>

struct SReturnData {
    PyObject* callback;
    unsigned char bank;
    unsigned short addr;
    unsigned char flags;
    unsigned int count;
};

typedef std::stack<SReturnData> KReturnStack;
typedef std::map<unsigned int, PyObject*> MTrapMap;
typedef std::vector<PyObject*> VPyRoutines;

struct SWigglerContext {
    bool loaded;
    char* filename;
    PyObject* refreshCallback;
    MTrapMap TrapMap;
    VPyRoutines PyRoutines;
    KReturnStack ReturnStack;
    bool avoidInfiniteHookLoop; //use this to avoid entering an infinite hook->call->RTS->hook->call... loop
    
    // todo: screen objects
    //       click/touch callback
    //       music?
};

extern struct SWigglerContext WigglerContext;
void Wiggler_CheckForPyScript(const char *rom_filename);
void Wiggler_Refresh();
void Wiggler_Trap(unsigned int address);
void Wiggler_HardReset();
void Wiggler_SoftReset();
void Wiggler_Unload();

// Opcodes:
void Wiggler_WDMJSR(unsigned char address);
void Wiggler_WDMJSL(unsigned short address);

inline void Wiggler_SubJump() {
    if (!WigglerContext.ReturnStack.empty())
        WigglerContext.ReturnStack.top().count++;
}

void Wiggler_SubReturnB();
inline bool Wiggler_SubReturn() {
    if (WigglerContext.ReturnStack.empty())
        return false;
    if ((--WigglerContext.ReturnStack.top().count) == 0) { // I don't normally like to do that, but...
        Wiggler_SubReturnB();
        return true;
    }
    return false;
}

#endif
