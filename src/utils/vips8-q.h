/* Qt and C++-safe inclusion of VIPS */

#undef signals
#include <vips/vips8>
#define signals Q_SIGNALS
