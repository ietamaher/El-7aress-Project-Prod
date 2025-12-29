#ifndef VERSION_H
#define VERSION_H

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// These will be defined from .pro file
#ifndef APP_VERSION_FULL
#define APP_VERSION_FULL "unknown"
#endif

namespace AppVersion {
    inline const char* version() { return APP_VERSION_FULL; }
    inline const char* fullVersion() { return APP_VERSION_FULL; }
}

#endif // VERSION_H