#include "wiggler9x.h"
#include <map> // registerhack
#include <vector> // subroutine

#include "snes9x.h"
#include "memmap.h" // peek, poke, push, pull
#include "controls.h" // poll
#include "display.h" // inform
#include "65c816.h" // push, pull

// wiggler.register_refresh(callable)
// Registers the passed callable, so we may call it at screen refresh.
static PyObject*
wpy_register_refresh(PyObject *self, PyObject *args) {
    PyObject *hook;
    if (!PyArg_ParseTuple(args, "O", &hook))
        return NULL;
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

// wiggler.register_trap(address, callable)
// Registers the passed callable and calls it before address is run.
static PyObject*
wpy_register_trap(PyObject *self, PyObject *args) {
    uint32 address;
    PyObject *hook;
    if (!PyArg_ParseTuple(args, "IO", &address, &hook))
        return NULL;
    if (WigglerContext.TrapMap.count(address)) {
        Py_CLEAR(WigglerContext.TrapMap[address]);
        WigglerContext.TrapMap.erase(address);
    }
    if (hook == Py_None) {
        Py_RETURN_NONE;
    } else if (!PyCallable_Check(hook)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return NULL;
    }
    
    Py_INCREF(hook);
    WigglerContext.TrapMap[address] = hook;
    
    Py_RETURN_NONE;
}

// wiggler.register_sub(callable) -> fake_address
// Registers the passed callable as a WDM JSR subroutine at the returned address.
static PyObject*
wpy_register_sub(PyObject *self, PyObject *args) {
    PyObject *hook;
    if (!PyArg_ParseTuple(args, "O", &hook))
        return NULL;
    if (!PyCallable_Check(hook)) {
        PyErr_SetString(PyExc_TypeError, "register_sub expects a callable");
        return NULL;
    }
    
    Py_INCREF(hook);
    WigglerContext.PyRoutines.push_back(hook);
    return Py_BuildValue("I", WigglerContext.PyRoutines.size() - 1);
}

// The following function 'peeks' at a byte in memory.
static PyObject*
wpy_peek(PyObject *self, PyObject *args) {
    uint32 address;
    uint32 length = 0;
    if (!PyArg_ParseTuple(args, "I|I", &address, &length))
        return NULL;
    
    if (length)
        return Py_BuildValue("s#", S9xGetMemPointer(address), length); //todo: length checking
    return Py_BuildValue("B", *S9xGetMemPointer(address));
}

// As before, but for writing instead of reading.
static PyObject*
wpy_poke(PyObject *self, PyObject *args) {
    uint32 address;
    PyObject* byte_or_buffer;
    uint8 *memptr;
    if (!PyArg_ParseTuple(args, "IO", &address, &byte_or_buffer))
        return NULL;
    
    memptr = S9xGetMemPointer(address);
    if (PyObject_CheckReadBuffer(byte_or_buffer)) { // buffer
        const uint8 *bufptr;
        Py_ssize_t length;
        PyObject_AsReadBuffer(byte_or_buffer, (const void**)&bufptr, &length);
        memcpy(memptr, bufptr, length); //todo: LENGTH CHECKING (PLEASE!)
        Py_RETURN_NONE;
    } else { // hopefully an int
        long value;
        value = PyInt_AsLong(byte_or_buffer);
        if (value == -1 && PyErr_Occurred()) {
            // todo: replace this with a more proper exception
            return NULL;
        }
        *memptr = (uint8)value;
        Py_RETURN_NONE;
    }
}

// wiggler.poll returns an integer in which each bit is a button on the controller.
uint16 MovieGetJoypad(int i);
static PyObject*
wpy_poll(PyObject *self, PyObject *args) {
    uint player;
    if (!PyArg_ParseTuple(args, "I", &player))
        return NULL;
    if (player > 7) {
        PyErr_SetString(PyExc_ValueError, "poll expects a player number from 0 to 7");
        return NULL;
    }
    return Py_BuildValue("I", MovieGetJoypad(player));
}

// wiggler.poll_mouse returns the current x and y location of the mouse as well as the button state
// only the left button is supported (for possibly misguided compatibility reasons)
// I mean, don't want to leave the touch-screen and Mac users in the dark
static PyObject*
wpy_poll_mouse(PyObject *self, PyObject *args) {
    uint player = 0;
    uint32 pid = 0x82200000;
    uint32 bid = 0x82100000;
    int16 x, y;
    bool pressed;
    
    if (!PyArg_ParseTuple(args, "|I", &player))
        return NULL;
    if (player == 0) {
        pid += 0x100; // ID of player 1 mouse
        bid += 0x100;
    }
    else if (player == 1) {
        pid += 0x200; // p2
        bid += 0x200;
    }
    else {
        PyErr_SetString(PyExc_ValueError, "poll_mouse expects a player number of 0 or 1");
        return NULL;
    }
    
    S9xPollPointer(pid, &x, &y);
    S9xPollButton(bid, &pressed);
    return Py_BuildValue("hhB", x, y, pressed);
}

// wiggler.inform displays the passed string
static PyObject*
wpy_inform(PyObject *self, PyObject *args) {
    PyObject *object = NULL;
    if (!PyArg_ParseTuple(args, "|O", &object))
        return NULL;
    
    if (object) {
        PyObject *pyString = PyObject_Str(object);
        if (pyString) {
            char *string = PyString_AsString(pyString);
            if (string)
                S9xSetInfoString(string);
            Py_DECREF(pyString);
            if (!string) //should probably never happen but whatever
                return NULL;
        } else {
            PyErr_SetString(PyExc_TypeError, "cannot get string form of object");
            return NULL;
        }
    }
    else
        S9xSetInfoString("");
    Py_RETURN_NONE;
}

// Jump to an arbitrary address.
static PyObject*
wpy_jump(PyObject *self, PyObject *args) {
    uint32 address;
    if (!PyArg_ParseTuple(args, "I", &address))
        return NULL;
    
    Registers.PCw = address & 0xFFFF;
    Registers.PB = address >> 16;
    Py_RETURN_NONE;
}

// Return from subroutine
static PyObject*
wpy_short_return(PyObject *self, PyObject *args) {
    //AddCycles(TWO_CYCLES);
    Registers.SL++;
    Registers.PCw = S9xGetWord(Registers.S.W, WRAP_PAGE);
    Registers.SL++;
    //AddCycles(ONE_CYCLE);
    Registers.PCw++;
    S9xSetPCBase (Registers.PBPC);
    
    Py_RETURN_NONE;
}

// Return from 16-bit subroutine
static PyObject*
wpy_long_return(PyObject *self, PyObject *args) {
    //AddCycles(TWO_CYCLES);
    Registers.PCw = S9xGetWord(Registers.S.W + 1, WRAP_BANK);
    Registers.S.W += 2;
    Registers.PB = S9xGetByte(++Registers.S.W);
    Registers.SH = 1;
    Registers.PCw++;
    S9xSetPCBase(Registers.PBPC);
    
    Py_RETURN_NONE;
}

static PyMethodDef mod_wiggler[] = {
    {"register_refresh", wpy_register_refresh, METH_VARARGS,
     "register_refresh(callback)\nRegister a callback to be run at screen refresh."},
    {"register_trap", wpy_register_trap, METH_VARARGS,
     "register_trap(address, callback)\nRegister a callback to be run before the instruction at address."},
    {"register_sub", wpy_register_sub, METH_VARARGS,
     "register_sub(callback)\nRegister a callback as a subroutine at the fake address of the return value.\n"
     "Call it from 65816 code by poking 0x4220xx, where xx is the returned value.\n"
     "You can also poke 0x4222xx00."},
    
    {"peek", wpy_peek, METH_VARARGS,
     "peek(address) -> int\nPeek at a byte in memory.\n\n"
     "peek(address, length) -> string\nPeek at many bytes in memory."},
    
    {"poke", wpy_poke, METH_VARARGS,
     "poke(address, value)\nPoke a byte into memory.\n\n"
     "poke(address, string)\nPoke a string into memory."},
    
    {"poll", wpy_poll, METH_VARARGS,
     "poll(player) -> buttons\nPoll the player's controller."},
    {"poll_mouse", wpy_poll_mouse, METH_VARARGS,
     "poll_mouse(player) -> (x,y)\nPoll the player's mouse."},
    
    {"inform", wpy_inform, METH_VARARGS,
     "inform(string)\nDisplays the string."},
    
    {"jump", wpy_jump, METH_VARARGS,
     "jump(address)\nJumps to an arbitrary address."},
    {"short_return", wpy_short_return, METH_NOARGS,
     "short_return()\nReturns from a subroutine."},
    {"long_return", wpy_long_return, METH_NOARGS,
     "long_return()\nReturns from a 16-bit subroutine."},
    {NULL,NULL,0,NULL}
};

PyMODINIT_FUNC
initwiggler(void) {
    (void) Py_InitModule("wiggler", mod_wiggler);
}