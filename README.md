# User space block backend (us-blkback)
This provides a completely userspace block backend for Xen guests that utilize
the blkfront driver (winvbd or xen-blkfront on Windows and Linux respectively). The main target of this driver is for use with the Xen compatibility layer on top of a flavor of the Bareflank hypervisor, however main development was done against Xen 4.9 and should work on pretty much any modern Xen.

# Building the user space block backend
The user space block backend builds on both Windows and Linux.

## Windows
us-blkback has 4 other major dependencies required to build it:
* cmake
* Git for Windows
* Xen Windows PV drivers (windows-pv-drivers.git)
* libxenbe (libxenbe.git, Windows branch)

Open git bash and enter the following:

```
cd /c/Users/user/Documents
git clone -b windows https://github.com/brendank310/libxenbe.git
git clone https://gitlab.com/brendank310/us-blkback.git
git clone https://gitlab.com/redfield/winpv/windows-pv-drivers.git
```

### Resolve the windows-pv-driver dependency
Run the provisioning script described in the windows-pv-drivers repository README.md (https://gitlab.com/redfield/winpv/windows-pv-drivers/-/blob/master/README.md), and follow the building instructions.

### Resolve the libxenbe dependency
1) Create a folder within the libxenbe folder (C:\Users\user\Documents\libxenbe), called build.
2) Open the cmake-gui, and set the "Where is the source code:" to the libxenbe folder (C:\Users\user\Documents\libxenbe).
3) Set the "Where to build the binaries:" to the build folder (C:\Users\user\Documents\libxenbe\build).
4) Select the WITH_WIN tick box.
5) Click "Configure".
6) Click "Add Entry" for each of the following settings:
* XENIFACE_WINDOWS_INCLUDE_PATH (as type path) and point it to the xeniface include folder (C:\Users\user\Documents\windows-pv-drivers\xeniface\include)
* XENIFACE_WINDOWS_LIB_PATH (as type path) and point it to the xeniface build output folder (C:\Users\user\Documents\windows-pv-drivers\xeniface\xeniface\x64)
* XENBUS_WINDOWS_INCLUDE_PATH (as type path) and point it to the xenbus include folder (C:\Users\user\Documents\windows-pv-drivers\xenbus\include)
* XENBUS_WINDOWS_LIB_PATH (as type path) and point it to the xenbus build output folder (C:\Users\user\Documents\windows-pv-drivers\xenbus\vs2017\Windows10Debug\x64)

## Linux
