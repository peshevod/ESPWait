/*
 * CppTest.h
 *
 *  Created on: 31 июл. 2021 г.
 *      Author: ilya_000
 */

#ifndef COMPONENTS_CPPTEST_CPPTEST_H_
#define COMPONENTS_CPPTEST_CPPTEST_H_

#ifdef	__cplusplus
#define EXPORT_C extern "C"

class CppTest {
public:
	CppTest();
	virtual ~CppTest();
};

#else

#define EXPORT_C
typedef struct CppTest CppTest;

#endif // __cplusplus



EXPORT_C CppTest* CppTest_new(void);


#endif /* COMPONENTS_CPPTEST_CPPTEST_H_ */
