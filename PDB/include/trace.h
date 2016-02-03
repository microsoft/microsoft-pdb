#pragma once
#ifndef __TRACE_INCLUDED__
#define __TRACE_INCLUDED__

#if defined(_DEBUG)

#if defined(_UNICODE) || defined(UNICODE)
#define trace(args) traceW_ args
#else
#define trace(args) trace_ args
#endif

#define traceOnly(x) x

#else
#define trace(args)	0
#define traceOnly(x)
#endif

enum TR {
	trMap,
	trSave,
	trStreams,
	trStreamImage,
	trStreamImageSummary,
	trMax
};
BOOL trace_(TR tr, const char* szFmt, ...);
BOOL traceW_(TR tr, const wchar_t* szFmt, ...);

#endif // !__TRACE_INCLUDED__
