// Small generic helpers shared by Global.h and Fill.h.
#ifndef __UTIL_H
#define __UTIL_H

template <typename t>
t clamp(t x, t min, t max) {
    if (x < min) x = min;
    if (x > max) x = max;
    return x;
}

#endif
