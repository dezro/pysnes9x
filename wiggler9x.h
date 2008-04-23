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

struct SWigglerContext {
    bool loaded;
    char* filename;
    PyObject* refreshCallback;
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

#endif
