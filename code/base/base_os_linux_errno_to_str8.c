#include "base/base.h"
#include <errno.h>

str8 
ErrnoToStr8(s32 Value)
{
    str8 Message = ZeroStruct;
    switch(Value)
    {
        case EPERM:   Message = S8("EPERM: Operation not permitted");            break;
        case ENOENT:  Message = S8("ENOENT: No such file or directory");         break;
        case ESRCH:   Message = S8("ESRCH: No such process");                    break;
        case EINTR:   Message = S8("EINTR: Interrupted system call");            break;
        case EIO:     Message = S8("EIO: I/O error");                            break;
        case ENXIO:   Message = S8("ENXIO: No such device or address");          break;
        case E2BIG:   Message = S8("E2BIG: Argument list too long");             break;
        case ENOEXEC: Message = S8("ENOEXEC: Exec format error");                break;
        case EBADF:   Message = S8("EBADF: Bad file number");                    break;
        case ECHILD:  Message = S8("ECHILD: No child processes");                break;
        case EAGAIN:  Message = S8("EAGAIN: Try again");                         break;
        case ENOMEM:  Message = S8("ENOMEM: Out of memory");                     break;
        case EACCES:  Message = S8("EACCES: Permission denied");                 break;
        case EFAULT:  Message = S8("EFAULT: Bad address");                       break;
        case ENOTBLK: Message = S8("ENOTBLK: Block device required");            break;
        case EBUSY:   Message = S8("EBUSY: Device or resource busy");            break;
        case EEXIST:  Message = S8("EEXIST: File exists");                       break;
        case EXDEV:   Message = S8("EXDEV: Cross-device link");                  break;
        case ENODEV:  Message = S8("ENODEV: No such device");                    break;
        case ENOTDIR: Message = S8("ENOTDIR: Not a directory");                  break;
        case EISDIR:  Message = S8("EISDIR: Is a directory");                    break;
        case EINVAL:  Message = S8("EINVAL: Invalid argument");                  break;
        case ENFILE:  Message = S8("ENFILE: File table overflow");               break;
        case EMFILE:  Message = S8("EMFILE: Too many open files");               break;
        case ENOTTY:  Message = S8("ENOTTY: Not a typewriter");                  break;
        case ETXTBSY: Message = S8("ETXTBSY: Text file busy");                   break;
        case EFBIG:   Message = S8("EFBIG: File too large");                     break;
        case ENOSPC:  Message = S8("ENOSPC: No space left on device");           break;
        case ESPIPE:  Message = S8("ESPIPE: Illegal seek");                      break;
        case EROFS:   Message = S8("EROFS: Read-only file system");              break;
        case EMLINK:  Message = S8("EMLINK: Too many links");                    break;
        case EPIPE:   Message = S8("EPIPE: Broken pipe");                        break;
        case EDOM:    Message = S8("EDOM: Math argument out of domain of func"); break;
        case ERANGE:  Message = S8("ERANGE: Math result not representable");     break;
    }
    
    return Message;
}
