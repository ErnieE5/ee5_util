ee5_util
========

C++ Utilities

During my career I have worked on many systems that share the fundamental basis of the work that this code represents. All  all of that code is proprietary in nature and was done as work for hire. (After all, I like to eat and have a home, you know, basics.) The last major system I worked on started prior to C++ 11 being finalized and was based on Microsoft cl 16. *Microsoft (R) C/C++ Optimizing Compiler Version 16.00.40219.01 for x64* The support for TR1 was decent, but lacked the most important feature I was looking forward to: Variadic templates. It wasn't the best idea for the project to base that work on something that was still in flux, so those ideas were expressed as best as possible. I've had the opportunity to work on the ideas within the framework of the "more modern design" of C++ 11 and this represents the principles of that work.

The basic idea is an attempt to unify all the "goodness" of the systems I have worked on in the past, into something that I can publicly display (warts included at this point). I have borrowed ideas from many projects and worked them into this design. If I have the time and resources I'd like to make this more general purpose, but I doubt that is ultimately likely. The programming model isn't far from what a developer used to using a "managed" platform might expect and has a few "functional" ideas tossed in.

**This is a work in progress.** It works (in principle) but needs (a lot of) further development to make it fully functional. (Many of the files aren't properly staged and code is still being placed where expedient.) A more mature project would have most of this under control, but the foundational items are being put together.

Environment
-----------

The code is developed using clang++ under Debian Wheezy running under Parallels and tested on a native server. Recently I have also started verifying the builds against the compiler included with Visual Studio 2013. It figures that there are "quirks." Fewer than expected, but still enough to be annoying. Good job MS.

Neither platform is 100% "perfect." There are missing features from the GNU headers in 4.7 (and still in 4.8!). MS doesn't have a clean slate either. cl 17.00.60610.1 and the headers have a few missing features too. Nothing major (Whew!) on either platform YET.

I expect the most issues with the divergence between POSIX and Windows API's for async-io. (Yeah, that is going to be painful.)


Info:

```Textile
vm /home/erniee/ee5_util -> clang++ --version
Debian clang version 3.5-1~exp1 (trunk) (based on LLVM 3.5)
Target: x86_64-pc-linux-gnu
Thread model: posix
```

```Textile
vm /home/erniee/ee5_util -> uname -a
Linux debian-vm 3.2.0-4-amd64 #1 SMP Debian 3.2.57-3+deb7u2 x86_64 GNU/Linux
```

```Textile
Microsoft (R) C/C++ Optimizing Compiler Version 17.00.60610.1 for x86
Copyright (C) Microsoft Corporation.  All rights reserved.
```

```Textile
Host Name:                 WIN8-VM
OS Name:                   Microsoft Windows 8.1 Pro
OS Version:                6.3.9600 N/A Build 9600
OS Manufacturer:           Microsoft Corporation
OS Configuration:          Standalone Workstation
OS Build Type:             Multiprocessor Free
<snip>
System Manufacturer:       Parallels Software International Inc.
System Model:              Parallels Virtual Platform
System Type:               x64-based PC
Processor(s):              1 Processor(s) Installed.
```

