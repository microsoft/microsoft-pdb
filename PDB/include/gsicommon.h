#pragma once
// These should migrate to the real GSI interfaces

struct _GSI : public GSI {
    virtual BOOL getEnumSyms(EnumSyms **, PB) pure;
};