#include <cstdlib>

// No environment variables on this platform
char* getenv(const char* name) {
    return 0;
}
