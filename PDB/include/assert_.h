// assert_.h - assert specific to the pdb project
#if defined(__cplusplus)

extern "C" void failAssertion(const char* szFile, int line);
extern "C" void failExpect(const char* szFile, int line);
extern "C" void failAssertionFunc(const char* szFile, const char * szFunc, int line, const char* szCond);
extern "C" void failExpectFunc(const char* szFile, const char * szFunc, int line, const char* szCond);

#else // i.e not C++

void failAssertion(const char* szFile, int line);
void failExpect(const char* szFile, int line);
void failAssertionFunc(const char* szFile, const char * szFunc, int line, const char* szCond);
void failExpectFunc(const char* szFile, const char * szFunc, int line, const char* szCond);

#endif

#if defined(_DEBUG)

#define assert(x)		if (!(x)) { failAssertionFunc(__FILE__, __FUNCTION__, __LINE__, #x); } else
#define expect(x)		if (!(x)) { failExpectFunc(__FILE__ , __FUNCTION__, __LINE__, #x); } else
#define expectFailed(x)	failExpectFunc(__FILE__ , __FUNCTION__, __LINE__, x)
#define rangeCheck(val, valMin, valMax) \
( (!((val >= valMin) && (val < valMax))) ? (failExpectFunc(__FILE__, __FUNCTION__, __LINE__, "(rangeCheck(" #val ", " #valMin ", " #valMax "))"), val) : val)
#define assume(x)       assert(x)

#define verify(x)		assert(x)
#define	dassert(x)		assert(x)
extern BOOL rgbEnableDiagnostic[20];
#define	dprintf(args)	printf args
#define	debug(x)		x

#else
#define assert(x)       ((void)0)
#define verify(x)       (x,0)
#define	dassert(x)
#define expect(x)
#define	dprintf(args)
#define	debug(x)
#define rangeCheck(val, valMin, valMax) (val)
#define assume(x)       __assume(x)
#endif

// compile time assert
#if !defined(cassert)
#define cassert(x) extern char dummyAssert[ (x) ]
#endif
