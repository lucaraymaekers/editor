/* date = January 15th 2026 5:40 pm */

#if !defined(BASE_OS_LINUX_H)
#define BASE_OS_LINUX_H

#include <dirent.h>

typedef struct os_dir_opaque os_dir_opaque;
struct os_dir_opaque
{
    DIR *Dir;
    struct dirent *Entry;
};

internal s64 LinuxTimeSpecTo(struct timespec Counter);

#endif //BASE_OS_LINUX_H
