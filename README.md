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
cd /c/Users/<user>/Documents
git clone -b windows https://github.com/brendank310/libxenbe.git
git clone -b initial-implementation https://gitlab.ainfosec.com/kerriganb/win-blkback.git
git clone https://gitlab.com/beam/winpv/windows-pv-drivers.git
```

Run the provisioning script described in the windows-pv-drivers repository documentation.

## Linux
