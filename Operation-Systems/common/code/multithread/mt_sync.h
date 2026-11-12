#pragma once

// Additions for lab3; existing thread API signatures are preserved.
#include <cstdint>
#include "mt.h"

namespace mt {
struct MutexHandleDummy;
using MutexHandle = MutexHandleDummy*;
struct BarrierHandleDummy;
using BarrierHandle = BarrierHandleDummy*;

// Creation throws std::runtime_error on an OS error.
// Operations/destruction are noexcept: unrecoverable errors terminate the process.
MutexHandle MutexCreate();
void MutexDestroy(MutexHandle handle) noexcept;
void MutexLock(MutexHandle handle) noexcept;
void MutexUnlock(MutexHandle handle) noexcept;

BarrierHandle BarrierCreate(unsigned participants);
void BarrierDestroy(BarrierHandle handle) noexcept;
void BarrierWait(BarrierHandle handle) noexcept;
}
