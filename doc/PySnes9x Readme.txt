PySnes9x Source Readme

I have little idea what I'm doing.

To compile the Mac version you need Xcode on Leopard.

To compile the Windows version on a Mac you may need the following directory tree:

some_dir/
    |- repository_dir/
    |   |- 2xsai.cpp
    |   |- 2xsaiwin.cpp
    |   |- 3d.h
    |   |- ...
    |   |- doc/
    |   |   |- PySnes9x Readme.txt
    |   |   |- ...
    |   |- ...
    |- Windows/
    |   |- bin/
    |   |- ddraw_include/
    |   |- FMOD/
    |   |   |- api/
    |   |   |   |- lib/
    |   |   |   |- inc/
    |   |- libpng/
    |   |   |- lib/
    |   |   |- include/
    |   |- obj/
    |   |- Python25/
    |   |- zlib/
    |   |   |- lib/
    |   |   |- include/

Create the Windows dir and all those directories. ddraw_include should include the headers from DirectX, including ddraw.h, dinput.h, dsound.h, and maybe others. FMOD should include FMOD 3. libpng should have the Windows binaries and headers for libpng - similar for zlib. Python25 should be a copy of C:\Python25 from a Windows installation of Python 2.5.

To compile on Windows with mingw you'll need to modify either the original Makefile.mingw to include PySnes9x's additions, or Makefile.wincross in various fun ways. To compile with Visual Studio you'll need to add PySnes9x's additions, too.

Linux users are on their own. Sorry.


Double sorry if these are bad directions. I'm not good at this...
