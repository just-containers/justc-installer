#include "tar.h"
#include "scan.h"
#include "zero.h"

#include <stdlib.h>
#include <string.h>

int justc_tar_parse(justc_tar *d, const char *s, size_t len) {
    if(len < 500) return 1;
    if(memcmp(s,zero_block,512) == 0) return 1;

    d->filename[0] = '\0';
    d->filename[100] = '\0';
    d->mode = 0;
    d->uid = 0;
    d->gid = 0;
    d->size = 0;
    d->mtime = 0;
    d->checksum[0] = '\0';
    d->type = 0;
    d->linked[0] = '\0';
    d->linked[100] = '\0';
    d->ustar_ind[0] = '\0';
    d->ustar_ver = 0;
    d->username[0] = '\0';
    d->username[32] = '\0';
    d->groupname[0] = '\0';
    d->groupname[32] = '\0';
    d->dev_maj[0] = '\0';
    d->dev_min[0] = '\0';
    d->filename_prefix[0] = '\0';
    d->filename_prefix[155] = '\0';

    memcpy(d->filename,&s[0],100);
    d->mode = (mode_t)justc_uint64_scan(&s[100],8,8);
    d->uid = (uid_t)justc_uint64_scan(&s[108],8,8);
    d->gid = (gid_t)justc_uint64_scan(&s[116],8,8);
    d->size = (size_t)justc_uint64_scan(&s[124],12,8);
    d->mtime = (time_t)justc_uint64_scan(&s[136],12,8);
    d->type = s[156];
    if(d->type == '\0') d->type = '0';
    memcpy(d->linked,&s[157],100);
    memcpy(d->ustar_ind,&s[257],6);
    d->ustar_ver = (uint8_t)justc_uint64_scan(&s[263],2,10);
    memcpy(d->username, &s[265],32);
    memcpy(d->groupname, &s[297],32);
    memcpy(d->filename_prefix, &s[345],155);

    return 0;
}


