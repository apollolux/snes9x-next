/* Included at the beginning of library source files, after all other #include lines.
Sets up helpful macros and services used in my source code. They don't need
module an annoying module prefix on their names since they are defined after
all other #include lines. */

// snes_spc 0.9.0
#ifndef BLARGG_SOURCE_H
#define BLARGG_SOURCE_H

// Like printf() except output goes to debug log file. Might be defined to do
// nothing (not even evaluate its arguments).
// void dprintf( const char* format, ... );
static inline void blargg_dprintf_( const char*, ... ) { }
#undef dprintf
#define dprintf (1) ? (void) 0 : blargg_dprintf_

// If expr yields error string, return it from current function, otherwise continue.
#undef RETURN_ERR
#define RETURN_ERR( expr ) do {                         \
		blargg_err_t blargg_return_err_ = (expr);               \
		if ( blargg_return_err_ ) return blargg_return_err_;    \
	} while ( 0 )

// If ptr is 0, return out of memory error string.
#undef CHECK_ALLOC
#define CHECK_ALLOC( ptr ) do { if ( (ptr) == 0 ) return "Out of memory"; } while ( 0 )

// Avoid any macros which evaluate their arguments multiple times
#undef min
#undef max

#define DEF_MIN_MAX( type ) \
	static inline type min( type x, type y ) { if ( x < y ) return x; return y; }\
	static inline type max( type x, type y ) { if ( y < x ) return x; return y; }

DEF_MIN_MAX( int )
DEF_MIN_MAX( unsigned )
DEF_MIN_MAX( long )
DEF_MIN_MAX( unsigned long )
DEF_MIN_MAX( float )
DEF_MIN_MAX( double )

#undef DEF_MIN_MAX

// TODO: good idea? bad idea?
#undef byte
#define byte byte_
typedef unsigned char byte;

// deprecated
#define BLARGG_CHECK_ALLOC CHECK_ALLOC
#define BLARGG_RETURN_ERR RETURN_ERR

// BLARGG_SOURCE_BEGIN: If defined, #included, allowing redefition of dprintf and check
#ifdef BLARGG_SOURCE_BEGIN
	#include BLARGG_SOURCE_BEGIN
#endif

#endif
