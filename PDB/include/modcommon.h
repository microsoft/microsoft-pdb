#pragma once

class ModCommon : public Mod {
public:
    BOOL AddLines(SZ_CONST szSrc, ISECT isect, OFF offCon, CB cbCon, OFF doff, LINE lineStart, PB pbCoff, CB cbCoff);
    BOOL QueryPdbFile(_Out_z_cap_(_MAX_PATH) OUT char szFile[_MAX_PATH], OUT CB* pcb);             // mts safe
    BOOL QueryCBName(OUT CB* pcb);
    BOOL QueryName(_Out_z_cap_(_MAX_PATH) OUT char szName[_MAX_PATH], OUT CB* pcb);
    BOOL QueryCBFile(OUT CB* pcb);
    BOOL QueryFile(_Out_z_cap_(_MAX_PATH) OUT char szFile[_MAX_PATH], OUT CB* pcb);
    BOOL QuerySrcFile(_Out_z_cap_(_MAX_PATH) OUT char szFile[_MAX_PATH], OUT CB* pcb);             // mts safe
};
