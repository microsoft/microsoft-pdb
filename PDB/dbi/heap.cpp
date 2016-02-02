#include "pdbimpl.h"
#include "dbiimpl.h"

#pragma warning(disable:4073)
#pragma init_seg(lib)

#if defined(PDB_SERVER) || defined(MSOBJ_DLL) || defined(STANDALONE_HEAP)

Heap Heap::theHeap;
HANDLE Heap::hheap;

#endif // defined(PDB_SERVER) || defined(MSOBJ_DLL) || defined(STANDALONE_HEAP)

namespace pdb_internal {
    SysPage cbSysPage;
    }
