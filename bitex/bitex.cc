#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include <vector>

using std::vector;

static const int BUFSIZE = 8192;

int main(int argc, char ** argv) 
{
    int infd = STDIN_FILENO;
    int outfd = STDOUT_FILENO;
    assert(argc == 2 || argc == 3);
    const int bitex = atoi(argv[1]);
    assert(bitex < 8 || bitex >= 0);

    if (argc == 3)
    {
        char *infn = argv[2];
        infd = open(infn, O_RDONLY);
        if (infd < 0) {
            fprintf(stderr, "failed opening '%s', %s\n", infn, strerror(errno));
        }
    }

    vector<uint8_t> inbuf(BUFSIZE);
    vector<uint8_t> outbuf(BUFSIZE/8);
    const uint8_t bitmask = 1 << bitex;

    while (true) 
    {
        for (size_t i = 0; i < inbuf.size(); ) 
        {
            int ret = read(infd, inbuf.data()+i, inbuf.size() - i);
            if (ret == 0)
            {
                // XXX aren't flushing the final buffer
                return 0;
            }
            else if (ret < 0)
            {
                if (errno != EINTR)
                {
                    fprintf(stderr, "failed reading, %s\n", strerror(errno));
                    return 2;
                }
            }
            else
            {
                i += ret;
            }
        }

        for (size_t i = 0, j = 0; j < outbuf.size(); i++, j++)
        {
            outbuf[j] = (inbuf[i] & bitmask) >> bitex << 7
                | (inbuf[i+1] & bitmask) >> bitex << 6
                | (inbuf[i+2] & bitmask) >> bitex << 5
                | (inbuf[i+3] & bitmask) >> bitex << 4
                | (inbuf[i+4] & bitmask) >> bitex << 3
                | (inbuf[i+5] & bitmask) >> bitex << 2
                | (inbuf[i+6] & bitmask) >> bitex << 1
                | (inbuf[i+7] & bitmask) >> bitex << 0;
        }

        for (size_t i = 0; i < outbuf.size(); ) 
        {
            int ret = write(outfd, outbuf.data()+i, outbuf.size() - i);
            if (ret < 0)
            {
                if (errno != EINTR)
                {
                    fprintf(stderr, "failed writing, %s\n", strerror(errno));
                    return 3;
                }
            }
            else
            {
                i += ret;
            }
        }
    }


    return 9;
}
