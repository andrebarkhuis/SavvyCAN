// Force-included (/FI) by SavvyCAN.pro and src/plugins/passthrucan/passthrucan.pro when
// building with MSVC.
//
// Visual Studio 2026 (MSVC 14.50+, _MSC_VER >= 1950) removed the long-deprecated
// stdext::checked_array_iterator family from its STL, but Qt 5.15's
// qcompilerdetection.h still expands QT_MAKE_CHECKED_ARRAY_ITERATOR /
// QT_MAKE_UNCHECKED_ARRAY_ITERATOR to those functions (used by QVector::operator==
// and friends), which breaks compilation of anything that instantiates them.
// Provide plain-pointer replacements, which is exactly what Qt uses on every
// other compiler.
#pragma once

#if defined(_MSC_VER) && _MSC_VER >= 1950
#include <cstddef>
namespace stdext {
template <class T>
inline T *make_checked_array_iterator(T *p, std::size_t /*size*/, std::size_t /*index*/ = 0)
{
    return p;
}
template <class T>
inline T *make_unchecked_array_iterator(T *p)
{
    return p;
}
} // namespace stdext
#endif
