/********************************************************************  
filename:   MP4Encoder.c
created:    2013-04-16 
author:     firehood  
purpose:    MP4编码器，基于开源库mp4v2实现（https://code.google.com/p/mp4v2/）。 
*********************************************************************/  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include "MP4Encoder.h"

#define BUFFER_SIZE  (1024*1024)  
int frame=0;
  
MP4FileHandle CreateMP4File(const char *pFileName,int width,int height,int timeScale/* = 90000*/,int frameRate/* = 25*/)  
{  
    if(pFileName == NULL)  
    {  
        return NULL;  
    }  
    // create mp4 file  
    MP4FileHandle hMp4file = MP4Create(pFileName,0);  
    if (hMp4file == MP4_INVALID_FILE_HANDLE)  
    {  
        //printf("ERROR:Open file fialed.\n");  
        return NULL;  
    }  
    m_nWidth = width;  
    m_nHeight = height;  
    m_nTimeScale = timeScale;  
    m_nFrameRate = frameRate;  
    MP4SetTimeScale(hMp4file, m_nTimeScale);  
    return hMp4file;  
}  
  
  
int WriteH264Data(MP4FileHandle hMp4File,const unsigned char* pData,int size)  
{  
    //printf("call WriteH264Data()\n");
    if(hMp4File == NULL)  
    {  
        return -1;  
    }  
    if(pData == NULL)  
    {  
        return -1;  
    }  
    MP4ENC_NaluUnit nalu;  
    int pos = 0, len = 0;  
    while (len = ReadOneNaluFromBuf(pData,size,pos,&nalu))  //返回一个NALU的完整大小（含NAL头）
    {  
		//printf("nalu.type = %d\n",nalu.type);
        if(nalu.type == 0x07) // sps  
        {  
			//printf("NALU type is sps\n");
	    if(m_videoId==NULL)
	    {
            	// 添加h264 track      
            	m_videoId = MP4AddH264VideoTrack  
                (hMp4File,   
                m_nTimeScale,   
                m_nTimeScale / m_nFrameRate,   
                m_nWidth,     // width  
                m_nHeight,    // height  
                nalu.data[1], // sps[1] AVCProfileIndication  
                nalu.data[2], // sps[2] profile_compat  
                nalu.data[3], // sps[3] AVCLevelIndication  
                3);           // 4 bytes length before each NAL unit  
		    if (m_videoId == MP4_INVALID_TRACK_ID)  
		    {  
			//printf("add video track failed.\n");  
			return 0;  
		    }  
            	//printf("call MP4AddH264VideoTrack(),m_nWidth=%d,m_nHeight=%d,nalu.data[1]=%d,nalu.data[2]=%d,nalu.data[3]=%d\n",m_nWidth,m_nHeight,nalu.data[1],nalu.data[2],nalu.data[3]);
            	MP4SetVideoProfileLevel(hMp4File, 1); //  Simple Profile @ Level 3  
	    }
            //printf("call MP4AddH264SequenceParameterSet() in case nalu.type == 0x07\n");
            MP4AddH264SequenceParameterSet(hMp4File,m_videoId,nalu.data,nalu.size);  
        }  
        else if(nalu.type == 0x08) // pps  
        {  
            //printf("call MP4AddH264PictureParameterSet() in case nalu.type == 0x8\n");
            MP4AddH264PictureParameterSet(hMp4File,m_videoId,nalu.data,nalu.size);  
        }  
        else  
        {  
            int datalen = nalu.size+4;  
            unsigned char data[datalen];  
            // MP4 Nalu前四个字节表示Nalu长度,并且是大端顺序（big endian）
            data[0] = nalu.size>>24;  
            data[1] = nalu.size>>16;  
            data[2] = nalu.size>>8;  
            data[3] = nalu.size&0xff;  
            //printf("data[0]=%d,data[1]=%d,data[2]=%d,data[3]=%d\n",data[0],data[1],data[2],data[3]);
            memcpy(data+4,nalu.data,nalu.size);  // dest，src，size
            // 现在data指向的内存区存储了所需数据
            
            //printf("call MP4WriteSample(),frame = %d\n",frame++);
            if(!MP4WriteSample(hMp4File, m_videoId, data, datalen, MP4_INVALID_DURATION, 0, 1))  
            {  
                return 0;  
            }   
        }  
          
        pos += len;  
    }  
    return pos;  
}  
  
int ReadOneNaluFromBuf(const unsigned char *buffer,unsigned int nBufferSize,unsigned int offSet,MP4ENC_NaluUnit *pnalu)  
{  
    //printf("call ReadOneNaluFromBuf()\n");
    unsigned int i = offSet;  
    while(i<nBufferSize)  
    {  
        if(buffer[i++] == 0x00 &&  
            buffer[i++] == 0x00 &&  
            buffer[i++] == 0x00 &&  
            buffer[i++] == 0x01  
            )  // 确认找到NAL头
        {  
			//printf("found NAL header at i = %d\n",i);
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
				//printf("found NALU size = %d\n",pnalu->size);
            }  
            // 两个NAL头之间就是nalu的大小size
            
            
            pnalu->type = buffer[i]&0x1f;
			//printf("found NALU type = %d\n",pnalu->type);
			//printf("pnalu->type = %d\n",pnalu->type);
            pnalu->data =(unsigned char*)&buffer[i];  
            return (pnalu->size+i-offSet);  //这里的i-offset就是4啊
        }  
    }  
    return 0;  
}  
  
void CloseMP4File(MP4FileHandle hMp4File)  
{  
    if(hMp4File)  
    {  
        MP4Close(hMp4File,0);  
        hMp4File = NULL;  
    }  
}  
  
bool WriteH264File(const char* pFile264,const char* pFileMp4)  
{  
    int i;
	//printf("call WriteH264File()\n");
    
    if(pFile264 == NULL || pFileMp4 == NULL)  
    {  
        return false;  
    }  
  
    MP4FileHandle hMp4File = CreateMP4File(pFileMp4,1280,720,90000,25);  
  
    if(hMp4File == NULL)  
    {  
        //printf("ERROR:Create file failed!");  
        return false;  
    }  
  
    FILE *fp = fopen(pFile264, "rb");    
    if(!fp)    
    {    
        //printf("ERROR:open file failed!");  
        return false;  
    }    
    fseek(fp, 0, SEEK_SET);  
  
    unsigned char buffer[BUFFER_SIZE];  
    int pos = 0;  
    while(1)  
    {  
        int readlen = fread(buffer+pos, sizeof(unsigned char), BUFFER_SIZE-pos, fp);  
  
  		//printf("readlen=%d\n",readlen);
        if(readlen<=0)  
        {  
            break;  
        }  
  
        readlen += pos;  
  
        int writelen = 0;  
        for(i = readlen-1; i>=0; i--)  
        {  
                if(buffer[i--] == 0x01 &&  
                    buffer[i--] == 0x00 &&  
                    buffer[i--] == 0x00 &&  
                    buffer[i--] == 0x00  
                    )  
                {  
                    //writelen = ++i+4;  
                    writelen = ++i; 
                    break;  
                }  
        }  
          
        writelen = WriteH264Data(hMp4File,buffer,writelen);  
        if(writelen<=0)  
        {  
            break;  
        }  
        memcpy(buffer,buffer+writelen,readlen-writelen+1);  
        pos = readlen-writelen+1;
    }
    long ftel,j;
    ftel=ftell(fp);
    for(j=0L;j<ftel;j++)
    {
        //printf("j=%ld\n",j);
        fseek(fp, -j, SEEK_END); 
        if(getc(fp)==0x01)
        {
            //printf("getc1\n");
            //getchar();
            fseek(fp, -j-1, SEEK_END); 
            if(getc(fp)==0x00)
            {
                //printf("getc2\n");
                //getchar();
                fseek(fp, -j-2, SEEK_END);
                if(getc(fp)==0x00)
                {
                    //printf("getc3\n");
                    //getchar();
                    fseek(fp, -j-3, SEEK_END);
                    if(getc(fp)==0x00)
                    {
                        //printf("getc4\n");
                        //getchar();
                        fseek(fp, -j-4, SEEK_END);
                        int readlen = fread(buffer, sizeof(unsigned char), j+4, fp);  
  		                //printf("readlen=%d\n",readlen);
                        //printf("buffer0=%d\n",(int)*buffer);
                        //printf("buffer1=%d\n",(int)*(buffer+1));
                        //printf("buffer2=%d\n",(int)*(buffer+2));
                        //printf("buffer3=%d\n",(int)*(buffer+3));
                        //printf("buffer4=%d\n",(int)*(buffer+4));
                        //printf("buffer5=%d\n",(int)*(buffer+5));
                        //printf("buffer6=%d\n",(int)*(buffer+6));
                        //printf("buffer7=%d\n",(int)*(buffer+7));
                        //printf("buffer8=%d\n",(int)*(buffer+8));
                        //printf("buffer9=%d\n",(int)*(buffer+9));
                        int writelen = WriteH264Data(hMp4File,buffer,j+4); 
                        //printf("writelen=%d\n",writelen);
                        break;
                    }
                    else
                    {
                        //printf("ctn3\n");
                        continue;
                    }
                }
                else
                {
                    //printf("ctn2\n");
                    continue;
                }
            }
            else
            {
                //printf("ctn1\n");
                continue;
            }
        }
    }
    
    
    
    fclose(fp);  
  
    CloseMP4File(hMp4File);  
  
    return true;  
}  
  
  
