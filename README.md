# microsoft-pdb
This repo contains information from Microsoft about the PDB (Program Database) 
[Symbol File](https://msdn.microsoft.com/en-us/library/windows/desktop/aa363368(v=vs.85).aspx) format.

[WILL NOT currently build. There is a cvdump.exe till the repo is completed.  pdb.h is in the langapi folder]

The intent here is to provide code that will show all the binary level formats and simple tools that can use the pdb.

Simply put ...We will make best efforts to role this foward with the new compilers and tools that we ship every release. We will continue to innovate and change binary API's and ABI's for all the Microsoft platforms and we will try to include the community by keeping this PDB repo in synch with the latest retail products (compilers,linkers,debuggers) just shipped.  

By publishing this source code, we are by passing the publically documented API we provided for only reading a PDB - that was DIA
https://msdn.microsoft.com/en-us/library/x93ctkx8.aspx 

With this information we are now building the information for other compilers (and tools) to efficiently write a PDB. 

The PDB format has not been officially documented, presenting a challenge for other compilers and
toolsets (such as Clang/LLVM) that want to work with Windows or the Visual Studio debugger. We want
to help the Open Source compilers to get onto the Windows platform.
 
The majority of content on this repo is presented as actual source files from the VC++ compiler 
toolset. Source code is the ultimate documentation :-) We hope that you will find it helpful. If you 
find that you need other information to successfully complete your project, please enter an
[Issue](https://github.com/microsoft/microsoft-pdb/issues) letting us know what information you need.

The file pdb.h (on in langapi), provides the API surface for mscorpdb.dll, which we ship with every compiler and toolset.

Important points:

•	Mscorpdb.dll is what our linker and compiler uses to create PDB files.
•	Mscorpdb.dll implements the “stream” abstractions.

Also there is another file that we ship that should allow you to determine whether you have correctly produced an “empty” PDB which contains the minimal encoding to let another tool open and correctly parse that “empty” file.  “Empty” really meaning minimal (just using the words used in our meeting so we can be clear)

A tool that I thought we also ship that would easily verify your “empty” PDB file is dia2dump.exe

So in summary, by using the externally defined function entry points in pdb.h you can call into mscorpdb.dll.

