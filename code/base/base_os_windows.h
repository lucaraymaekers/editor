/* date = August 28th 2026 3:47 pm */

#ifndef BASE_OS_WINDOWS_H
#define BASE_OS_WINDOWS_H

typedef struct os_dir_opaque os_dir_opaque;
struct os_dir_opaque
{
    HANDLE Handle;
};

#endif //BASE_OS_WINDOWS_H
