#pragma once

class SrcCommon : public Src {
public:
    virtual bool Add(IN PCSrcHeader psrcheader, IN const void * pvData);
    virtual bool QueryByName(IN SZ_CONST szFile, OUT PSrcHeaderOut psrcheaderOut) const;
};