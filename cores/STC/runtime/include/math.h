#ifndef STCXX_FREESTANDING_MATH_H
#define STCXX_FREESTANDING_MATH_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

#if !defined(__STC_CLANG_IR_ONLY__)
# if defined(__GNUC__) || defined(__clang__)
#  include_next <math.h>
# else
#  error "The STC math.h shim needs either the STC target or include_next"
# endif
#else

/* Arduino.h defines round as a macro.  Preserve it for sketches without
 * allowing it to corrupt the freestanding C math declaration below. */
#if defined(round)
# pragma push_macro("round")
# undef round
# define STCXX_MATH_RESTORE_ROUND 1
#endif

#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

#define NAN __builtin_nanf("")
#define INFINITY __builtin_inff()
#define HUGE_VAL __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()

#ifdef __cplusplus
extern "C" {
#endif

/* The STC ABI deliberately uses IEEE-754 binary32 for both float and double.
 * SDCC's runtime exports the C99 *f spellings only, so bind the unsuffixed C
 * API to those exact linker symbols.  Keeping the aliases target-only avoids
 * changing the host conformance build's native double ABI. */
#define STCXX_MATH_FLOAT_SYMBOL(name) __asm__(#name "f")

double acos(double value) STCXX_MATH_FLOAT_SYMBOL(acos);
double asin(double value) STCXX_MATH_FLOAT_SYMBOL(asin);
double atan(double value) STCXX_MATH_FLOAT_SYMBOL(atan);
double atan2(double y, double x) STCXX_MATH_FLOAT_SYMBOL(atan2);
double cos(double value) STCXX_MATH_FLOAT_SYMBOL(cos);
double sin(double value) STCXX_MATH_FLOAT_SYMBOL(sin);
double tan(double value) STCXX_MATH_FLOAT_SYMBOL(tan);
double cosh(double value) STCXX_MATH_FLOAT_SYMBOL(cosh);
double sinh(double value) STCXX_MATH_FLOAT_SYMBOL(sinh);
double tanh(double value) STCXX_MATH_FLOAT_SYMBOL(tanh);
double exp(double value) STCXX_MATH_FLOAT_SYMBOL(exp);
double frexp(double value, int *exponent) STCXX_MATH_FLOAT_SYMBOL(frexp);
double ldexp(double value, int exponent) STCXX_MATH_FLOAT_SYMBOL(ldexp);
double log(double value) STCXX_MATH_FLOAT_SYMBOL(log);
double log10(double value) STCXX_MATH_FLOAT_SYMBOL(log10);
double modf(double value, double *integer_part) STCXX_MATH_FLOAT_SYMBOL(modf);
double pow(double base, double exponent) STCXX_MATH_FLOAT_SYMBOL(pow);
double sqrt(double value) STCXX_MATH_FLOAT_SYMBOL(sqrt);
double ceil(double value) STCXX_MATH_FLOAT_SYMBOL(ceil);
double fabs(double value) STCXX_MATH_FLOAT_SYMBOL(fabs);
double floor(double value) STCXX_MATH_FLOAT_SYMBOL(floor);
double fmod(double numerator, double denominator) STCXX_MATH_FLOAT_SYMBOL(fmod);
double fmin(double x, double y) STCXX_MATH_FLOAT_SYMBOL(fmin);
double fmax(double x, double y) STCXX_MATH_FLOAT_SYMBOL(fmax);
double round(double value) STCXX_MATH_FLOAT_SYMBOL(round);
double trunc(double value) STCXX_MATH_FLOAT_SYMBOL(trunc);

float acosf(float value);
float asinf(float value);
float atanf(float value);
float atan2f(float y, float x);
float cosf(float value);
float sinf(float value);
float tanf(float value);
float coshf(float value);
float sinhf(float value);
float tanhf(float value);
float expf(float value);
float frexpf(float value, int *exponent);
float ldexpf(float value, int exponent);
float logf(float value);
float log10f(float value);
float modff(float value, float *integer_part);
float powf(float base, float exponent);
float sqrtf(float value);
float ceilf(float value);
float fabsf(float value);
float floorf(float value);
float fmodf(float numerator, float denominator);
float fminf(float x, float y);
float fmaxf(float x, float y);
float roundf(float value);
float truncf(float value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#undef STCXX_MATH_FLOAT_SYMBOL

#ifdef __cplusplus
static inline bool isfinite(double value) { return __builtin_isfinite(value); }
static inline bool isinf(double value) { return __builtin_isinf(value); }
static inline bool isnan(double value) { return __builtin_isnan(value); }
static inline bool signbit(double value) { return __builtin_signbit(value); }
#else
# define isfinite(value) __builtin_isfinite(value)
# define isinf(value) __builtin_isinf(value)
# define isnan(value) __builtin_isnan(value)
# define signbit(value) __builtin_signbit(value)
#endif

#if defined(STCXX_MATH_RESTORE_ROUND)
# pragma pop_macro("round")
# undef STCXX_MATH_RESTORE_ROUND
#endif

#endif /* STC target */
#endif
