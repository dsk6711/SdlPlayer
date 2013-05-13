
#ifndef _LARGEFILE_SOURCE 
#define _LARGEFILE_SOURCE
#endif

#define _LARGEFILE64_SOURCE

#define VERSION_MAJ 1
#define VERSION_MIN 0

#include "stdafx.h"
#include "WINDOWS.H"
#include <stdio.h>

#include "byteswap.h"
#include "avifmt.h"

#include "aviFileParse.h"


const char pwjpFOURCC[] = {'p', 'w', 'j', 'p'};

//#define DEBUG
#ifdef DEBUG
    #define AVI_PARSE_TRACE   printf
#else
    #define AVI_PARSE_TRACE 
#endif

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
    fseek(file, (long)curpos, SEEK_SET);

    return length;    
}

int* readInts(FILE* file, int count)
{
    if(NULL == file || count < 1)
    {
        return 0;
    }

    int* data = new int[count];

    size_t countRead = fread((char*)data, count*sizeof(int), 1, file);

    if(countRead != 1)
    {
        free(data);

        return NULL;
    }

    return data;    
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

    if(0 != FOURCC_CMP(riffID, data))
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

    if(0 != FOURCC_CMP(aviType, &aviId))
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

void skipData(FILE* file, int skipBytes)
{
    if(NULL == file || skipBytes <= 0)
        return;

    int length = ftell(file);
    int left = (int)getFileSize(file) - length;

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

int loadData(FILE* file, int loadBytes, void *pBuf)
{
    if(NULL == file || loadBytes <= 0)
        return -1;

    int length = ftell(file);
    int left = (int)getFileSize(file) - length;

    if(loadBytes > left)
    {
        return -1;
    }
    else
    {    
        fread(pBuf, loadBytes, 1, file);
    }
    return loadBytes;
}


static char* FromFourCC(int FourCC)
{
    char* chars = new char[4];
    chars[0] = (char)(FourCC & 0xFF);
    chars[1] = (char)((FourCC >> 8) & 0xFF);
    chars[2] = (char)((FourCC >> 16) & 0xFF);
    chars[3] = (char)((FourCC >> 24) & 0xFF);

    return (chars);
}

static void processList(FILE* file, int fourCC, int length, int level);
static void processChunk(FILE* file, int fourCC, int length, int paddedLength);


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

    if(0 == FOURCC_CMP(listElement, data))
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


//chech file integrity.
// Params:
//  filePath    : [in]  file path
int integrityCheck(char* filePath)
{
    FILE* file = fopen(filePath, "rb");

    int ret = -1;

    if(NULL == file)
    {
        printf("[%s] does NOT exist.\n", filePath);
        return -1;/*file not exist*/
    }

    size_t fileLength = getFileSize(file);

    int riffSize = 0;

    if(isRiffAviHeader(file, (int)fileLength, &riffSize) < 1)
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


static int _aviHeaderGet2(FILE* file, int dataLength, MainAVIHeader *pAviH)
{
    //printf("parseAviElements enter\n");
    int res;

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
    char hdrlFOURCC[] = {'h', 'd', 'r', 'l'};
    char avihFOURCC[] = {'a', 'v', 'i', 'h'};

    int* data;
    do 
    {
        data = readInts(file, 2);
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

            res = -3;
            goto cleanup_exit;        
        }

        if(0 == FOURCC_CMP(listElement, data))
        {
            /*We have a list, get its type/name*/
            int* listType = readInts(file, 1);

            if(listType)
            {
                char* elementName = FromFourCC(*listType);

                if(0 != FOURCC_CMP(hdrlFOURCC, listType))
                {
                    AVI_PARSE_TRACE("Skip list [%c%c%c%c][%d bytes].\n", 
                        elementName[0], elementName[1], elementName[2], elementName[3],
                        elementSize - 4);

                    skipData(file, elementSize - 4);
                }
                else
                {
                    AVI_PARSE_TRACE("Found hdrl list [%c%c%c%c][%d bytes].\n", 
                        elementName[0], elementName[1], elementName[2], elementName[3],
                        elementSize - 4);


                    int* avih = readInts(file, 2);

                    free(elementName);
                    elementName = FromFourCC(*avih);
                    int avihSize = *(avih + 1); 
                    int avihSz = sizeof (*pAviH);

                    if( (0 != FOURCC_CMP(avihFOURCC, avih)) || 
                        (avihSize != avihSz ) )
                    {
                        printf("Can't suppose avih behind 'hdrl' FOURCC.\n"); 

                    }

                    res = loadData(file, sizeof (*pAviH), pAviH);

                    //TODO

                }

                free(elementName);

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

            if(0 != FOURCC_CMP(avihFOURCC, data))
            {
                AVI_PARSE_TRACE("Skip chunk [%c%c%c%c][%d bytes].\n", 
                    elementName[0], elementName[1], elementName[2], elementName[3],
                    elementSize);
                skipData(file, paddedSize);
            }
            else
            {
                AVI_PARSE_TRACE("Found avih chunk [%c%c%c%c][%d bytes].\n", 
                    elementName[0], elementName[1], elementName[2], elementName[3],
                    elementSize);

                //TODO

            }

            free(elementName);


            // Adjust size
            bytesLeft -= paddedSize;
        }    
    }while (bytesLeft > 0);

cleanup_exit:
    //printf("parseAviElements %d bytes left\n", bytesLeft);   

    if(data)
    {
        free(data);

        data = NULL;
    }

    return bytesLeft;    
}

//Load avih data from file.
// Params:
//  filePath    : [in]  file path
//  pAviH       : [out] buffer to load avih data.
//
// Return:
//  created hdrl buffer size, return -1 if fail
int aviHeaderGet(char* filePath, MainAVIHeader *pAviH)
{
    FILE* file = fopen(filePath, "rb");

    int ret = -1;

    if(NULL == file)
    {
        printf("[%s] does NOT exist.\n", filePath);
        return -1;/*file not exist*/
    }

    size_t fileLength = getFileSize(file);

    int riffSize = 0;

    if(isRiffAviHeader(file, (int)fileLength, &riffSize) < 1)
    {
        printf("Wrong RIFF file header.\n");

        goto cleanup_exit;
    }

    int bytesLeft = riffSize - 4;


    printf("parsing AVI elements [bytes Left=%d][file size = %d].\n\n", bytesLeft, fileLength);

    ret = _aviHeaderGet2(file, bytesLeft, pAviH);    


cleanup_exit:


    if(file)
    {
        fclose(file);
        file = NULL;
    }

    return ret;
}


//Load hdrl data from file, & returned by *ppBuf.
// Params:
//  filePath    : [in]  file path
//  ppBuf       : [out] create buffer to load hdrl data.
//
// Return:
//  created hdrl buffer size, return -1 if fail
int aviHdrlBufferGet(char* filePath, BYTE** ppBuf)
{
    FILE* file = fopen(filePath, "rb");

    int ret = -1;
    int found = 0;

    if(NULL == file)
    {
        printf("[%s] does NOT exist.\n", filePath);
        return -1;/*file not exist*/
    }

    size_t fileLength = getFileSize(file);

    int riffSize = 0;

    if(isRiffAviHeader(file, (int)fileLength, &riffSize) < 1)
    {
        printf("Wrong RIFF file header.\n");
        goto cleanup_exit;
    }

    int bytesLeft = riffSize - 4;

    char listElement[] = {'L', 'I', 'S', 'T'};
    char hdrlFOURCC[] = {'h', 'd', 'r', 'l'};

    int* data;
    BYTE* pBuf = NULL;

    do 
    {
        data = readInts(file, 2);
        bytesLeft -= 2*sizeof(int);

        if(NULL == data)
        {
            ret = -2;
            goto cleanup_exit;
        }     

        int elementSize = *(data + 1);

        /* check if we have enough bytes to read*/
        if(bytesLeft < elementSize)
        {
            ret = -3;
            goto cleanup;        
        }

        if(0 == FOURCC_CMP(listElement, data))
        {
            //found a list it. Check it is hdrl
            int* listType = readInts(file, 1);
            bytesLeft -= 4;

            if(listType)
            {
                char* elementName = FromFourCC(*listType);

                if(0 != FOURCC_CMP(hdrlFOURCC, listType))
                {
                    printf("Skip list [%c%c%c%c][%d bytes].\n", 
                        elementName[0], elementName[1], elementName[2], elementName[3],
                        elementSize - 4);

                    skipData(file, elementSize - 4);
                    bytesLeft -= (elementSize - 4);

                }
                else
                {
                    printf("Found list [%c%c%c%c][%d bytes].\n", 
                        elementName[0], elementName[1], elementName[2], elementName[3],
                        elementSize - 4);

                    pBuf = new BYTE [elementSize - 4];

                    if(NULL == pBuf)
                    {
                        ret = -3;
                        goto cleanup;  
                    }

                    ret = loadData(file, elementSize - 4, pBuf);
                    found = 1;

                }

                free (elementName);
            }
        }
        else
        {
            skipData(file, elementSize);
            bytesLeft -= elementSize;
        }

    }while ( (bytesLeft > 0) && (!found) );


cleanup:

    if(data)
    {
        free(data);
        data = NULL;
    }

cleanup_exit:


    if(file)
    {
        fclose(file);
        file = NULL;
    }

    *ppBuf = pBuf;
    return ret;
}

//found position MainAVIHeader structure in hdrl
// Params:
//  pHdrl       : [in]  hdrl buffer for search
//  nSize       : [in]  hdrl buffer size
//  pnMainAviHSize  : [out] avih structure size
//
// Return:
//  the position MainAVIHeader structure in hdrl, return -1 if fail
int aviHeaderGetFromHdrl(BYTE *pHdrl, int nSize, DWORD *pnMainAviHSize)
{
    char *pHere;
    char *pEnd;
    int *pSize;
    char avihFOURCC[] = {'a', 'v', 'i', 'h'};

    int avihSz = sizeof (MainAVIHeader);
    int pos = -1;

    pHere = (char *)pHdrl;
    pEnd = pHere + nSize;

    //we suppose avih chunk is in the top level of hdrl list, it is not enclosed in another LIST
    do 
    {
        pSize = (int *)(pHere + 4);
        if( (0 == FOURCC_CMP(avihFOURCC, pHere)) )
        {
            if ( *pSize != avihSz )
            {
                printf("Found avih structure size abnormal: [%d bytes] while we expect [%d bytes].\n",*pSize, avihSz);
                return -2;
            }

            pos = (int)((BYTE*)pHere - pHdrl + 8);
            *pnMainAviHSize = *pSize;
        }

        pHere += (*pSize + LENGTH_8BYTES);   //need add length of FOURCC & size field

    }while ((pHere < pEnd) && (pos < 0));

    return pos;
}


// Params:
//  pHdrl              : [in]   hdrl buffer for search
//  nSize              : [in]   hdrl buffer size
//  strlDataSize    : [out]  strl pay load size
//
// Return:
//  the position of strl pay load, or -1 if fail
static int strlDataSearchInBuf(BYTE *pHdrl, int nSize, DWORD *strlDataSize)
{
    char listFOURCC[] = {'L', 'I', 'S', 'T'};
    char strlFOURCC[] = {'s', 't', 'r', 'l'};

    char *pHere;
    char *pEnd;
    int *pSize;
    char *pListData;
    int pos;

    AVI_list_hdr *pListHdr;

    pHere = (char *)pHdrl;
    pEnd = pHere + nSize;

    pos = -1;
    pListHdr = NULL;

    do 
    {
        //suppose strl LIST is in top level of hdrl
        pSize = (int *)(pHere + 4);
        if( 0 == FOURCC_CMP(pHere, listFOURCC) )
        {
            pListHdr = (AVI_list_hdr *)pHere;

            if( 0 == FOURCC_CMP(pListHdr->type, strlFOURCC) )
            {
                //found it.
                pListData = (char *)(pListHdr+1); 
                pos = (int)( (BYTE*)pListData - pHdrl );
            }
        }

        pHere += (*pSize + LENGTH_8BYTES);   //need add length of FOURCC & size field

    }while ((pHere < pEnd) && (pos < 0));

    if(pos > 0)
        *strlDataSize = pListHdr->sz;

    return pos;

}


//found the data of strh which fccType=pFccType ('vids', 'auds', 'txts')
// Params:
//  pStrlData       : [in]   strl buffer for search
//  nSize           : [in]   strl buffer size
//  pFccType        : [in]   strh pFccType which are look for. ('vids', 'auds', 'txts')
//  strhDataSize    : [out]  strh data size
//
// Return:
//  the position of strh pay load, or -1 if fail
static int strhDataSearchInStrl(BYTE *pStrlData, int nSize, char *pFccType, DWORD *strhDataSize)
{
    char strhFOURCC[] = {'s', 't', 'r', 'h'};

    struct AVI_chunk_hdr *pChunkHdr;

    struct AVI_strh *pAviStrh;

    char *pHere;
    char *pEnd;
    int pos;
    int *pSize;

    pHere = (char *)pStrlData;
    pEnd = pHere + nSize;

    pos = -1;
    pChunkHdr = NULL;
    pAviStrh = NULL;

    do 
    {
        //suppose strh chunk is in top level of strl
        pSize = (int *)(pHere + 4);
        if( 0 == FOURCC_CMP(pHere, strhFOURCC) )
        {
            pChunkHdr = (AVI_chunk_hdr *)pHere;
            //check sz =? structure AVIStreamHeader
            if (pChunkHdr->sz != sizeof (struct AVI_strh) )
            {
                printf("Warning: strh payload size %d bytes, while AVIStreamHeader structure size %d bytes.\n", 
                    pChunkHdr->sz, sizeof (struct AVI_strh) );
            }

            pAviStrh = (struct AVI_strh *)(pChunkHdr + 1);
            if ( 0 == FOURCC_CMP(pAviStrh->type, pFccType) )
            {
                //found it.
                printf("strh [%c%c%c%c] size %d bytes, handler [%c%c%c%c] .\n", 
                    pFccType[0], pFccType[1], pFccType[2], pFccType[3], pChunkHdr->sz, 
                    pAviStrh->handler[0], pAviStrh->handler[1], pAviStrh->handler[2], pAviStrh->handler[3] );

                pos = (int)( (BYTE*)pAviStrh - pStrlData);
            }

        }
        pHere += (*pSize + LENGTH_8BYTES);   //need add length of FOURCC & size field
    }while ((pHere < pEnd) && (pos < 0));

    if(pos > 0)
        *strhDataSize = pChunkHdr->sz;

    return pos;
}


// Params:
//  pHdrl           : [in]   hdrl buffer for search
//  nSize           : [in]   hdrl buffer size
//  nStreams        : [in]   streams number in this hdrl
//  pStrhFOURCC     : [in]   strh FOURCC which are look for
//  pnStrlNum       : [out] the number of stream which strh FOURCC = pStrhFOURCC  
//  pnStrhSize      : [out] the strh pay load size
//
// Return:
//  the position of strh pay load, or -1 if fail
static int strhSearchInHdrl(BYTE *pHdrl, int nSize, int nStreams, char *pStrhFOURCC, int *pnStrlNum, DWORD *pnStrhSize)
{
    char strhFOURCC[] = {'s', 't', 'r', 'h'};

    BYTE *pStart;
    int  nSearchLen;


    char *pAVI_strh;        //point to the start address of AVI_strh (stream type  = pStrhFOURCC)

    int nStrlNo = -1;

    int pos = -1;
    int strlDataPos = -1;


    int nStrl = 0;
    DWORD nStrlDataSize;
    DWORD nStrhDataSize;

    pAVI_strh = NULL;

    pStart = pHdrl;
    nSearchLen = nSize;

    //a. find each strl LIST data in hdrl
    //b. in each strl LIST data, search the strh who fccType = *pFOURCC, it could be 'auds', 'vids', 'txts', and so on
    for (nStrl=0; nStrl < nStreams; nStrl++)
    {
        strlDataPos = strlDataSearchInBuf(pStart, nSearchLen, &nStrlDataSize);
        if (strlDataPos < 0)
            break;

        pos = strhDataSearchInStrl(pStart + strlDataPos, nStrlDataSize, pStrhFOURCC, &nStrhDataSize);
        if (pos > 0)
        {
            //found it.
            pAVI_strh = (char *)pStart + strlDataPos + pos;
            break;
        }


        //next search start point: pStart+strlDataPos+ nStrlDataSize, 
        //next search len:           nSearchLen - (strlDataPos-nStrlDataSize)

        pStart          += strlDataPos+ nStrlDataSize;
        nSearchLen  -= strlDataPos+ nStrlDataSize;
    }

    if(pAVI_strh)
    {
        *pnStrhSize = nStrhDataSize;
        *pnStrlNum = nStrl;
        pos = (int)( (BYTE*)pAVI_strh - pHdrl);
    }

    return pos;    
}

// pHdrl -- [input] hdrl buffer
// Params:
//  pHdrl       : [in]   hdrl buffer for search
//  nSize       : [in]   hdrl buffer size
// nStreams     : [in]   how many streams in this hdrl
//  pnStrlNum   : [out]  the StrNo for vids stream   
// Return:
//  the position of AVI_strh (which stream type is 'vids') in hdrl buffer that 
int vidsStrhGetFromHdrl(BYTE *pHdrl, int nSize, int nStreams, int *pnStrlNum)
{
    char vidsFOURCC[] = {'v', 'i', 'd', 's'};
    //char vidsFOURCC[] = {'a', 'u', 'd', 's'};
    int pos = -1;
    DWORD nAviStrhSize;

    pos = strhSearchInHdrl(pHdrl, nSize, nStreams, vidsFOURCC, pnStrlNum,  &nAviStrhSize);

    return pos;
}


// Seek file pointer to the payload of LIST (type == pListTypeFOURCC)
// Params:
//  file            : [in]   
//  pListTypeFOURCC : [in]  the type of target LIST
//  pListDataSize   : [out] target LIST size
//
// Return:
//  the position of target LIST, or -1 if fail
int aviListFileSeek(FILE* file, char *pListTypeFOURCC, DWORD *pListDataSize)
{
    char listElement[] = {'L', 'I', 'S', 'T'};
    int riffSize;
    int pos;

    if(NULL == file)
    {
        return -1;/*file not exist*/
    }

    fseek(file, 0, SEEK_SET);
    size_t fileLength = getFileSize(file);

    if(isRiffAviHeader(file, (int)fileLength, &riffSize) < 1)
    {
        printf("Wrong RIFF file header.\n");
        return -1;
    }

    int bytesLeft = riffSize - 4;

    pos = -1;
    int* data;
    do 
    {
        data = readInts(file, 2);
        bytesLeft -= 2*sizeof(int);

        if(NULL == data)
        {
            pos = -2;
            goto cleanup;
        }     

        int elementSize = *(data + 1);

        /* check if we have enough bytes to read*/
        if(bytesLeft < elementSize)
        {
            pos = -3;
            goto cleanup;        
        }

        if(0 == FOURCC_CMP(listElement, data))
        {
            //found a list it. Check it is hdrl
            int* listType = readInts(file, 1);
            bytesLeft -= 4;

            if(listType)
            {
                char* elementName = FromFourCC(*listType);

                if(0 != FOURCC_CMP(elementName, pListTypeFOURCC) )
                {
                    printf("Skip list [%c%c%c%c][%d bytes].\n", 
                        elementName[0], elementName[1], elementName[2], elementName[3],
                        elementSize - 4);
                    
                    skipData(file, elementSize - 4);
                    bytesLeft -= (elementSize - 4);
                }
                else
                {
                    printf("Found list [%c%c%c%c][%d bytes].\n", 
                        elementName[0], elementName[1], elementName[2], elementName[3],
                        elementSize - 4);

                    *pListDataSize = elementSize - 4;

                    pos = ftell(file);
                }

                free (elementName);
            }
        }
        else
        {
            skipData(file, elementSize);
            bytesLeft -= elementSize;
        }

    }while ( (bytesLeft > 0) && (pos < 0) );


cleanup:

    if(data)
    {
        free(data);
        data = NULL;
    }

    return pos;
}

//load one chunk from file. (suppose file curpos is at payload of movi LIST)
// Params:
//  file              : [in]   
//  nMoviSize        : [in]   total movi LIST payload size
//  pChunkSize  : [out]   total chunk size
//  ppBuf       : [out] the chunk   
//
// Return:
//  the position of current file pointer, or -1 if fail
int aviMoviChunkLoad(FILE* file, DWORD nMoviSize, DWORD *pChunkSize, BYTE** ppBuf)
{
    char listElement[] = {'L', 'I', 'S', 'T'};
    int pos;

    if(NULL == file)
    {
        return -1;/*file not exist*/
    }

    int* data;
    BYTE *pChunk;
    int elementSize;

    pos = -1;
    int bytesLeft = nMoviSize;
    do 
    {
        data = readInts(file, 2);
        bytesLeft -= 2*sizeof(int);

        if(NULL == data)
        {
            pos = -2;
            goto cleanup;
        }     

        elementSize = *(data + 1);
        /* check if we have enough bytes to read*/
        if(bytesLeft < elementSize)
        {
            pos = -3;
            goto cleanup;        
        }

        if(0 == FOURCC_CMP(listElement, data))
        {
            //found a list, skip it.
            skipData(file, elementSize);
        }
        else
        {
            pChunk = new BYTE [elementSize + LENGTH_8BYTES];
            if (NULL == pChunk)
            {
                pos = -2;
                goto cleanup;
            }

            //found one chunk. read it & return pos/total chunk
            pos = ftell(file) - LENGTH_8BYTES;
            memcpy(pChunk, data,  2*sizeof(int));
            fread( (pChunk+LENGTH_8BYTES), elementSize, 1, file);
        }
        bytesLeft -= elementSize;

    } while ( (bytesLeft > 0) && (pos < 0));

    if(pos >= 0)
    {
        *pChunkSize = elementSize + LENGTH_8BYTES;
        *ppBuf = pChunk;
    }

cleanup:
    if(data)
    {
        free(data);
        data = NULL;
    }

    return pos;
}


//Found image data from one Movi chunk. return the pos of image.
// Params:
//  pMoviChunk  : [in]   Movi chunk
//  nMoviSize   : [in]   Movi chunk size
//  nStrNo      : [in]   vids stream no
//  pInfo       : [out]  the image information include left/top/size/type
//
// Return:
//  the position of image data, or -1 if fail
int aviMoviChunkParse(BYTE *pMoviChunk, DWORD nMoviChunkSize, int nStrNo, IMG_Info *pInfo)
{
    int pos = -1;
    int i;

    if (NULL == pInfo)
        return -1;

    char dcFOURCC[] = {'0', '0', 'd', 'c'};
    char dbFOURCC[] = {'0', '0', 'd', 'b'};

    //rewrite dcFOURCC & dbFOURCC to meet input nStrNo
    for (i=0; i<2; i++)
    {
        dcFOURCC[i] = dcFOURCC[i] + nStrNo;
        dbFOURCC[i] = dbFOURCC[i] + nStrNo;
    }

    struct AVI_chunk_hdr *pChunkHdr = (struct AVI_chunk_hdr *)pMoviChunk;

    if( 0 == FOURCC_CMP(pChunkHdr->chunk_id, dcFOURCC) )
    {
        pInfo->sLTPnt.nLeft = pInfo->sLTPnt.nTop = 0;
        pInfo->ImgType = eImgJpg;
        pInfo->nImgSize = pChunkHdr->sz;
        pos = sizeof AVI_chunk_hdr;
    }
    else if ( 0 == FOURCC_CMP(pChunkHdr->chunk_id, dbFOURCC) )
    {
        pInfo->sLTPnt.nLeft = pInfo->sLTPnt.nTop = 0;
        pInfo->ImgType = eImgRBG;
        pInfo->nImgSize = pChunkHdr->sz;
        pos = sizeof AVI_chunk_hdr;
    }
    else if ( 0 == FOURCC_CMP(pChunkHdr->chunk_id, pwjpFOURCC) )
    {
        PWDC_hdr *pLTPnt;
        pLTPnt = (PWDC_hdr *)(pChunkHdr + 1);

        pInfo->sLTPnt.nLeft = pLTPnt->nLeft ;
        pInfo->sLTPnt.nTop = pLTPnt->nTop;
        pInfo->ImgType = eImgJpg;
        pInfo->nImgSize = pChunkHdr->sz - sizeof(PWDC_hdr);

        pos = sizeof AVI_chunk_hdr + sizeof(PWDC_hdr);
    }

    return pos;
}
    

