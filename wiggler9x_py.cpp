#import "wiggler9x.h"
#import "snes9x.h"
#include "memmap.h" // peek, poke
#include "controls.h" // poll
#include "display.h" // inform

// wiggler.register_refresh(callable)
// Registers the passed callable, so we may call it at screen refresh.
static PyObject*
wpy_register_refresh(PyObject *self, PyObject *args) {
    PyObject *hook;
    if (!PyArg_ParseTuple(args, "O", &hook)) {
        PyErr_SetString(PyExc_TypeError, "register_refresh expects one callable object");
        return NULL;
    }
    if (hook == Py_None) {
        Py_CLEAR(WigglerContext.refreshCallback);
        Py_RETURN_NONE;
    }
    if (!PyCallable_Check(hook)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return NULL;
    }
    Py_CLEAR(WigglerContext.refreshCallback);
    WigglerContext.refreshCallback = hook;
    Py_INCREF(WigglerContext.refreshCallback);
    Py_RETURN_NONE;
}

// The following four functions are stolen or modified from cheats2.cpp.
// They get and set bytes and words without effing with the timing.
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
// end

// The following four functions 'peek' at a value in memory. Standard 'peek'
// gives an unsigned byte. Suffixes change to signed, word, or both.
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

// As before, but for writing instead of reading.
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

// wiggler.poll returns an integer in which each bit is a button on the controller.
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

// wiggler.poll_mouse returns the current x and y location of the mouse
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
        id = 0x82200100; // ID of player 1 mouse
    else if (player == 1)
        id = 0x82200200; // p2
    else {
        PyErr_SetString(PyExc_ValueError, "poll_mouse expects a player number of 0 or 1");
        return NULL;
    }
    
    S9xPollPointer(id, &x, &y);
    return Py_BuildValue("hh", x, y);
}

// wiggler.inform displays the passed string
static PyObject*
wpy_inform(PyObject *self, PyObject *args) {
    char *string = NULL;
    if (!PyArg_ParseTuple(args, "|z", &string)) {
        PyErr_SetString(PyExc_TypeError, "inform expects a plain ascii string");
        return NULL;
    }
    
    if (string)
        S9xSetInfoString(string);
    else
        S9xSetInfoString("");
    Py_RETURN_NONE;
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
    
    {"inform", wpy_inform, METH_VARARGS,
     "inform(string)\nDisplays the string."},
    {NULL,NULL,0,NULL}
};

PyMODINIT_FUNC
initwiggler(void) {
    (void) Py_InitModule("wiggler", mod_wiggler);
}