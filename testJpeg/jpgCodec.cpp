
#include "stdafx.h"
#include "WINDOWS.H"
#include <iostream>

#include "ijl.h"

using namespace std;

//----------------------------------------------------------
// -- Decode a JPEG image from a JFIF file to general pixel buffer.
//----------------------------------------------------------
BOOL DecodeJPGFileToGeneralBuffer(char* JPGFile,
                                  DWORD* width,
                                  DWORD* height,
                                  DWORD* nchannels,
                                  DWORD* pitch,
                                  BYTE** buffer)
{
    BOOL bres;
    IJLERR jerr;

    DWORD x = 0; // pixels in scan line
    DWORD y = 0; // number of scan lines
    DWORD c = 0; // number of channels
    DWORD dwPitch = 0;

    DWORD wholeimagesize;
    BYTE* pixel_buf = NULL;

    cout << "Loading image file " << JPGFile << "\n";
    JPEG_CORE_PROPERTIES *p = new JPEG_CORE_PROPERTIES;

    bres = TRUE;
    try
    {
        // Initialize the IntelR JPEG Library.
        jerr = ijlInit( p );
        if(  jerr != IJL_OK )
            throw "Cannot initialize Intel JPEG library\n";

        p->JPGFile = (const char*)(JPGFile);

        // Get information on the JPEG image
        // (i.e., width, height, and channels).
        jerr = ijlRead( p, IJL_JFILE_READPARAMS );
        if( jerr != IJL_OK )
            throw "Cannot read JPEG file header\n";

        //IJL_RGB: R -> Byte 0, G -> Byte 1, B -> Byte 2
        //IJL_BGR: B -> Byte 0, G -> Byte 1, R -> Byte 2
        switch( p->JPGChannels )
        {
        case 1:  p->JPGColor    = IJL_G;
            p->DIBChannels = 3;
            p->DIBColor    = IJL_BGR;
            break;
        case 3:  p->JPGColor    = IJL_YCBCR;
            p->DIBChannels = 3;
            p->DIBColor    = IJL_BGR;
            break;
        case 4:  p->JPGColor    = IJL_YCBCRA_FPX;
            p->DIBChannels = 4;
            p->DIBColor    = IJL_RGBA_FPX;
            break;
        default:
            // This catches everything else, but no
            // color twist will be performed by the IJL.
            p->DIBColor = (IJL_COLOR)IJL_OTHER;
            p->JPGColor = (IJL_COLOR) IJL_OTHER;
            p->DIBChannels = p->JPGChannels;
            break;
        }

        p->DIBWidth    = p->JPGWidth;
        p->DIBHeight   = p->JPGHeight;
        p->DIBPadBytes = IJL_DIB_PAD_BYTES( p->DIBWidth, p->DIBChannels );

        dwPitch = p->DIBWidth * p->DIBChannels + p->DIBPadBytes;

        wholeimagesize = dwPitch * p->DIBHeight;

        // Allocate memory to hold the decompressed image data.
        pixel_buf = new BYTE [wholeimagesize];
        if(NULL == pixel_buf)
        {
            throw "Allocate memory to hold the decompressed image data\n";
        }

        p->DIBBytes = pixel_buf;

        // Now get the actual JPEG image data into the pixel buffer.
        jerr = ijlRead( p, IJL_JFILE_READWHOLEIMAGE );
        if ( jerr != IJL_OK )
            throw "Cannot read image data\n";
        if ( p->DIBColor == IJL_RGBA_FPX )
            throw "Conversion code needed here. Left out of the demo program\n";
    } 

    catch( const char *s )
    {
        cout << s;
        if ( p->DIBBytes )
            delete[] p->DIBBytes;
        ijlFree( p );
        delete p;
        return FALSE;
    }

    *width = p->DIBWidth;
    *height = p->DIBHeight;
    *nchannels = p->DIBChannels;
    *buffer = pixel_buf;
    *pitch = dwPitch;

    // Clean up the IntelR JPEG Library.
    ijlFree(p);

    return bres;
} // DecodeJPGFileToGeneralBuffer()


//----------------------------------------------------------
// -- Decode image from a JFIF buffer.
//----------------------------------------------------------
BOOL DecodeFromJPEGBuffer(BYTE* lpJpgBuffer,
                          DWORD dwJpgBufferSize,
                          DWORD* width,
                          DWORD* height,
                          DWORD* nchannels,
                          DWORD* pitch,
                          BYTE** buffer)
{
    BOOL bres;
    IJLERR jerr;

    DWORD x = 0; // pixels in scan line
    DWORD y = 0; // number of scan lines
    DWORD c = 0; // number of channels
    DWORD dwPitch = 0;

    DWORD wholeimagesize;
    BYTE* pixel_buf = NULL;

    //cout << "Decoding jpeg buffer: " << lpJpgBuffer << "size = " << dwJpgBufferSize << "\n";
    JPEG_CORE_PROPERTIES *p = new JPEG_CORE_PROPERTIES;

    bres = TRUE;
    try
    {
        // Initialize the IntelR JPEG Library.
        jerr = ijlInit( p );
        if(  jerr != IJL_OK )
            throw "Cannot initialize Intel JPEG library\n";

        p->JPGFile = NULL;
        p->JPGBytes = lpJpgBuffer;
        p->JPGSizeBytes = dwJpgBufferSize;

        // Get information on the JPEG image
        // (i.e., width, height, and channels).
        jerr = ijlRead( p, IJL_JBUFF_READPARAMS);
        if( jerr != IJL_OK )
            throw "Cannot read JPEG file header\n";

        //IJL_RGB: R -> Byte 0, G -> Byte 1, B -> Byte 2
        //IJL_BGR: B -> Byte 0, G -> Byte 1, R -> Byte 2
        switch( p->JPGChannels )
        {
        case 1:  p->JPGColor    = IJL_G;
            p->DIBChannels = 3;
            p->DIBColor    = IJL_BGR;
            break;
        case 3:  p->JPGColor    = IJL_YCBCR;
            p->DIBChannels = 3;
            p->DIBColor    = IJL_BGR;
            break;
        case 4:  p->JPGColor    = IJL_YCBCRA_FPX;
            p->DIBChannels = 4;
            p->DIBColor    = IJL_RGBA_FPX;
            break;
        default:
            // This catches everything else, but no
            // color twist will be performed by the IJL.
            p->DIBColor = (IJL_COLOR)IJL_OTHER;
            p->JPGColor = (IJL_COLOR) IJL_OTHER;
            p->DIBChannels = p->JPGChannels;
            break;
        }

        p->DIBWidth    = p->JPGWidth;
        p->DIBHeight   = p->JPGHeight;
        p->DIBPadBytes = IJL_DIB_PAD_BYTES( p->DIBWidth, p->DIBChannels );

        dwPitch = p->DIBWidth * p->DIBChannels + p->DIBPadBytes;

        wholeimagesize = dwPitch * p->DIBHeight;

        // Allocate memory to hold the decompressed image data.
        pixel_buf = new BYTE [wholeimagesize];
        if(NULL == pixel_buf)
        {
            throw "Allocate memory to hold the decompressed image data\n";
        }

        p->DIBBytes = pixel_buf;

        // Now get the actual JPEG image data into the pixel buffer.
        jerr = ijlRead( p, IJL_JBUFF_READWHOLEIMAGE );
        if ( jerr != IJL_OK )
            throw "Cannot read image data\n";
        if ( p->DIBColor == IJL_RGBA_FPX )
            throw "Conversion code needed here. Left out of the demo program\n";
    } 

    catch( const char *s )
    {
        cout << s;
        if ( p->DIBBytes )
            delete[] p->DIBBytes;
        ijlFree( p );
        delete p;
        return FALSE;
    }

    *width = p->DIBWidth;
    *height = p->DIBHeight;
    *nchannels = p->DIBChannels;
    *buffer = pixel_buf;
    *pitch = dwPitch;

    // Clean up the IntelR JPEG Library.
    ijlFree(p);

    return bres;
}


BYTE* utilFileLoad(char* cFile, DWORD *pdwFileDataSize)
{
    FILE* hFile;
    BYTE* pBuf;
    int res;
    long nSize;

    pBuf = NULL;
    hFile = NULL;

    try
    {
        hFile = fopen(cFile, "rb");
        if (NULL == hFile)
            throw "Cannot open file\n";

        res = fseek(hFile, 0, SEEK_END);
        if (res)
            throw "Cannot seek file\n";

        nSize = ftell(hFile);
        if (-1L == nSize)
            throw "Cannot ftell file\n";

        pBuf = new BYTE [nSize];
        if (NULL == pBuf)
            throw "Cannot alloc buf\n";

        res = fseek(hFile, 0, SEEK_SET);
        if (res)
            throw "Cannot seek file\n";

        fread(pBuf, nSize, 1, hFile);
        fclose(hFile);

    }

    catch( const char *s )
    {
        cout << s;
        if ( NULL != hFile )
            fclose(hFile);

        if (NULL != pBuf )
            free (pBuf);

        return NULL;
    }

    *pdwFileDataSize = (DWORD) nSize;
    return pBuf;
}

