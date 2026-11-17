// sprintf guard for the firmware build. No include guard on purpose: included
// by sim_prelude.h (force-included first) and again by hal/cstdio after the
// standard header has undefined the macro.
//
// sprintf into fixed buffers: the firmware has off-by-one overflows (a format
// one byte longer than the buffer, e.g. "       %03d" into char[10], or "%02d"
// into char[2]). On the device the extra NUL lands in a neighbouring stack
// slot and goes unnoticed; on x86_64 it can hit a saved register (the
// precount "%02d" into char[2] took the low byte of a saved `this`).
// sim_sprintf keeps the characters that fit, terminates inside the buffer,
// and reports the call site once. Buffers of unknown size are written
// normally.
#ifndef SIM_NO_PRELUDE
#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C"
#endif
int sim_sprintf(char* buffer, size_t size, const char* file, int line, const char* format, ...) __attribute__((format(printf, 5, 6)));
#undef sprintf
#define sprintf(buffer, ...) sim_sprintf((buffer), __builtin_object_size((buffer), 1), __FILE__, __LINE__, __VA_ARGS__)
#endif
