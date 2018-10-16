# microsoft-pdb
This repo contains information from Microsoft about the PDB (Program Database) 
[Symbol File](https://msdn.microsoft.com/en-us/library/windows/desktop/aa363368(v=vs.85).aspx) format.

[WILL NOT currently build. There is a cvdump.exe till the repo is completed.  pdb.h is in the langapi folder]

The intent here is to provide code that will show all the binary level formats and simple tools that can use the pdb.

Simply put ...We will make best efforts to role this forward with the new compilers and tools that we ship every release. We will continue to innovate and change binary API's and ABI's for all the Microsoft platforms and we will try to include the community by keeping this PDB repo in synch with the latest retail products (compilers,linkers,debuggers) just shipped.  

By publishing this source code, we are bypassing the publically documented API we provided for only reading a PDB - that was DIA
https://msdn.microsoft.com/en-us/library/x93ctkx8.aspx 

With this information, we are now building the information for other compilers (and tools) to efficiently write a PDB. 

The PDB format has not been officially documented, presenting a challenge for other compilers and
toolsets (such as Clang/LLVM) that want to work with Windows or the Visual Studio debugger. We want
to help the Open Source compilers to get onto the Windows platform.
 
The majority of content on this repo is presented as actual source files from the VC++ compiler 
toolset. The source code is the ultimate documentation :-) We hope that you will find it helpful. If you 
find that you need other information to successfully complete your project, please enter an
[Issue](https://github.com/microsoft/microsoft-pdb/issues) letting us know what information you need.

##Start here
The file pdb.h (on in langapi), provides the API surface for mscorpdb.dll, which we ship with every compiler and toolset.

Important points:

•	Mscorpdb.dll is what our linker and compiler use to create PDB files.
•	Mscorpdb.dll implements the “stream” abstractions.

Also, there is another file that we ship that should allow you to determine whether you have correctly produced an “empty” PDB which contains the minimal encoding to let another tool open and correctly parse that “empty” file.  “Empty” really meaning a properly formatted file where the sections contain the correct information to indicate zero records or symbols are present
A tool that I thought we also ship that would easily verify your “empty” PDB file is dia2dump.exe

So in summary, by using the externally defined function entry points in pdb.h you can call into mscorpdb.dll.

##What is a PDB

PDBs are files with multiple ‘streams’ of information in them. You can almost assume each stream as an individual file, except that storing them as individual files is wasteful and inconvenient, hence these multiple streams approach. PDB streams are not NTFS streams though. They can be implemented as NTFS streams, but since they are to be made available on Win9X as well, they use a home-brewed implementation. The implementation allows a primitive form of two-phase commit protocol. The writers of PDB files write whatever they want to in PDBs, but it won’t be committed until an explicit commit is issued. This allows the clients quite a bit of flexibility - say, for example, a compiler can keep on writing information, and just not commit it, if it encounters an error in users’ source code.

Each stream is identified with a unique stream number and an optional name. In a nutshell here’s how the PDB looks like -
	
| Stream No.			| Contents																								|Short Description
|--------------|---------------------------------|-------------------
| 1            | Pdb (header)	                   | Version information, and information to connect this PDB to the EXE
| 2	           | Tpi (Type manager)	             | All the types used in the executable.
| 3	           | Dbi (Debug information)	        | Holds section contributions, and list of ‘Mods’
| 4	           | NameMap	                        | Holds a hashed string table
| 4-(n+4)	     | n Mod’s (Module information)	   | Each Mod stream holds symbols and line numbers for one compiland
| n+4	         | Global symbol hash	             | An index that allows searching in global symbols by name
| n+5	         | Public symbol hash	             | An index that allows searching in public symbols by addresses
| n+6	         | Symbol records	                 | Actual symbol records of global and public symbols
| n+7	         | Type hash	                      | Hash used by the TPI stream.

