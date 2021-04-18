#if !defined (HANDMADE_INTRINSICS_H)

#include "math.h"

inline int32
RoundReal32ToInt32(real32 RealVal)
{
    int32 Result = (int32)roundf(RealVal);
    return Result;
}

inline uint32
RoundReal32ToUInt32(real32 RealVal)
{
    uint32 Result = (uint32)roundf(RealVal);
    return Result;
}

inline int32
FloorReal32ToInt32(real32 RealVal)
{
    int32 Result = (int32)floorf(RealVal);
    return Result;
}

inline int32
TruncateReal32ToInt32(real32 RealVal)
{
    int32 Result = (int32)RealVal;
    return Result;
}

inline real32
Sin(real32 Angle)
{
    real32 Result = sinf(Angle);
    return Result;
}

inline real32
Cos(real32 Angle)
{
    real32 Result = cosf(Angle);
    return Result;
}

inline real32
ATan2(real32 Y, real32 X)
{
    real32 Result = atan2f(Y, X);
    return Result;
}


inline real32
AbsoluteValue(real32 Real32)
{
    real32 Result = fabs(Real32);
    return Result;
}

inline uint32
RotateLeft(uint32 Value, int32 Ammount)
{
    uint32 Result = _rotl(Value, Ammount);
    return Result;
}

inline uint32
RotateRight(uint32 Value, int32 Ammount)
{
    uint32 Result = _rotr(Value, Ammount);
    return Result;
}

struct bit_scan_result
{
    uint32 Index;
    bool32 Found;
};

inline bit_scan_result
FindLeastSignificantSetBit(uint32 Value)
{
    bit_scan_result Result = {};

#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long*)&Result.Index, Value);
#else
    for(uint32 Test = 0; Test < 32; ++Test)
    {
        if(Value & (1 << Test))
        {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
#endif

    return Result;
}

#define HANDMADE_INTRINSICS_H
#endif
