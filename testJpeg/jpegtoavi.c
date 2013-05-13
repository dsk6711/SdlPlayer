/*
  jpegtoavi

  A simple converter of JPEGs to an AVI/MJPEG animation.

  Copyright (C) 2003 Phillip Bruce <dI77IHd@yahoo.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _LARGEFILE_SOURCE 
#define _LARGEFILE_SOURCE
#endif

#define _LARGEFILE64_SOURCE

#define VERSION_MAJ 1
#define VERSION_MIN 0

#include "byteswap.h"
#include <stdio.h>
#include "avifmt.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "llist.h"


/*
  containing name of jpeg file, size in
  bytes, and offset in pending avi file;

  we create a list of these for later
  production of the jpeg data and indices
  in the avi
*/

typedef struct _Jpeg_Data Jpeg_Data;

struct _Jpeg_Data
{
  DWORD size;
  DWORD offset;
  char name[0]; /* i.e. variable length structure */
};


#define JPEG_DATA_SZ (sizeof(DWORD) * 2)


/*
  spc: indicating file sz in bytes, -1 on error
*/

off_t file_sz(char *fn)
{
  struct stat s;
  if (stat(fn, &s) == -1)
    return -1;
  return s.st_size;
}


/*
  spc: returning sum of sizes of named JPEGs, -1 on error;
       file sizes adjusted to multiple of 4-bytes
  pos: l->size fields set to true file size
*/

off_t get_file_sz(List *l)
{
  off_t tmp, ret = 0;

  for(; (l); l = l->next) {
#if VERBOSE >= 2
    printf("determining file size for file == %s\n", 
	   ((Jpeg_Data *) l->data)->name);
#endif

    if((tmp = file_sz(((Jpeg_Data *)l->data)->name)) == -1)
      return -1;
    ((Jpeg_Data *)l->data)->size = (DWORD) tmp;
    tmp += ((4-(tmp%4)) % 4);
    ret += tmp;
  }

  return ret;
}


/* 
   spc: for obtaining list of file names from STDIN 
*/

List *get_file_list_stdin(void)
{
  char fn[PATH_MAX];
  Jpeg_Data *tmp;
  List *ret = (List *) malloc(sizeof(List)),
       *l = ret;

  ret->data = 0;
  ret->prev = 0;
  ret->next = 0;

  while (scanf("%s", fn) == 1) {
#if VERBOSE >= 2
    printf("read file name %s on stdin\n", fn);
#endif

    tmp = (Jpeg_Data *) malloc(strlen(fn) + 1 + JPEG_DATA_SZ);
    tmp->offset = 0;
    tmp->size = 0;
    strcpy(tmp->name, fn);
    if (l->data == 0)
      l->data = tmp;
    else
      l = list_push_back(l, tmp);
  }

#if VERBOSE >= 2
  printf("list size == %d\n", list_size(ret));
#endif

  return ret;
}


/*
  spc: for obtaining list of file names from argv
*/

List *get_file_list_argv(int argc, char **argv)
{
  List *ret = (List *) malloc(sizeof(List)),
       *l = ret;
  Jpeg_Data *tmp;
  int i;

  ret->data = 0;
  ret->prev = 0;
  ret->next = 0;

  for (i = 0; i < argc; ++i) {
    tmp = (Jpeg_Data *) malloc(strlen(argv[i]) + 1 + JPEG_DATA_SZ);
    strcpy(tmp->name, argv[i]);
    if (l->data == 0)
      l->data = tmp;
    else
      l = list_push_back(l, tmp);
  }

  return ret;
}


/*
  spc: printing 4 byte word in little-endian fmt
*/

void print_quartet(unsigned int i)
{
  putchar(i % 0x100);  i /= 0x100;
  putchar(i % 0x100);  i /= 0x100;
  putchar(i % 0x100);  i /= 0x100;
  putchar(i % 0x100);
}


void show_help(int argc, char **argv)
{
  fprintf(stderr, "jpegtoavi v%d.%d\n\
Copyright (C) 2003 Phillip Bruce\n\
\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n\
USAGE: jpegtoavi {usec per img} {img width} {img height} {img1 .. imgN}\n\
       jpegtoavi -f {fps} {img width} {img height} {img1 .. imgN}\n\
       jpegtoavi --fsz {img1 .. imgN}\n\n\
Creating an AVI/MJPEG on stdout.\n", VERSION_MAJ, VERSION_MIN);
}

#define RT_WRITE 1

#if RT_WRITE
static FILE* _fileStream;
#else
static char*        _aviDataBuffer;
static unsigned int _aviDataBufferLength;
static unsigned int _aviDataLength;
#endif
static char         _aviFilePath[256];
static List*        _aviFrameInfoList = NULL;
static DWORD        _aviWindowWidth;
static DWORD        _aviWindowHeight;
static DWORD        _aviFps;
static char*        _bufferSmallJpg;
static int          _lengthSmallJpg;



static off64_t getInitialMovieDataSize(DWORD frames, 
                                 DWORD width,
                                 DWORD height)
{
    off64_t tmp = 0;
    off64_t ret = 0;

    tmp = width*height*3;
    tmp += ((4-(tmp%4)) % 4);
    ret += tmp*frames;

    return ret;
}

static off_t getRealMovieDataSize()
{
    List* f = NULL;
    off_t total = 0;

    if(NULL == _aviFrameInfoList)
    {
        return 0;
    }    

    for (f = _aviFrameInfoList; (f); f = f->next) 
    {
        if(f->data)
        {
        #ifdef PRINT_DBG
            printf("id[%s] size[%d]offset[%d]\n",
            ((Jpeg_Data *) f->data)->name, 
            ((Jpeg_Data *) f->data)->size, 
            ((Jpeg_Data *) f->data)->offset);
        #endif

            total += ((Jpeg_Data *) f->data)->size;
            total += ((4-(total%4)) % 4);
        }
        else
        {
            printf("data NULL pointer at this node.\n");
        }
    }

    return total;
}

static int flushDataToBuffer(void* srcData, size_t size, void* dstBuffer)
{
    size_t copied = 0;
    
    if(srcData && dstBuffer)
    {
        if(NULL != memcpy(dstBuffer, srcData, size))
        {
            copied = size;
        }
    }

    return copied;
}

static int flushBufferToFile(char* path, void* buffer, size_t size)
{
    FILE* stream = fopen(path, "wb+");
    size_t countWritten = 0;
    if(stream)
    {
        if(buffer)
        {
            //count of items written.
            countWritten = fwrite(buffer, size, 1, stream);        
        }

        fclose(stream);
    }

    return countWritten*size;
}

static void aviGlobalBufferAlloc(unsigned int size)
{
#if !RT_WRITE
    _aviDataBuffer = malloc(size);
    _aviDataBufferLength = size;
    _aviDataLength = 0;
#endif
}

static void aviGlobalBufferFree()
{
#if !RT_WRITE
    free(_aviDataBuffer);
    _aviDataBuffer = NULL;
    _aviDataBufferLength = 0;
    _aviDataLength = 0;
#endif

    list_rerased(_aviFrameInfoList);
    _aviFrameInfoList = NULL;
}

static void aviGlobalBufferDataAdd(void* data, size_t size)
{
#if RT_WRITE
    if(_fileStream)
    {
        //return the count of items written NOT the bytes written.
        //size_t writtenCount = 
        fwrite(data, size, 1, _fileStream);        
    }
#else

    if(_aviDataBuffer && data && (size > 0))
    {
        if((_aviDataLength - _aviDataLength) >= size)
        {
            memcpy((_aviDataBuffer + _aviDataLength), data, size);

            _aviDataLength += size;
        }
    }
    else
    {
        printf("[Error] aviGlobalBufferDataAdd(): Nothing copied.\n");
    }
#endif
}

static void aviGlobalBufferDataFlush(void* path)
{
#if RT_WRITE
    if(_fileStream)
    {
        fclose(_fileStream);
    }   
#else
    if(_aviDataBuffer && path && (_aviDataLength > 0))
    {
        if(flushBufferToFile(path, _aviDataBuffer, _aviDataLength)> 0)
        {
            memset(_aviDataBuffer, 0, _aviDataBufferLength);

            _aviDataLength = 0;
        }
    }
#endif
}

static void printFrameInfoList()
{
#ifdef PRINT_DBG
    List* f = NULL;

    printf("there are [%d] nodes in the list.\n", list_size(_aviFrameInfoList));
    
    for (f = _aviFrameInfoList; (f); f = f->next) 
    {
        if(f->data)
        {
            printf("id[%s] size[%d]offset[%d]\n",
                   ((Jpeg_Data *) f->data)->name, 
                   ((Jpeg_Data *) f->data)->size, 
                   ((Jpeg_Data *) f->data)->offset);
        }
        else
        {
            printf("data NULL pointer at this node.\n");
        }
   }
#endif
}


static void addFrameInfo(List* infoList, int offset, int frameSize, int id)
{
    char frameId[PATH_MAX];
    Jpeg_Data *tmp = NULL;
    List *l = infoList;

    sprintf(frameId, "%d", id);

    tmp = (Jpeg_Data *) malloc(strlen(frameId) + 1 + JPEG_DATA_SZ);
    tmp->offset = offset;
    tmp->size = frameSize;
    strcpy(tmp->name, frameId);
    if (l->data == 0)
      l->data = tmp;
    else
      l = list_push_back(l, tmp);
    
#ifdef PRINT_DBG
    printf("list size == %d\n", list_size(infoList));
#endif
}

static void shapeTheHeader(DWORD width,
                         DWORD height,
                         DWORD per_usec, 
                         DWORD frames, 
                         long jpg_sz, 
                         DWORD riff_sz)
{
    DWORD temp = 0;
    
    struct AVI_list_hdrl hdrl = {
    /* header */
    {
        {'L', 'I', 'S', 'T'},
        LILEND4(sizeof(struct AVI_list_hdrl) - 8),
        {'h', 'd', 'r', 'l'}
    },

    /* chunk avih */
    {'a', 'v', 'i', 'h'},
    LILEND4(sizeof(struct AVI_avih)),
    {
        LILEND4(per_usec),
        LILEND4(1000000 * (jpg_sz/frames) / per_usec),
        LILEND4(0),
        LILEND4(AVIF_HASINDEX|AVIF_TRUSTCKTYPE),
        LILEND4(frames),
        LILEND4(0),
        LILEND4(1),
        LILEND4(0),
        LILEND4(width),
        LILEND4(height),
        {LILEND4(0), LILEND4(0), LILEND4(0), LILEND4(0)}
    },

    /* list strl */
    {
        {
            {'L', 'I', 'S', 'T'},
            LILEND4(sizeof(struct AVI_list_strl) - 8),
            {'s', 't', 'r', 'l'}
        },

        /* chunk strh */
        {'s', 't', 'r', 'h'},
        LILEND4(sizeof(struct AVI_strh)),
        {
            {'v', 'i', 'd', 's'},
            {'M', 'J', 'P', 'G'},
            LILEND4(0),
            LILEND4(0),
            LILEND4(0),
            LILEND4(1),
            LILEND4(1000000/per_usec),
            /*LILEND4(per_usec),
            LILEND4(1000000),*/
            LILEND4(0),
            LILEND4(frames),
            LILEND4(0),
            LILEND4(0),
            LILEND4(0)
        },

        /* chunk strf */
        {'s', 't', 'r', 'f'},
        sizeof(struct AVI_strf),
        {      
            LILEND4(sizeof(struct AVI_strf)),
            LILEND4(width),
            LILEND4(height),
            LILEND4(1 + 24*256*256),
            {'M', 'J', 'P', 'G'},
            LILEND4(width * height * 3),
            LILEND4(0),
            LILEND4(0),
            LILEND4(0),
            LILEND4(0)
        },

        /* list odml */
        {
            {
                {'L', 'I', 'S', 'T'},
                LILEND4(16),
                {'o', 'd', 'm', 'l'}
            },
            {'d', 'm', 'l', 'h'},
            LILEND4(4),
            LILEND4(frames)
        }
    }
    };

#if 0
    printf("* riff_sz = %u, jpg_sz = %ld, frames = %u *\n", riff_sz, jpg_sz, frames);
#endif

    aviGlobalBufferDataAdd("RIFF", strlen("RIFF")*sizeof(char));
    aviGlobalBufferDataAdd(&riff_sz, 4);
    aviGlobalBufferDataAdd("AVI ", strlen("AVI ")*sizeof(char));

    /* list hdrl */
    hdrl.avih.us_per_frame = LILEND4(per_usec);
    hdrl.avih.max_bytes_per_sec = LILEND4(1000000 * (jpg_sz/frames)
                      / per_usec);
    hdrl.avih.tot_frames = LILEND4(frames);
    hdrl.avih.width = LILEND4(width);
    hdrl.avih.height = LILEND4(height);
    hdrl.strl.strh.scale = LILEND4(per_usec);
    hdrl.strl.strh.rate = LILEND4(1000000);
    hdrl.strl.strh.length = LILEND4(frames);
    hdrl.strl.strf.width = LILEND4(width);
    hdrl.strl.strf.height = LILEND4(height);
    hdrl.strl.strf.image_sz = LILEND4(width * height * 3);
    hdrl.strl.list_odml.frames = LILEND4(frames); /*  */
    aviGlobalBufferDataAdd(&hdrl, sizeof(hdrl));

    /* list movi */
    aviGlobalBufferDataAdd("LIST", strlen("LIST")*sizeof(char));
    temp = jpg_sz + 8*frames + 4;
    aviGlobalBufferDataAdd(&temp, 4);
    aviGlobalBufferDataAdd("movi", strlen("movi")*sizeof(char));

    //End
}


static void initializeAviStack(DWORD width,
                         DWORD height,
                         DWORD per_usec, 
                         DWORD frames, 
                         long jpg_sz, 
                         DWORD riff_sz)
{
    shapeTheHeader(width,
                     height,
                     per_usec, 
                     frames, 
                     jpg_sz, 
                     riff_sz);
    

    _aviFrameInfoList = (List *) malloc(sizeof(List));

    if(_aviFrameInfoList)
    {
        _aviFrameInfoList->data = 0;
        _aviFrameInfoList->prev = 0;
        _aviFrameInfoList->next = 0;
    }

}

int createAviStack(char* path,
                       DWORD width,
                       DWORD height,
                       DWORD fps,
                       DWORD initFrames)
{
    DWORD per_usec = 1;
    DWORD frames = 1;
    off64_t jpg_sz_64 = 0;
    off64_t riff_sz_64 = 0;
    long jpg_sz = 1;
    const off64_t MAX_RIFF_SZ = 2147483648LL; /* 2 GB limit */
    DWORD riff_sz = 0;

    if(fps <= 0)
        return -1;

    if(initFrames <= 0)
        return -1;

#ifdef PRINT_DBG
    printf("creating new avi stack:(%d x %d) fps%d, max frames %d at path [%s].\n",
            width, height, fps, initFrames, path);
#endif

    _aviWindowWidth = width;
    _aviWindowHeight = height;
    _aviFps = fps;

    per_usec = 1000000 / fps;

    frames = initFrames;

    strcpy(_aviFilePath, path);

    /* getting image, and hence, riff sizes */
    /*calculate the estimated buffer size according to the input parameters and
            return the size that is the multiple of 4 into jpg_sz_64*/
    jpg_sz_64 = getInitialMovieDataSize(frames, 
                                 width,
                                 height);  

    riff_sz_64 = sizeof(struct AVI_list_hdrl) + 4 + 4 + jpg_sz_64
    + 8*frames + 8 + 8 + 16*frames;

    if (riff_sz_64 >= MAX_RIFF_SZ) {
        fprintf(stderr,"RIFF would exceed 2 Gb limit\n");
        return -3;
    }

#if RT_WRITE
    _fileStream = fopen(path, "wb+");
    if(NULL == _fileStream)
    {
        printf("Error! cannot create file [%s].\n", path);
    }
#else
    aviGlobalBufferAlloc(riff_sz_64);
#endif

#if 0
        printf("* riff_sz_64 = %lld, jpg_sz_64 = %lld, frames = %lu *\n", riff_sz_64, jpg_sz_64, frames);
#endif


    jpg_sz = (long) jpg_sz_64;
    riff_sz = (DWORD) riff_sz_64; 

#if 0
        printf("* riff_sz = %ld, jpg_sz = %ld, frames = %lu *\n", riff_sz, jpg_sz, frames);
#endif

    initializeAviStack(width,
                         height,
                         per_usec, 
                         frames, 
                         jpg_sz, 
                         riff_sz);

    return 0;
}

int* readInts(FILE* file, int count)
{
    if(NULL == file || count < 1)
    {
        return 0;
    }

    int* data = malloc(count*sizeof(int));

    size_t countRead = fread((char*)data, count*sizeof(int), 1, file);

    if(countRead != 1)
    {
        free(data);

        return NULL;
    }

    return data;    
}

static char* FromFourCC(int FourCC)
{
    char* chars = malloc(4);
    chars[0] = (char)(FourCC & 0xFF);
    chars[1] = (char)((FourCC >> 8) & 0xFF);
    chars[2] = (char)((FourCC >> 16) & 0xFF);
    chars[3] = (char)((FourCC >> 24) & 0xFF);

    return (chars);
}

/*check if the file is in RIFF format for AVI.
"RIFF" datasize filetype filedata
datasize = sizeof(filetype + filedata)
*/
int isRiffAviHeader(FILE* file, int fileSize, int* riffDataSize)
{
    int ret = 0;
    if(NULL == file)
        return 0;

    /*restore the file pointer position by moving to the head*/
    fseek(file, 0L, SEEK_SET);

    char riffID[] = {'R', 'I', 'F', 'F'};
    char aviType[] = {'A', 'V', 'I', ' '};

    int* data = readInts(file, 3);

    if(0 != strncmp(riffID, (char*)data, sizeof(riffID)))
    {
       printf("This file is NOT in RIFF format.\n");

       ret = 0;

       goto cleanup_exit;
    }

    int riffSize = *(data + 1);

    if((fileSize - riffSize) != 8)
    {
        printf("RIFF size: %d bytes.BAD size.\n", riffSize);
        
        ret = -1;

        goto cleanup_exit;
    }

    int aviId = *(data + 2);
    
    if(0 != strncmp(aviType, (char*)&aviId, sizeof(aviType)))
    {
       printf("This RIFF file is NOT for AVI.[0x%x].\n", aviId);

       ret = -2;

       goto cleanup_exit;
    }

    if(NULL != riffDataSize)
    {
        *riffDataSize = riffSize;        
    }

    ret = 1;
    
cleanup_exit:
    
    if(data)
    {
        free(data);

        data = NULL;
    }

    return ret;
}

size_t getFileSize(FILE* file)
{
    if(NULL == file)
        return 0;
    
    size_t curpos = 0;
    size_t length = 0;

    curpos = ftell(file);

    fseek(file, 0L, SEEK_END);

    length = ftell(file);

    /*restore the file position*/
    fseek(file, curpos, SEEK_SET);

    return length;    
}

void skipData(FILE* file, int skipBytes)
{
    if(NULL == file || skipBytes <= 0)
        return;

    int length = ftell(file);
    int left = getFileSize(file) - length;

    if(skipBytes > left)
    {
        printf("Warning: skip %d bytes, while there only left %d bytes of data.\n", skipBytes, left);
        fseek(file, left, SEEK_CUR); 
    }
    else
    {    
        fseek(file, skipBytes, SEEK_CUR);
    }
}

// Process a RIFF list element (list sub elements)
static void processList(FILE* file, int fourCC, int length, int level)
{
    int bytesLeft = length;

    level ++;

    while(bytesLeft > 0)
    {
        bytesLeft = parseAviElements(file, bytesLeft, level);
    } 
}

// Process a RIFF chunk element (skip the data)
static void processChunk(FILE* file, int fourCC, int length, int paddedLength)
{     
    skipData(file, length);

   /*string type = RiffParser.FromFourCC(FourCC);
   Console.WriteLine("Found chunk element of type \"" + type + "\" and length " + length.ToString());

   // Skip data and update bytesleft
   rp.SkipData(paddedLength);*/
}

#define LENGTH_8BYTES    (8)


/*return bytes left(>=0), or return error code(<0)*/
int parseAviElements(FILE* file, int dataLength, int level)
{
    //printf("parseAviElements enter\n");

    if(NULL == file)
    {
        return -1;
    }

    int bytesLeft = dataLength;

    if (LENGTH_8BYTES > bytesLeft)
    {
        return -1;
    }
    
    char listElement[] = {'L', 'I', 'S', 'T'};
    //char chunkElement[] = {'A', 'V', 'I', ' '};

    int* data = readInts(file, 2);

    bytesLeft -= 2*sizeof(int);

    if(NULL == data)
    {
        return -2;
    }     

    int elementSize = *(data + 1);

    /* check if we have enough bytes to read*/
    if(bytesLeft < elementSize)
    {
        skipData(file, bytesLeft);

        char* elementName = FromFourCC(*data);

        printf("No enough data for element[%c%c%c%c]. Required Element Size %d bytes, but only %d bytes left.\n", 
               elementName[0], elementName[1], elementName[2], elementName[3],
               elementSize, bytesLeft);

        free(elementName);

        bytesLeft = 0;

        goto cleanup_exit;        
    }

    if(0 == strncmp(listElement, (char*)data, sizeof(listElement)))
    {
       /*We have a list, get its type/name*/
       int* listType = readInts(file, 1);

       if(listType)
       {
            char* elementName = FromFourCC(*listType);
            int numTabs = level;
            while(numTabs > 0)
            {
                printf("\t");
                numTabs --;
            }
            printf("list [%c%c%c%c][%d bytes].\n", 
               elementName[0], elementName[1], elementName[2], elementName[3],
               elementSize - 4);
            
            free(elementName);
            
            processList(file, *listType, elementSize - 4, level);
       }

       bytesLeft -= elementSize;       
    }
    else
    {
        /*We have a chunk*/
        // Calculated padded size - padded to WORD boundary
        int paddedSize = elementSize;
        if (0 != (elementSize & 1)) 
            ++ paddedSize;

        char* elementName = FromFourCC(*data);    
        int numTabs = level;
        while(numTabs > 0)
        {
            printf("\t");
            numTabs --;
        }
        printf("chunk [%c%c%c%c][%d bytes].\n", 
           elementName[0], elementName[1], elementName[2], elementName[3],
           elementSize);
        free(elementName);

        processChunk(file, *data, elementSize, paddedSize);

        // Adjust size
        bytesLeft -= paddedSize;
    }    
    
cleanup_exit:
    //printf("parseAviElements %d bytes left\n", bytesLeft);   
    
    if(data)
    {
        free(data);

        data = NULL;
    }

    return bytesLeft;    
}

int integrityCheck(char* filePath)
{
    FILE* file = fopen(filePath, "rb");

    int ret = -1;
    
    if(NULL == file)
    {
        printf("[%s] does NOT exist.\n", filePath);
        return -1;/*file not exist*/
    }

    char buff[512];

	size_t fileLength = getFileSize(file);

    int riffSize = 0;

    if(isRiffAviHeader(file, fileLength, &riffSize) < 1)
    {
        printf("Wrong RIFF file header.\n");

        goto cleanup_exit;
    }

    int bytesLeft = riffSize - 4;

    printf("parsing AVI elements [bytes Left=%d][file size = %d].\n\n", bytesLeft, fileLength);

    while(bytesLeft > 0)
    {
        bytesLeft = parseAviElements(file, bytesLeft, 0);//root
    }

    printf("\nRIFF AVI parser done[bytesLeft=%d].\n", bytesLeft);

cleanup_exit:


    if(file)
    {
        fclose(file);

        file = NULL;
    }

    return ret;
}


struct RectOffset{
    int leftOffset;
    int topOffset;
    int rightOffset;
    int bottomOffset;
    
};

struct AVI_pwMovh 
{
  unsigned char id[4];   /* "PWMV" */
  DWORD sz;              /* size of owning struct minus 8 */
  unsigned char type[4]; /* type of PW movi : jpeg*/
  DWORD buff_sz;
  DWORD buff_offset;
  struct RectOffset rcDisplayOffset;/*related to the (0,0)width and height of AVI_avih or left/top of AVI_strh if supported*/
};

int loadSmallImage()
{
    FILE* file = fopen("/home/chlinpvgsoft/samples/dog.jpg", "rb");

    int ret = -1;
    
    if(NULL == file)
    {
        printf("Failed to open file, dog.jpg.\n");
        return -1;/*file not exist*/
    }	

    struct AVI_pwMovh pwMovHeader = {
        {'P', 'W', 'M', 'V'},
        LILEND4(sizeof(struct AVI_pwMovh) - 8),
        {'J', 'P', 'E', 'G'}  
    };

    int sizePwMovHeader = sizeof(struct AVI_pwMovh);

    sizePwMovHeader += ((4-(sizePwMovHeader%4)) % 4);

    size_t fileLength = getFileSize(file);

    size_t bufferSize = 0;

    bufferSize += sizePwMovHeader;

    bufferSize += fileLength;

    bufferSize += ((4-(fileLength%4)) % 4);

    pwMovHeader.buff_sz = bufferSize - sizePwMovHeader;

    pwMovHeader.buff_offset = sizePwMovHeader;

    pwMovHeader.rcDisplayOffset.leftOffset = 10;
    pwMovHeader.rcDisplayOffset.topOffset = 50;
    pwMovHeader.rcDisplayOffset.rightOffset = 0;
    pwMovHeader.rcDisplayOffset.bottomOffset = 0;    

    _bufferSmallJpg = malloc(bufferSize);

    fread(_bufferSmallJpg, fileLength, 1, file);

    _lengthSmallJpg = fileLength;


cleanup_exit:


    if(file)
    {
        fclose(file);

        file = NULL;
    }

    return ret;
}

void insertSmallImage()
{
    if(_bufferSmallJpg)
    {
        pushbackFrame(_bufferSmallJpg, _lengthSmallJpg, 20, 30);
    }
}


/* list movi */
int pushbackFrame(void* jpegFrameData, off_t mfsz, int leftOffset, int topOffset, int w, int h)
{
    off_t remnant = 0;
    DWORD temp = 0;
    long nbr, nbw, tnbw = 0;
    char* buff = NULL;
    int offset = 0;

    if(mfsz < 10){
     fprintf(stderr, "error\n");
     return -7;
    }

    if(NULL == _aviFrameInfoList){
     fprintf(stderr, "avi stack is not ready.\n");
     return -7;
    }
    
    if( (_aviWindowWidth != w) || (_aviWindowHeight != h) )
    {
        aviGlobalBufferDataAdd("pwjp", strlen("pwjp")*sizeof(char));
    }
    else
    {
        aviGlobalBufferDataAdd("00db", strlen("00db")*sizeof(char));
    }
    
    remnant = (4-(mfsz%4)) % 4;
    temp = mfsz + remnant;

    if( (_aviWindowWidth != w) || (_aviWindowHeight != h) )
    {
        temp += 8;
        aviGlobalBufferDataAdd(&temp, 4);
        aviGlobalBufferDataAdd(&leftOffset, 4);
        aviGlobalBufferDataAdd(&topOffset, 4);
    }
    else
    {
        aviGlobalBufferDataAdd(&temp, 4);
    }
    
    
    nbw = 0;  

    buff = jpegFrameData;

    //add the real image data
    nbr = mfsz;

    aviGlobalBufferDataAdd(buff, nbr);
 
    nbw += nbr;    
    
    if (remnant > 0) 
    {
        /*added the meaningless paddings*/
        aviGlobalBufferDataAdd(buff, remnant);

        nbw += remnant;
    }

    if( (_aviWindowWidth != w) || (_aviWindowHeight != h) )
    {
        nbw += 8;
    }
    
    tnbw += nbw;

#ifdef PRINT_DBG
    printf("saving a new frame info into the list:\n");
    printf("=========================================\n");

    printFrameInfoList();

    printf("=========================================\n");
#endif

    int id = 0;

    List* f = list_back(_aviFrameInfoList);

    if(NULL == _aviFrameInfoList->data)
    {
#ifdef PRINT_DBG
        printf("list_back of _aviFrameInfoList == _aviFrameInfoList: _aviFrameInfoList->data == NULL.\n");
#endif
        id = 0;
        
        offset = 4;
    }
    else
    {
        if(f)
        {
            if((Jpeg_Data *) f->data)
            {
                offset = ((Jpeg_Data *) f->data)->offset +
                         ((Jpeg_Data *) f->data)->size + 8;

                id = list_size(_aviFrameInfoList);
            }
            else
            {
            #ifdef PRINT_DBG
                printf("data is NULL pointer in current node.\n");
            #endif
            }
        }
    }  

#ifdef PRINT_DBG
    printf("adding offset[%d], size[%d], id[%d]\n", offset, nbw, id);
#endif

    addFrameInfo(_aviFrameInfoList, offset, nbw, id);  

    return 0;
}

void aviRetouchHeaderInfo()
{
#if RT_WRITE
    if(NULL == _fileStream)
        return;
    
    //move to the file head position
    rewind(_fileStream);

    DWORD per_usec = 1;
    DWORD frames = 1;
    off64_t jpg_sz_64 = 0;
    off64_t riff_sz_64 = 0;
    long jpg_sz = 1;
    const off64_t MAX_RIFF_SZ = 2147483648LL; /* 2 GB limit */
    DWORD riff_sz = 0;

    per_usec = 1000000 / _aviFps;

    frames = list_size(_aviFrameInfoList);


    jpg_sz_64 = getRealMovieDataSize();  

    riff_sz_64 = 4 + sizeof(struct AVI_list_hdrl) /*sizeof("AVI") + sizeof(hdrl list)*/
        + (8 + 4 + 8*frames + jpg_sz_64) /*sizeof(List)+sizeof(list length)+sizeof(movi)+sizeof(00db + length)*jpeg count+jpeg total size*/
        + (8 + 16*frames); /*idxl+ length + count * size of(00db + length)*/
    
    if (riff_sz_64 >= MAX_RIFF_SZ) {
        fprintf(stderr,"RIFF would exceed 2 Gb limit\n");
        return;
    }

    jpg_sz = (long) jpg_sz_64;
    riff_sz = (DWORD) riff_sz_64;

    shapeTheHeader(_aviWindowWidth,
                     _aviWindowHeight,
                     per_usec, 
                     frames, 
                     jpg_sz, 
                     riff_sz);
#else
//do sth.
#endif
    
}

#define CHECK_FILE_INTEGRITY 0

void flushAviStack()
{
#ifdef PRINT_DBG
    printf("flush all data for file[%s].\n", _aviFilePath);
#endif

 /*   if (tnbw != jpg_sz) {
    fprintf(stderr, "error writing images (wrote %ld bytes, expected %ld bytes)\n",
	    tnbw, jpg_sz);
	
    return -8;
  }*/

 
    /* indices */
    DWORD frames = list_size(_aviFrameInfoList);
    if(frames > 0)
    {
        aviGlobalBufferDataAdd("idx1", strlen("idx1")*sizeof(char));
        DWORD temp = 16 * frames;
        aviGlobalBufferDataAdd(&temp, 4);

        List* f = NULL;

        for (f = _aviFrameInfoList; (f); f = f->next) 
        {
            aviGlobalBufferDataAdd("00db", strlen("00db")*sizeof(char));
            temp = 18;
            aviGlobalBufferDataAdd(&temp, 4);
            temp = ((Jpeg_Data *) f->data)->offset;
            aviGlobalBufferDataAdd(&temp, 4);    
            temp = ((Jpeg_Data *) f->data)->size;
            aviGlobalBufferDataAdd(&temp, 4);
        }
    }

    aviRetouchHeaderInfo();

    aviGlobalBufferDataFlush(_aviFilePath);

#if CHECK_FILE_INTEGRITY
    //printf("BEF integrityCheck\n");
    integrityCheck(_aviFilePath);
    //printf("AFT integrityCheck\n");
#endif
}

void destroyAviStack()
{
#ifdef PRINT_DBG
    printf("destroy old avi stack.\n");
#endif

    aviGlobalBufferFree();
}

int jpeg2avi_main(int argc, char **argv)
{
  DWORD per_usec = 1;
  DWORD width;
  DWORD height;
  DWORD frames = 1;
  unsigned int fps;
  unsigned short img0;
  off64_t jpg_sz_64, riff_sz_64;
  long jpg_sz = 1;
  const off64_t MAX_RIFF_SZ = 2147483648LL; /* 2 GB limit */
  DWORD riff_sz;

  int fd;
  long nbr, nbw, tnbw = 0;
  char buff[512];
  off_t mfsz, remnant;
  List *frlst = 0;
  List *f = 0;

   FILE* stream = NULL;
   DWORD temp = 0;

  struct AVI_list_hdrl hdrl = {
    /* header */
    {
      {'L', 'I', 'S', 'T'},
      LILEND4(sizeof(struct AVI_list_hdrl) - 8),
      {'h', 'd', 'r', 'l'}
    },

    /* chunk avih */
    {'a', 'v', 'i', 'h'},
    LILEND4(sizeof(struct AVI_avih)),
    {
      LILEND4(per_usec),
      LILEND4(1000000 * (jpg_sz/frames) / per_usec),
      LILEND4(0),
      LILEND4(AVIF_HASINDEX),
      LILEND4(frames),
      LILEND4(0),
      LILEND4(1),
      LILEND4(0),
      LILEND4(width),
      LILEND4(height),
      {LILEND4(0), LILEND4(0), LILEND4(0), LILEND4(0)}
    },

    /* list strl */
    {
      {
	{'L', 'I', 'S', 'T'},
	LILEND4(sizeof(struct AVI_list_strl) - 8),
	{'s', 't', 'r', 'l'}
      },

      /* chunk strh */
      {'s', 't', 'r', 'h'},
      LILEND4(sizeof(struct AVI_strh)),
      {
	{'v', 'i', 'd', 's'},
	{'M', 'J', 'P', 'G'},
	LILEND4(0),
	LILEND4(0),
	LILEND4(0),
	LILEND4(per_usec),
	LILEND4(1000000),
	LILEND4(0),
	LILEND4(frames),
	LILEND4(0),
	LILEND4(0),
	LILEND4(0)
      },
      
      /* chunk strf */
      {'s', 't', 'r', 'f'},
      sizeof(struct AVI_strf),
      {      
	LILEND4(sizeof(struct AVI_strf)),
	LILEND4(width),
	LILEND4(height),
	LILEND4(1 + 24*256*256),
	{'M', 'J', 'P', 'G'},
	LILEND4(width * height * 3),
	LILEND4(0),
	LILEND4(0),
	LILEND4(0),
	LILEND4(0)
      },

      /* list odml */
      {
	{
	  {'L', 'I', 'S', 'T'},
	  LILEND4(16),
	  {'o', 'd', 'm', 'l'}
	},
	{'d', 'm', 'l', 'h'},
	LILEND4(4),
	LILEND4(frames)
      }
    }
  };

  /* parsing command line arguments */
  if (argc < 2) {
    show_help(argc, argv);
    return -1;

  } else if (argc == 2 && strcmp(argv[1], "--version") == 0) {
    printf("jpegtoavi v%d.%d\n", VERSION_MAJ, VERSION_MIN);
    return 0;

  } else if (argc < 3) {
    show_help(argc, argv);
    return -1;

  } else if (strcmp(argv[1], "-fsz") == 0) {
    img0 = 2;

  } 
  else if (strcmp(argv[1], "-f") == 0) 
  {
    if (argc < 5 || sscanf(argv[2], "%u", &fps) != 1
       || fps == 0
       || sscanf(argv[3], "%u", &width) != 1
       || sscanf(argv[4], "%u", &height) != 1) 
    {
      show_help(argc, argv);
      return -1;

    } 
    else 
    {
      per_usec = 1000000 / fps;
      img0 = 5;
    }

  } else if(argc < 4 || sscanf(argv[1], "%u", &per_usec) != 1 
     || sscanf(argv[2], "%u", &width) != 1
     || sscanf(argv[3], "%u", &height) != 1) {
    show_help(argc, argv);
    return -1;

  } else {
    img0 = 4;
  }

  if (img0 >= argc)
    frlst = get_file_list_stdin();
  else
    frlst = get_file_list_argv(argc - img0 , &argv[img0]);

  frames = list_size(frlst);

  /* getting image, and hence, riff sizes */
  jpg_sz_64 = get_file_sz(frlst);

  if (jpg_sz_64 == -1) {
    fprintf(stderr, "couldn't determine size of images\n");
    return -2;
  }

  riff_sz_64 = sizeof(struct AVI_list_hdrl) + 4 + 4 + jpg_sz_64
    + 8*frames + 8 + 8 + 16*frames;

  if (riff_sz_64 >= MAX_RIFF_SZ) {
    fprintf(stderr,"RIFF would exceed 2 Gb limit\n");
    return -3;
  }

  jpg_sz = (long) jpg_sz_64;
  riff_sz = (DWORD) riff_sz_64;

  /* printing predicted file size and quitting */
  if (img0 == 2) {
    printf("%lu\n", (unsigned long) riff_sz + 8UL);
    return 0;
  }

  stream = fopen("res.avi", "wb+");

  /* printing AVI riff hdr */
  printf("RIFF");
  fwrite("RIFF", strlen("RIFF")*sizeof(char), 1, stream);
  print_quartet(riff_sz);
  fwrite(&riff_sz, 4, 1, stream);
  printf("AVI ");
  fwrite("AVI ", strlen("AVI ")*sizeof(char), 1, stream);

  

  /* list hdrl */
  hdrl.avih.us_per_frame = LILEND4(per_usec);
  hdrl.avih.max_bytes_per_sec = LILEND4(1000000 * (jpg_sz/frames)
				      / per_usec);
  hdrl.avih.tot_frames = LILEND4(frames);
  hdrl.avih.width = LILEND4(width);
  hdrl.avih.height = LILEND4(height);
  hdrl.strl.strh.scale = LILEND4(per_usec);
  hdrl.strl.strh.rate = LILEND4(1000000);
  hdrl.strl.strh.length = LILEND4(frames);
  hdrl.strl.strf.width = LILEND4(width);
  hdrl.strl.strf.height = LILEND4(height);
  hdrl.strl.strf.image_sz = LILEND4(width * height * 3);
  hdrl.strl.list_odml.frames = LILEND4(frames);	/*  */
  fwrite(&hdrl, sizeof(hdrl), 1, stream);/*stdout);*/

  /* list movi */
  printf("LIST");
  fwrite("LIST", strlen("LIST")*sizeof(char), 1, stream);
  print_quartet(jpg_sz + 8*frames + 4);
  temp = 4 + 8*frames + jpg_sz;/*sizeof(movi)+sizeof(00db + length)*jpeg count+jpeg total size*/
  fwrite(&temp, 4, 1, stream);
  printf("movi");
  fwrite("movi", strlen("movi")*sizeof(char), 1, stream);

  /* list movi .. frames */
  for (f = frlst; (f); f = f->next) {
    printf("00db");
	fwrite("00db", strlen("00db")*sizeof(char), 1, stream);
    mfsz = ((Jpeg_Data *) f->data)->size;
    remnant = (4-(mfsz%4)) % 4;
    print_quartet(mfsz + remnant);
	temp = mfsz + remnant;
    fwrite(&temp, 4, 1, stream);
    ((Jpeg_Data *) f->data)->size += remnant;

    if (f == frlst) {
      ((Jpeg_Data *) f->data)->offset = 4;

    } else {
      ((Jpeg_Data *) f->data)->offset = 
	((Jpeg_Data *) f->prev->data)->offset +
	((Jpeg_Data *) f->prev->data)->size + 8;
    }

    if ((fd = open(((Jpeg_Data *) f->data)->name, O_RDONLY)) < 0) {
      fprintf(stderr, "couldn't open file!\n");
      list_rerased(frlst);

	  fclose(stream);
	  
      return -6;
    }
    nbw = 0;

    if ((nbr = read(fd, buff, 6)) != 6) {
      fprintf(stderr, "error\n");
      list_rerased(frlst);

  	  fclose(stream);
      return -7;
    }
    fwrite(buff, nbr, 1, stream);/*SOI(4)_App0Marker(2)_App0DataLength(2)_JFIF\0(5)_version(1+1)_density unit(1)_density(2)...*/
    read(fd, buff, 4);
    fwrite("AVI1", 4, 1, stream);/*stdout);*/
    nbw = 10;

    while ((nbr = read(fd, buff, 512)) > 0) {
      fwrite(buff, nbr, 1, stream);/*stdout);*/
      nbw += nbr;
    }
    if (remnant > 0) {
      fwrite(buff, remnant, 1, stream);/*stdout);*/
      nbw += remnant;
    }
    tnbw += nbw;
    close(fd);
  }

  if (tnbw != jpg_sz) {
    fprintf(stderr, "error writing images (wrote %ld bytes, expected %ld bytes)\n",
	    tnbw, jpg_sz);
    list_rerased(frlst);

	fclose(stream);
	
    return -8;
  }

  
  
  /* indices */
  printf("idx1");
  fwrite("idx1", strlen("idx1")*sizeof(char), 1, stream);
  print_quartet(16 * frames);
  temp = 16 * frames;
  fwrite(&temp, 4, 1, stream);
  for (f = frlst; (f); f = f->next) {
    printf("00db");
	fwrite("00db", strlen("00db")*sizeof(char), 1, stream);
    print_quartet(18);
	temp = 18;
    fwrite(&temp, 4, 1, stream);
    print_quartet(((Jpeg_Data *) f->data)->offset);
	temp = ((Jpeg_Data *) f->data)->offset;
    fwrite(&temp, 4, 1, stream);
    print_quartet(((Jpeg_Data *) f->data)->size);
	temp = ((Jpeg_Data *) f->data)->size;
    fwrite(&temp, 4, 1, stream);
  }

  close(fd);
  list_rerased(frlst);

  fclose(stream);
  
  return 0;
}
