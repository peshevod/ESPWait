/*
 * CppTest.cpp
 *
 *  Created on: 31 июл. 2021 г.
 *      Author: ilya_000
 */

#include "CppTest.h"
#include <iostream>

CppTest::CppTest() {
    std::cout << "This is CPP!"  << "\n";
}

CppTest::~CppTest() {
	// TODO Auto-generated destructor stub
}

extern "C" CppTest* CppTest_new(void)
{
	return new CppTest();
}
