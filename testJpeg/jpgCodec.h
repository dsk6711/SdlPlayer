

#ifndef _JPGCODEC_H_
#define _JPGCODEC_H_

//----------------------------------------------------------
// -- Decode a JPEG image from a JFIF file to general pixel buffer.
//----------------------------------------------------------
BOOL DecodeJPGFileToGeneralBuffer(char* JPGFile,
                                  DWORD* width,
                                  DWORD* height,
                                  DWORD* nchannels,
                                  DWORD* pitch,
                                  BYTE** buffer);


//----------------------------------------------------------
// -- Decode image from a JFIF buffer.
//----------------------------------------------------------
BOOL DecodeFromJPEGBuffer(BYTE* lpJpgBuffer,
                          DWORD dwJpgBufferSize,
                          DWORD* width,
                          DWORD* height,
                          DWORD* nchannels,
                          DWORD* pitch,
                          BYTE** buffer);

BYTE* utilFileLoad(char* cFile, DWORD *pdwFileDataSize);

#endif