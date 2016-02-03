#ifndef __ENUM_INCLUDED__
#define __ENUM_INCLUDED__

// Expected enumerator usage:
//	XS xs;
//	EnumXS exs(xs);
//	while (exs.next())
//		exs.get(&x);
//	exs.reset();
//	while (exs.next())
//		exs.get(&x)

class Enum {
public:
	virtual ~Enum() { }
	virtual void reset() { }
	virtual BOOL next() {
		return TRUE;
	}
};

#endif // !__ENUM_INCLUDED__
