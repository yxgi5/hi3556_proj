#ifndef __MP4UNPACK_H
#define __MP4UNPACK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>		// getopt_long()
#include <fcntl.h>		// for open()
#include <unistd.h>     // for close()
#include <errno.h>
#include <malloc.h>
#include <mp4v2/mp4v2.h>

// NALU单元
typedef struct _MP4ENC_NaluUnit
{
    int type;
    int size;
    unsigned char *data;
}MP4ENC_NaluUnit;

MP4FileHandle openMp4File(char *pFileName);
int analysisMp4File(MP4FileHandle hMp4File);
void closeMp4File(MP4FileHandle hMp4File);
int getH264Stream(MP4FileHandle hMp4File, unsigned char* pData, int * pSize, int * pquit);
int writeH264Stream(FILE* outFile, unsigned char* pData, int nSize);

#endif // __MP4UNPACK_H
