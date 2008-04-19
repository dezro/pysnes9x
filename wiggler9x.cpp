#include <stdio.h>
#include "wiggler9x.h"
#include "snes9x.h"
#include "memmap.h"
#include "controls.h"

PyMODINIT_FUNC initwiggler(void);

void Wiggler_PyInit() {
    //WigglerContext = {0}; // todo: something like this.
    Py_Initialize();
    initwiggler();
}

void Wiggler_CheckForPyScript(const char *rom_filename) {
    char   dir [_MAX_DIR + 1];
    char   drive [_MAX_DRIVE + 1];
    char   name [_MAX_FNAME + 1];
    char   ext [_MAX_EXT + 1];
    char   fname [_MAX_PATH + 1];
    FILE* pyscript_file  = NULL;
    
    Wiggler_PyInit();
    
    _splitpath (rom_filename, drive, dir, name, ext);
    _makepath (fname, drive, dir, name, "py");
    if ((pyscript_file = fopen(fname, "r")))
    {
        printf ("Using Python script %s\n", fname);
        PyRun_AnyFile(pyscript_file, fname);
        fclose(pyscript_file);
    }
}

void Wiggler_Refresh() {
    if (WigglerContext.refreshCallback == NULL)
        return;
    PyObject_CallObject(WigglerContext.refreshCallback, NULL);
}

void Wiggler_Restart() {
    Wiggler_Unload();
    //todo: get the proper rom name
    Wiggler_CheckForPyScript("wiggler.smc");
}

void Wiggler_Unload() {
    Py_XDECREF(WigglerContext.refreshCallback); // Probably unnecessary what with the finalize...
    Py_Finalize();
    //todo: collect garbage
}

static PyObject*
wpy_register_refresh(PyObject *self, PyObject *args) {
    PyObject *hook;
    if (!PyArg_ParseTuple(args, "O", &hook)) {
        PyErr_SetString(PyExc_TypeError, "register_refresh expects one callable object");
        return NULL;
    }
    if (!PyCallable_Check(hook)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return NULL;
    }
    Py_XDECREF(WigglerContext.refreshCallback);
    WigglerContext.refreshCallback = hook;
    Py_INCREF(WigglerContext.refreshCallback);
    Py_RETURN_NONE;
}

// Copied from cheats2.cpp:
INLINE uint8 S9xGetByteFree (uint32 Address)
{
	uint32 Cycles = CPU.Cycles;
	uint32 WaitAddress = CPU.WaitAddress;
	uint8 rv = S9xGetByte (Address);
	CPU.WaitAddress = WaitAddress;
	CPU.Cycles = Cycles;
	return rv;
}
INLINE void S9xSetByteFree (uint8 Byte, uint32 Address)
{
	uint32 Cycles = CPU.Cycles;
	uint32 WaitAddress = CPU.WaitAddress;
	S9xSetByte (Byte, Address);
	CPU.WaitAddress = WaitAddress;
	CPU.Cycles = Cycles;
}
// end copy
// modified from the above
INLINE uint16 S9xGetWordFree (uint32 Address)
{
	uint32 Cycles = CPU.Cycles;
	uint32 WaitAddress = CPU.WaitAddress;
	uint16 rv = S9xGetWord (Address);
	CPU.WaitAddress = WaitAddress;
	CPU.Cycles = Cycles;
	return rv;
}
INLINE void S9xSetWordFree (uint16 Word, uint32 Address)
{
	uint32 Cycles = CPU.Cycles;
	uint32 WaitAddress = CPU.WaitAddress;
	S9xSetWord (Word, Address);
	CPU.WaitAddress = WaitAddress;
	CPU.Cycles = Cycles;
}
// end modify

static PyObject*
wpy_peek(PyObject *self, PyObject *args) {
    uint32 address;
    if (!PyArg_ParseTuple(args, "k", &address)) {
        PyErr_SetString(PyExc_TypeError, "peek expects an integer address");
        return NULL;
    }
    
    return Py_BuildValue("B", S9xGetByteFree(address));
}

static PyObject*
wpy_peek_word(PyObject *self, PyObject *args) {
    uint32 address;
    if (!PyArg_ParseTuple(args, "k", &address)) {
        PyErr_SetString(PyExc_TypeError, "peek_word expects an integer address");
        return NULL;
    }
    
    return Py_BuildValue("H", S9xGetWordFree(address));
}

static PyObject*
wpy_peek_signed(PyObject *self, PyObject *args) {
    uint32 address;
    if (!PyArg_ParseTuple(args, "k", &address)) {
        PyErr_SetString(PyExc_TypeError, "peek expects an integer address");
        return NULL;
    }
    
    return Py_BuildValue("b", S9xGetByteFree(address));
}

static PyObject*
wpy_peek_signed_word(PyObject *self, PyObject *args) {
    uint32 address;
    if (!PyArg_ParseTuple(args, "k", &address)) {
        PyErr_SetString(PyExc_TypeError, "peek_signed_word expects an integer address");
        return NULL;
    }
    
    return Py_BuildValue("h", S9xGetWordFree(address));
}

static PyObject*
wpy_poke(PyObject *self, PyObject *args) {
    uint32 address;
    uint8 byte;
    if (!PyArg_ParseTuple(args, "IB", &address, &byte)) {
        PyErr_SetString(PyExc_TypeError, "poke expects an address, followed by a byte value");
        return NULL;
    }
    
    S9xSetByteFree(byte, address);
    Py_RETURN_NONE;
}

static PyObject*
wpy_poke_word(PyObject *self, PyObject *args) {
    uint32 address;
    uint8 byte;
    if (!PyArg_ParseTuple(args, "IH", &address, &byte)) {
        PyErr_SetString(PyExc_TypeError, "poke_word expects an address, followed by a word value");
        return NULL;
    }
    
    S9xSetWordFree(byte, address);
    Py_RETURN_NONE;
}

static PyObject*
wpy_poke_signed(PyObject *self, PyObject *args) {
    uint32 address;
    uint8 byte;
    if (!PyArg_ParseTuple(args, "Ib", &address, &byte)) {
        PyErr_SetString(PyExc_TypeError, "poke_signed expects an address, followed by a signed byte value");
        return NULL;
    }
    
    S9xSetByteFree(byte, address);
    Py_RETURN_NONE;
}

static PyObject*
wpy_poke_signed_word(PyObject *self, PyObject *args) {
    uint32 address;
    uint8 byte;
    if (!PyArg_ParseTuple(args, "Ih", &address, &byte)) {
        PyErr_SetString(PyExc_TypeError, "poke_signed_word expects an address, followed by a signed word value");
        return NULL;
    }
    
    S9xSetWordFree(byte, address);
    Py_RETURN_NONE;
}

uint16 MovieGetJoypad(int i);
static PyObject*
wpy_poll(PyObject *self, PyObject *args) {
    int player;
    if (!PyArg_ParseTuple(args, "I", &player)) {
        PyErr_SetString(PyExc_TypeError, "poll expects a player number");
        return NULL;
    }
    
    return Py_BuildValue("I", MovieGetJoypad(player));
}

static PyObject*
wpy_poll_mouse(PyObject *self, PyObject *args) {
    int player;
    uint32 id;
    int16 x, y;
    
    if (!PyArg_ParseTuple(args, "I", &player)) {
        PyErr_SetString(PyExc_TypeError, "poll_mouse expects a player number");
        return NULL;
    }
    if (player == 0)
        id = 0x82200100;
    else if (player == 1)
        id = 0x82200200;
    else {
        PyErr_SetString(PyExc_ValueError, "poll_mouse expects a player number of 0 or 1");
        return NULL;
    }
    
    S9xPollPointer(id, &x, &y);
    return Py_BuildValue("hh", x, y);
}

static PyMethodDef mod_wiggler[] = {
    {"register_refresh", wpy_register_refresh, METH_VARARGS,
     "register_refresh(callback)\nRegister a callback to be run at screen refresh."},
    {"peek", wpy_peek, METH_VARARGS,
     "peek(address) -> byte\nPeek at an unsigned byte in memory."},
    {"poke", wpy_poke, METH_VARARGS,
     "poke(address, value)\nPoke an unsigned byte into memory."},
    {"peek_signed", wpy_peek_signed, METH_VARARGS,
     "peek_signed(address) -> byte\nPeek at a signed byte in memory."},
    {"poke_signed", wpy_poke_signed, METH_VARARGS,
     "poke_signed(address, value)\nPoke a signed byte into memory."},
    {"peek_word", wpy_peek_word, METH_VARARGS,
     "peek_word(address) -> byte\nPeek at an unsigned word in memory."},
    {"poke_word", wpy_poke_word, METH_VARARGS,
     "poke_word(address, value)\nPoke an unsigned word into memory."},
    {"peek_signed_word", wpy_peek_signed_word, METH_VARARGS,
     "peek_signed_word(address) -> byte\nPeek at a signed word in memory."},
    {"poke_signed_word", wpy_poke_signed_word, METH_VARARGS,
     "poke_signed_word(address, value)\nPoke a signed word into memory."},
    {"poll", wpy_poll, METH_VARARGS,
     "poll(player) -> buttons\nPoll the player's controller."},
    {"poll_mouse", wpy_poll_mouse, METH_VARARGS,
     "poll_mouse(player) -> (x,y)\nPoll the player's mouse."},
    {NULL,NULL,0,NULL}
};

PyMODINIT_FUNC
initwiggler(void) {
    (void) Py_InitModule("wiggler", mod_wiggler);
}