#ifndef _WIGGLER_H_
#define _WIGGLER_H_

#ifdef __MACOSX__
#include <Python/Python.h>
#else
#include <Python.h>
#endif

struct SWigglerContext {
    PyObject* refreshCallback;
    // todo: screen objects
    //       click/touch callback
    //       music?
};

extern struct SWigglerContext WigglerContext;
void Wiggler_CheckForPyScript(const char *rom_filename);
void Wiggler_Refresh();
void Wiggler_Restart(); //TODO: place this
void Wiggler_Unload(); //TODO: place this

#endif