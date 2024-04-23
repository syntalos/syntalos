/* Qt and C++-safe inclusion of VIPS */

#if defined(signals) && defined(Q_SIGNALS)
#define _VIPS_QT_SIGNALS_DEFINED
#undef signals
#endif

#include <vips/vips8>

#ifdef _VIPS_QT_SIGNALS_DEFINED
#define signals Q_SIGNALS
#undef _VIPS_QT_SIGNALS_DEFINED
#endif
