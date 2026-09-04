#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>

#define Assert(Expr) \
    if (!(Expr)) \
    { \
        raise(SIGTRAP); \
    }


int
main(int ArgC, char *Args[])
{
    if (ArgC < 2)
    {
        fprintf(stderr, "Missing argument.\n");
        fprintf(stderr, "Usage: %s <filename>\n", Args[0]);
    }
    else
    {    
        struct stat StatBuffer = {0};
        char *Filename = 0;
        int FD = -1;
        int Err = -1;
        size_t Filesize = 0;
        char *Buffer = 0;
        
        Filename = Args[1];
        FD = open(Filename, O_RDONLY);
        Assert(FD != -1);
        Err = stat(Filename, &StatBuffer);
        Assert(Err != -1);
        Filesize = StatBuffer.st_size;

        if (Filesize)
        {
            Buffer = mmap(0, Filesize, PROT_READ, MAP_SHARED, FD, 0);
            Assert(Buffer);

            for (size_t At = 0; At < Filesize; At++)
            {
                unsigned char Byte = Buffer[At];
                int Count = 8;
                while (Count--)
                {
                    printf("%d", Byte >> 7);
                    Byte <<= 1;
                }
                printf(" ");
            }
            printf("\n");
        }
        else
        {
            fprintf(stderr, "Empty file.\n");
        }
    }

    return 0;
}
