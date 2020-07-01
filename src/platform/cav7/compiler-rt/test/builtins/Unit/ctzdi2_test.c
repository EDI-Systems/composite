// RUN: %clang_builtins %s %librt -o %t && %run %t
// REQUIRES: librt_has_ctzdi2
//===-- ctzdi2_test.c - Test __ctzdi2 -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file tests __ctzdi2 for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include "int_lib.h"
#include <stdio.h>

// Returns: the number of trailing 0-bits

// Precondition: a != 0

COMPILER_RT_ABI si_int __ctzdi2(di_int a);

int test__ctzdi2(di_int a, si_int expected)
{
    si_int x = __ctzdi2(a);
    if (x != expected)
        printf("error in __ctzdi2(0x%llX) = %d, expected %d\n", a, x, expected);
    return x != expected;
}

char assumption_1[sizeof(di_int) == 2*sizeof(si_int)] = {0};

int main()
{
//    if (test__ctzdi2(0x00000000, N))  // undefined
//        return 1;
    if (test__ctzdi2(0x00000001, 0))
        return 1;
    if (test__ctzdi2(0x00000002, 1))
        return 1;
    if (test__ctzdi2(0x00000003, 0))
        return 1;
    if (test__ctzdi2(0x00000004, 2))
        return 1;
    if (test__ctzdi2(0x00000005, 0))
        return 1;
    if (test__ctzdi2(0x0000000A, 1))
        return 1;
    if (test__ctzdi2(0x10000000, 28))
        return 1;
    if (test__ctzdi2(0x20000000, 29))
        return 1;
    if (test__ctzdi2(0x60000000, 29))
        return 1;
    if (test__ctzdi2(0x80000000uLL, 31))
        return 1;
    if (test__ctzdi2(0x0000050000000000uLL, 40))
        return 1;
    if (test__ctzdi2(0x0200080000000000uLL, 43))
        return 1;
    if (test__ctzdi2(0x7200000000000000uLL, 57))
        return 1;
    if (test__ctzdi2(0x8000000000000000uLL, 63))
        return 1;

   return 0;
}