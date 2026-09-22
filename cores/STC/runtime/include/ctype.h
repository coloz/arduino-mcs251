#ifndef STCXX_FREESTANDING_CTYPE_H
#define STCXX_FREESTANDING_CTYPE_H

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC system_header
#endif

#if !defined(__STC_CLANG_IR_ONLY__)
# if defined(__GNUC__) || defined(__clang__)
#  include_next <ctype.h>
# else
#  error "The STC ctype.h shim needs either the STC target or include_next"
# endif
#else

#ifdef __cplusplus
extern "C" {
#endif

int isalnum(int value);
int isalpha(int value);
int isblank(int value);
int iscntrl(int value);
int isdigit(int value);
int isgraph(int value);
int islower(int value);
int isprint(int value);
int ispunct(int value);
int isspace(int value);
int isupper(int value);
int isxdigit(int value);
int tolower(int value);
int toupper(int value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STC target */
#endif
