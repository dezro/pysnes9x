#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "snes9x.h"
#include "wiggler9x.h"
#include "display.h" // inform
#include "65c816.h"

PyMODINIT_FUNC initwiggler(void);

// Private API:
void Wiggler_PyInit();
void Wiggler_ClearContext();
void Wiggler_Finalize();
// end private API

inline int PythonCall(PyObject* callable) {
    if (!callable)
        return -1;

    PyObject_CallObject(callable, NULL);
    if (PyErr_Occurred()) {
        PyErr_Print();
        return -1;
    }
    return 0;
}

// Finds a python script in the same directory as the ROM, then copies the filename to context.
// Called first by Memory.LoadROM.
// NOT called by LoadMultiCart, LoadSufamiTurbo, or LoadSameGame.
// Multiple games means multiple scripts, which doesn't work.
void Wiggler_CheckForPyScript(const char *rom_filename) {
    char   dir [_MAX_DIR + 1];
    char   drive [_MAX_DRIVE + 1];
    char   name [_MAX_FNAME + 1];
    char   ext [_MAX_EXT + 1];
    char   fname [_MAX_PATH + 1];
    
    _splitpath (rom_filename, drive, dir, name, ext);
    _makepath (fname, drive, dir, name, "py");
    
    if (WigglerContext.filename)
        free(WigglerContext.filename); //just in case...
    WigglerContext.filename = (char*)malloc(strlen(fname)+1);
    chdir(dir);
    strcpy(WigglerContext.filename, fname);
}

// (Re)sets Python. Called second by S9xReset in cpu.cpp
void Wiggler_HardReset() {
    if (WigglerContext.loaded)
        Wiggler_Finalize();
    
    Wiggler_PyInit();
}

// Initializes Python and loads the script.
void Wiggler_PyInit() {
    char *argv[] = {WigglerContext.filename, NULL};
    Py_InitializeEx(0);
    PySys_SetArgv(1, argv);
    initwiggler();
    
    PyObject* pyscript_fobj;
    if ((pyscript_fobj = PyFile_FromString(WigglerContext.filename, "r")))
    {
        printf ("Using Python script %s\n", WigglerContext.filename); //todo: maybe move this to screen?
        FILE* f = PyFile_AsFile(pyscript_fobj);
        if (PyRun_AnyFile(f, WigglerContext.filename) == 0) //success
            WigglerContext.loaded = true;
        else
            Wiggler_ClearContext();
    }
    Py_CLEAR(pyscript_fobj);
}

// The screen has refreshed. Called by S9xEndScreenRefresh in gfx.cpp
void Wiggler_Refresh() {
    PythonCall(WigglerContext.refreshCallback);
}

// Spring a trap.
void Wiggler_Trap(uint32 address) {
    PythonCall(WigglerContext.TrapMap[address]);
}

// Opcodes.
void Wiggler_WDMJSR(unsigned char address) {
    if (WigglerContext.PyRoutines.size() > address)
        PythonCall(WigglerContext.PyRoutines[address]);
    // else crash?
}
void Wiggler_WDMJSL(unsigned short address) {
    if (WigglerContext.PyRoutines.size() > address)
        PythonCall(WigglerContext.PyRoutines[address]);
    // else crash?
}

void Wiggler_SubReturnB() {
    SReturnData &rdat = WigglerContext.ReturnStack.top();
    Registers.PB = rdat.bank;
    Registers.PCw = rdat.addr;
    Registers.P.W = rdat.flags;
    if (rdat.callback) {
        PythonCall(rdat.callback);
        Py_CLEAR(rdat.callback);
    }
    WigglerContext.ReturnStack.pop();
    WigglerContext.avoidInfiniteHookLoop = true;
}

// Clear everything from Wiggler.Context EXCEPT filename.
void Wiggler_ClearContext() {
    WigglerContext.loaded = false;
    
    // Refresh callback
    Py_CLEAR(WigglerContext.refreshCallback);
    
    // Traps
    for (MTrapMap::iterator it = WigglerContext.TrapMap.begin(); it != WigglerContext.TrapMap.end(); it++)
        Py_CLEAR(it->second);
    WigglerContext.TrapMap.clear();
    
    // Fake Subroutines
    for (VPyRoutines::iterator it = WigglerContext.PyRoutines.begin(); it != WigglerContext.PyRoutines.end(); it++)
        Py_CLEAR(*it);
    WigglerContext.PyRoutines.clear();
    
    // Return data
    while (!WigglerContext.ReturnStack.empty()) {
        SReturnData &rdat = WigglerContext.ReturnStack.top();
        Py_CLEAR(rdat.callback);
        WigglerContext.ReturnStack.pop();
    }
    
    WigglerContext.avoidInfiniteHookLoop = false;
}

// Clear us up and unload Python.
void Wiggler_Finalize() {
    Wiggler_ClearContext();
    Py_Finalize();
}

// Press the reset button. Called by S9xSoftReset.
void Wiggler_SoftReset() {
    // todo: See if our script wants to handle resetting instead.
    Wiggler_HardReset();
}

// The ROM is being unloaded. For now, called by Memory.Deinit.
// I'm actually not sure if you can just unload the ROM - you might have to
// load a new one in its place. In which case, we're probably okay. What's
// going to happen is CheckForPyScript will be called, free the old filename,
// then later HardReset will be called, notice that loaded is still true, and
// call Finalize.
void Wiggler_Unload() {
    free(WigglerContext.filename);
    Wiggler_Finalize();
}