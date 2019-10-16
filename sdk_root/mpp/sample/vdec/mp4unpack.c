#include "mp4unpack.h"
#include <string.h>  

unsigned char NAL[]={0x00,0x00,0x00,0x01};
int oFrameCount = 0;
int ppsidx=0;
int spslen = 0,ppslen[64] = {0};
int videoindex = -1,audioindex = -1;
int nReadIndex = 0;
unsigned char sps[64],pps[2][64];

MP4Timestamp pStartTime=0;
MP4Duration pDuration=0;
MP4Duration pRenderingOffset=0;
//bool *pIsSyncSample = NULL;
bool IsSyncSample = 0;
bool first_flag=1;

MP4FileHandle openMp4File(char *pFileName)
{
    if(pFileName == NULL)  
    {  
        return false;  
    } 
    
    MP4FileHandle hMp4file = MP4Read(pFileName);
    if (hMp4file == MP4_INVALID_FILE_HANDLE)  
    {  
        printf("ERROR:Open file fialed.\n");  
        return false;  
    }  
   
    return hMp4file; 
}

int analysisMp4File(MP4FileHandle hMp4File)
{
    uint32_t i;
    uint32_t numSamples;
    
    MP4TrackId trackId = MP4_INVALID_TRACK_ID;
    uint32_t numTracks = MP4GetNumberOfTracks(hMp4File,NULL,0);
    printf("numTracks:%d\n",numTracks);
    
    
    for (i = 0; i < numTracks; i++)
    {
        trackId = MP4FindTrackId(hMp4File, i,NULL,0);
        const char* trackType = MP4GetTrackType(hMp4File, trackId);
        if (MP4_IS_VIDEO_TRACK_TYPE(trackType))
        {
            videoindex= trackId;

            numSamples = MP4GetTrackNumberOfSamples(hMp4File, trackId);
            oFrameCount = numSamples;
            printf("numSamples=%d\n",numSamples);
            printf("oFrameCount=%d\n",oFrameCount);

            // read sps/pps 
            uint8_t **seqheader;
            uint8_t **pictheader;
            uint32_t *pictheadersize;
            uint32_t *seqheadersize;
            uint32_t ix;
            MP4GetTrackH264SeqPictHeaders(hMp4File, trackId, &seqheader, &seqheadersize, &pictheader, &pictheadersize);

            for (ix = 0; seqheadersize[ix] != 0; ix++)
            {
                memcpy(sps, seqheader[ix], seqheadersize[ix]);
                spslen = seqheadersize[ix];
                free(seqheader[ix]);
            }
            free(seqheader);
            free(seqheadersize);
            printf("ix=%d@sps\n",ix);

            for (ix = 0; pictheadersize[ix] != 0; ix++)
            {
                memcpy(&pps[ix], pictheader[ix], pictheadersize[ix]);
                ppslen[ix] = pictheadersize[ix];
                free(pictheader[ix]);
            }
            free(pictheader);
            free(pictheadersize);
            ppsidx=ix;
            printf("ix=%d\n@pps",ix);
        }
        else if (MP4_IS_AUDIO_TRACK_TYPE(trackType))
        {
            audioindex = trackId;
            printf("audioindex:%d\n",audioindex);
        }   
    }
    
    return 0;
}

void closeMp4File(MP4FileHandle hMp4File)
{
    if(hMp4File)  
    {  
        MP4Close(hMp4File,0);  
        hMp4File = NULL;  
    }   
}

int ReadOneNaluFromBuf(const unsigned char *buffer,unsigned int nBufferSize,unsigned int offSet,MP4ENC_NaluUnit *pnalu)
{
    printf("call ReadOneNaluFromBuf()\n");
    unsigned int i = offSet;
    while(i<nBufferSize)
    {
        if(buffer[i++] == 0x00 &&
            buffer[i++] == 0x00 &&
            buffer[i++] == 0x00 &&
            buffer[i++] == 0x01
            )  // 确认找到NAL头
        {
			printf("found NAL header at i = %d\n",i);
            unsigned int pos = i;
            while (pos<nBufferSize)
            {
                if(buffer[pos++] == 0x00 &&
                    buffer[pos++] == 0x00 &&
                    buffer[pos++] == 0x00 &&
                    buffer[pos++] == 0x01
                    )
                {
                    break;   //确认找到下一个NAL头
                }
            }
            if(pos == nBufferSize)
            {
                pnalu->size = pos-i;    //处理几个NALU后的四字节NAL头，得出大小其实就是0，返回4
            }
            else
            {
                pnalu->size = (pos-4)-i;  // size是不包含NAL头四字节的
				printf("found NALU size = %d\n",pnalu->size);
            }
            // 两个NAL头之间就是nalu的大小size


            pnalu->type = buffer[i]&0x1f;
			printf("found NALU type = %d\n",pnalu->type);
			printf("pnalu->type = %d\n",pnalu->type);
            pnalu->data =(unsigned char*)&buffer[i];
            return (pnalu->size+i-offSet);  //这里的i-offset就是4啊
        }
    }
    return 0;
}

int getH264Stream(MP4FileHandle hMp4File, unsigned char* pData, int * pSize)
{
    if(!hMp4File) return -1;
    unsigned char *Data = NULL;
    unsigned int nSize = 0;
    
    
    if(nReadIndex <= oFrameCount)
    {    
    	//IsSyncSample = 0;
        MP4ReadSample(hMp4File,videoindex,nReadIndex,&Data,&nSize,&pStartTime,&pDuration,&pRenderingOffset,&IsSyncSample);
               
        if(IsSyncSample)
        {
            printf("nReadIndex=%d,oFrameCount=%d\n",nReadIndex,oFrameCount);
            memcpy(pData,NAL,4);
            memcpy(pData+4,sps,spslen);
            memcpy(pData+4+spslen,NAL,4);
            memcpy(pData+4+spslen+4,&pps[0],ppslen[0]);
            memcpy(pData+4+spslen+4+ppslen[0],NAL,4);
            memcpy(pData+4+spslen+4+ppslen[0]+4,&pps[1],ppslen[1]);
            printf("IsSyncSample, read again\n");
            //nReadIndex++;
            //printf("nReadIndex=%d,oFrameCount=%d\n",nReadIndex,oFrameCount);
            //MP4ReadSample(hMp4File,videoindex,nReadIndex,&Data,&nSize,&pStartTime,&pDuration,&pRenderingOffset,&pIsSyncSample);
            if(Data && nSize > 4)
            {
                Data[0] = 0x00;
                Data[1] = 0x00;
                Data[2] = 0x00;
                Data[3] = 0x01;

                MP4ENC_NaluUnit nalu;
                nSize = ReadOneNaluFromBuf(Data,nSize,0,&nalu);
                if(nalu.type == 0x06) // sei
                {
                    memcpy(pData+4+spslen+4+ppslen[0]+4+ppslen[1],Data,nSize);
                    *pSize = 4+spslen+4+ppslen[0]+4+ppslen[1]+nSize;
                    printf("*pSize=%d\n",*pSize);
                }
                else
                {
                	memcpy(pData,Data,nSize);
                	*pSize = nSize;
                }
            }
        }
        else if(Data && nSize > 4)
        {
            //printf("quit2=%d\n",*pquit);
            printf("nReadIndex=%d,oFrameCount=%d\n",nReadIndex,oFrameCount);
            
            Data[0] = 0x00;
            Data[1] = 0x00;
            Data[2] = 0x00;
            Data[3] = 0x01;
            memcpy(pData,Data,nSize);
        
            *pSize = nSize;
        }
        
        //pIsSyncSample_pre = pIsSyncSample;
        
        nReadIndex++;
        if(nReadIndex==oFrameCount+1)
        {    
            nReadIndex=0;
        }
    }
    
    return 0;
}


int writeH264Stream(FILE* outFile, unsigned char* pData, int nSize)
{
    if(!outFile) return -1;
    
//    //IDR֡ 帧，写入sps pps先
//    if(pIsSyncSample)
//    {
//        fwrite(NAL,4,1,outFile);
//        fwrite(sps,spslen,1,outFile);
//
//        fwrite(NAL,4,1,outFile);
//        fwrite(pps,ppslen,1,outFile);
//
//    }
//    //264frame
//    if(pData && nSize > 4)
//    {
//        //标准的264帧，前面几个字节就是frame的长度.
//        //需要替换为标准的264 nal 头.
//        pData[0] = 0x00;
//        pData[1] = 0x00;
//        pData[2] = 0x00;
//        pData[3] = 0x01;
//        fwrite(pData,nSize,1,outFile);
//    }
    //if((nReadIndex-1)==oFrameCount)
    //{
    //
    //}
    //else
    //{
    //    fwrite(pData,nSize,1,outFile);
    //}
    
    fwrite(pData,nSize,1,outFile);
    
    fflush(outFile);
    return 0;
}

