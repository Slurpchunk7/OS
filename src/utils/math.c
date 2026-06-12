#include "math.h"

#define PI      3.14159265358979323846
#define TWO_PI  6.28318530717958647692
#define HALF_PI 1.57079632679489661923

double floor(double x)
{
    return (double)(long long)x -
           (x < (double)(long long)x ? 1.0 : 0.0);
}

double ceil(double x)
{
    return (double)(long long)x +
           (x > (double)(long long)x ? 1.0 : 0.0);
}

double fabs(double x)
{
    return x < 0.0 ? -x : x;
}

double sqrt(double x)
{
    if (x <= 0.0)
        return 0.0;

    double r = x;

    for (int i = 0; i < 20; i++)
        r = (r + x / r) * 0.5;

    return r;
}

double pow(double b, double e)
{
    int n = (int)e;

    if (n == 0)
        return 1.0;

    if (n < 0)
    {
        n = -n;

        double r = 1.0;
        while (n--)
            r *= b;

        return 1.0 / r;
    }

    double r = 1.0;
    while (n--)
        r *= b;

    return r;
}

double fmod(double x, double y)
{
    if (y == 0.0)
        return 0.0;

    long long q = (long long)(x / y);
    return x - (double)q * y;
}

double sin(double x)
{
    while (x > PI)  x -= TWO_PI;
    while (x < -PI) x += TWO_PI;

    double x2 = x * x;

    return x
         - (x * x2) / 6.0
         + (x * x2 * x2) / 120.0
         - (x * x2 * x2 * x2) / 5040.0
         + (x * x2 * x2 * x2 * x2) / 362880.0;
}

double cos(double x)
{
    while (x > PI)  x -= TWO_PI;
    while (x < -PI) x += TWO_PI;

    double x2 = x * x;

    return 1.0
         - x2 / 2.0
         + (x2 * x2) / 24.0
         - (x2 * x2 * x2) / 720.0
         + (x2 * x2 * x2 * x2) / 40320.0
         - (x2 * x2 * x2 * x2 * x2) / 3628800.0;
}

double acos(double x)
{
    if (x >= 1.0)
        return 0.0;

    if (x <= -1.0)
        return PI;

    double negate = x < 0.0;

    if (negate)
        x = -x;

    double ret = -0.0187293;
    ret = ret * x + 0.0742610;
    ret = ret * x - 0.2121144;
    ret = ret * x + 1.5707288;
    ret = ret * sqrt(1.0 - x);

    ret = HALF_PI - ret;

    if (negate)
        ret = PI - ret;

    return ret;
}