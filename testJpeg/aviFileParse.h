
#ifndef _AVIFILEPARSE_H_
#define _AVIFILEPARSE_H_


#define FOURCC_LEN       (4)
#define LENGTH_8BYTES    (8)

#define FOURCC_CMP(a, b)    strncmp((char *)(a), (char *)(b), FOURCC_LEN)

typedef enum
{
    eImgJpg = 0,
    eImgRBG
} eImgType;

typedef struct
{
    PWDC_hdr sLTPnt;
    DWORD nImgSize;
    eImgType ImgType;
} IMG_Info;



//chech file integrity.
// Params:
//  filePath    : [in]  file path
int integrityCheck(char* filePath);


//Load hdrl data from file, & returned by *ppBuf.
// Params:
//  filePath    : [in]  file path
//  ppBuf       : [out] create buffer to load hdrl data.
//
// Return:
//  created hdrl buffer size, return -1 if fail
int aviHdrlBufferGet(char* filePath, BYTE** ppBuf);

//found position MainAVIHeader structure in hdrl
// Params:
//  pHdrl       : [in]  hdrl buffer for search
//  nSize       : [in]  hdrl buffer size
//  pnMainAviHSize  : [out] avih structure size
//
// Return:
//  the position MainAVIHeader structure in hdrl, return -1 if fail
int aviHeaderGetFromHdrl(BYTE *pHdrl, int nSize, DWORD *pnMainAviHSize);

// pHdrl -- [input] hdrl buffer
// Params:
//  pHdrl       : [in]   hdrl buffer for search
//  nSize       : [in]   hdrl buffer size
// nStreams     : [in]   how many streams in this hdrl
//  pnStrlNum   : [out]  the StrNo for vids stream   
// Return:
//  the position of AVI_strh (which stream type is 'vids') in hdrl buffer that 
int vidsStrhGetFromHdrl(BYTE *pHdrl, int nSize, int nStreams, int *pnStrlNum);

// Seek file pointer to the payload of LIST (type == pListTypeFOURCC)
// Params:
//  file            : [in]   
//  pListTypeFOURCC : [in]  the type of target LIST
//  pListDataSize   : [out] target LIST size
//
// Return:
//  the position of target LIST, or -1 if fail
int aviListFileSeek(FILE* file, char *pListTypeFOURCC, DWORD *pListDataSize);

//load one chunk from file. (suppose file curpos is at payload of movi LIST)
// Params:
//  file          : [in]   
//  nMoviSize     : [in]   total movi LIST payload size
//  pChunkSize  : [out]   total chunk size
//  ppBuf       : [out] the chunk   
//
// Return:
//  the position of current file pointer, or -1 if fail
int aviMoviChunkLoad(FILE* file, DWORD nMoviSize, DWORD *pChunkSize, BYTE** ppBuf);

//Found image data from one Movi chunk. return the pos of image.
// Params:
//  pMoviChunk  : [in]   Movi chunk
//  nMoviSize   : [in]   Movi chunk size
//  nStrNo      : [in]   vids stream no
//  pInfo       : [out]  the image information include left/top/size/type
//
// Return:
//  the position of image data, or -1 if fail
int aviMoviChunkParse(BYTE *pMoviChunk, DWORD nMoviChunkSize, int nStrNo, IMG_Info *pInfo);

#endif