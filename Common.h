#ifndef _COMMON_H_
#define _COMMON_H_

inline int Wrap(int x, int xMax) {
    int result = (x + xMax) % xMax;
    return result;
}

inline double Max(double a, double b) {
    return a > b ? a : b;
}

inline float Max(float a, float b) {
    return a > b ? a : b;
}

#endif //!_COMMON_H_