#include "wiggler9x.h"
#include <map> // registerhack
#include <vector> // subroutine

#include "snes9x.h"
#include "memmap.h" // peek, poke
#include "controls.h" // poll
#include "display.h" // inform
#include "65c816.h" // registerhack

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

// wiggler.register_trap(address, callable)
// Registers the passed callable and calls it before address is run.
extern std::map<uint32, PyObject*> TrapMap;
static PyObject*
wpy_register_trap(PyObject *self, PyObject *args) {
    uint32 address;
    PyObject *hook;
    if (!PyArg_ParseTuple(args, "IO", &address, &hook)) {
        PyErr_SetString(PyExc_TypeError, "register_trap expects an address and a callable object");
        return NULL;
    }
    if (TrapMap.count(address)) {
        Py_CLEAR(TrapMap[address]);
        TrapMap.erase(address);
    }
    if (hook == Py_None) {
        Py_RETURN_NONE;
    } else if (!PyCallable_Check(hook)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return NULL;
    }
    
    Py_INCREF(hook);
    TrapMap[address] = hook;
    
    Py_RETURN_NONE;
}

// wiggler.register_sub(callable) -> fake_address
// Registers the passed callable as a WDM JSR subroutine at the returned address.
extern std::vector<PyObject*> PyRoutines;
static PyObject*
wpy_register_sub(PyObject *self, PyObject *args) {
    PyObject *hook;
    if (!PyArg_ParseTuple(args, "O", &hook)) {
        PyErr_SetString(PyExc_TypeError, "register_sub expects a callable");
        return NULL;
    }
    if (!PyCallable_Check(hook)) {
        PyErr_SetString(PyExc_TypeError, "register_sub expects a callable");
        return NULL;
    }
    
    PyRoutines.push_back(hook);
    return Py_BuildValue("I", PyRoutines.size() - 1);
}

// The following two functions are stolen from cheats2.cpp.
// They get and set bytes without effing with the timing.
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
    int block = ((Address&0xffffff) >> MEMMAP_SHIFT);
    uint8 *ptr = Memory.Map [block];

    if (ptr >= (uint8 *) CMemory::MAP_LAST) //rom
        *(ptr + (Address & 0xffff)) = Byte;
    else { //ram
        uint32 Cycles = CPU.Cycles;
        uint32 WaitAddress = CPU.WaitAddress;
        S9xSetByte (Byte, Address);
        CPU.WaitAddress = WaitAddress;
        CPU.Cycles = Cycles;
    }
}
// end

// The following four functions 'peek' at a byte in memory.
static PyObject*
wpy_peek(PyObject *self, PyObject *args) {
    uint32 address;
    if (!PyArg_ParseTuple(args, "I", &address)) {
        PyErr_SetString(PyExc_TypeError, "peek expects an integer address");
        return NULL;
    }
    
    return Py_BuildValue("B", S9xGetByteFree(address));
}

static PyObject*
wpy_peek_bytes(PyObject *self, PyObject *args) {
    uint32 address;
    uint32 length;
    if (!PyArg_ParseTuple(args, "II", &address, &length)) {
        PyErr_SetString(PyExc_TypeError, "peek_bytes expects an integer address and an integer length");
        return NULL;
    }
    //todo: range checking!
    return Py_BuildValue("s#", S9xGetMemPointer(address), length);
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
wpy_poke_bytes(PyObject *self, PyObject *args) {
    uint32 address;
    uint8 *ptr;
    uint8 *buffer;
    uint length;
    if (!PyArg_ParseTuple(args, "Is#", &address, &buffer, &length)) {
        PyErr_SetString(PyExc_TypeError, "poke_bytes expects an address, followed by a string or buffer");
        return NULL;
    }
    //todo: range checking!
    ptr = S9xGetMemPointer(address);
    memcpy(ptr, buffer, length);
    Py_RETURN_NONE;
}

// wiggler.poll returns an integer in which each bit is a button on the controller.
uint16 MovieGetJoypad(int i);
static PyObject*
wpy_poll(PyObject *self, PyObject *args) {
    uint player;
    if (!PyArg_ParseTuple(args, "I", &player)) {
        PyErr_SetString(PyExc_TypeError, "poll expects a player number");
        return NULL;
    }
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
    
    if (!PyArg_ParseTuple(args, "|I", &player)) {
        PyErr_SetString(PyExc_TypeError, "poll_mouse expects a player number");
        return NULL;
    }
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

// Jump to an arbitrary address.
static PyObject*
wpy_jump(PyObject *self, PyObject *args) {
    uint32 address;
    if (!PyArg_ParseTuple(args, "I", &address)) {
        PyErr_SetString(PyExc_TypeError, "jump expects an address");
        return NULL;
    }
    
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
     "peek(address) -> byte\nPeek at an unsigned byte in memory."},
    {"peek_bytes", wpy_peek_bytes, METH_VARARGS,
     "peek_bytes(address, length) -> string\nPeek at contiguous bytes."},
    
    {"poke", wpy_poke, METH_VARARGS,
     "poke(address, value)\nPoke an unsigned byte into memory."},
    {"poke_bytes", wpy_poke_bytes, METH_VARARGS,
     "poke_bytes(address, string)\nPoke a string into memory."},
    
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