#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

struct justc_tar_s {
    char filename[101];
    mode_t mode;
    uid_t uid;
    gid_t gid;
    size_t size;
    time_t mtime;
    char checksum[8];
    char type;
    char linked[101];
    char ustar_ind[6];
    uint8_t ustar_ver;
    char username[33];
    char groupname[33];
    char dev_maj[8];
    char dev_min[8];
    char filename_prefix[156];
};

typedef struct justc_tar_s justc_tar;

int justc_tar_parse(justc_tar *d, const char *s, size_t len);
