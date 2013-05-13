// testJpeg.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "WINDOWS.H"
#include <iostream>

#include "SDL.h"
#include "avifmt.h"
#include "aviFileParse.h"
#include "jpgCodec.h"



#ifdef DEBUG
#define APP_TRACE 	    printf
#define APP_ERROR 	    printf(">>> Error\n"); printf
#else
#define APP_TRACE 	    //printf
#define APP_INFO 	    printf
#define APP_ERROR 	    printf(">>> Error\n"); printf
#endif

#ifndef BOOL
#define BOOL int
#endif

#define ASSERT(f)   



SDL_Surface *sdlMainScreenInit(DWORD nResW, DWORD nResH)
{
    SDL_Surface *screen;
    Uint8  video_bpp;
    Uint32 videoflags;

    /* Initialize SDL */
    if ( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 ) {
        APP_ERROR("Couldn't initialize SDL: %s\n", SDL_GetError());
        return NULL;
    }
    //atexit(SDL_Quit);			/* Clean up on exit */
    video_bpp = 24;

    /* double buffer & full screen */
    //videoflags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN;
    //screen=SDL_SetVideoMode(0, 0, video_bpp,videoflags);
    videoflags = SDL_HWSURFACE | SDL_DOUBLEBUF;
    screen=SDL_SetVideoMode(nResW, nResH, video_bpp, videoflags);

    if ( screen == NULL ) {
        APP_ERROR("Couldn't set screen x %d video mode: %s\n", video_bpp, SDL_GetError() );
        SDL_Quit();
        return NULL;
    }

	/* Set the window manager title bar */
	SDL_WM_SetCaption("PW projector Steam Viewer", "pwStreamViewer");

    APP_INFO("screen size %d x %d\n", screen->w, screen->h);

    return screen;
}

typedef struct 
{
    SDL_Surface *bitmap;
    BYTE *buffer;
    DWORD nWidth;
    DWORD nHeight;
} SDL_BMP_Surface;

BOOL sdlSurfaceCreateFromJpg(BYTE *pJpgBuf, DWORD nJpgSize, SDL_BMP_Surface *bitmapSurface)
{
    SDL_Surface *bitmap;

    Uint32 rmask, gmask, bmask, amask;
    //corresponding to IJL_BGR; //IJL_BGR: B -> Byte 0, G -> Byte 1, R -> Byte 2
    rmask = 0x00ff0000;
    gmask = 0x0000ff00;
    bmask = 0x000000ff;
    amask = 0x00000000;

    DWORD width, height, nchannels, pitch;
    BYTE *buffer;   //raw data buffer 
    BOOL bres;

    bres = DecodeFromJPEGBuffer(pJpgBuf, nJpgSize, &width, &height, &nchannels, &pitch, &buffer);

    if(!bres)
        return FALSE;

    bitmap = SDL_CreateRGBSurfaceFrom(buffer, width, height, 
        24, pitch,
        rmask, gmask, 
        bmask, amask);

    //free(buffer); CAN'T free, since bitmap surface point to this buffer.

    ASSERT(bitmapSurface != NULL);

    bitmapSurface->bitmap = bitmap;
    bitmapSurface->buffer = buffer;
    bitmapSurface->nWidth = width;
    bitmapSurface->nHeight = height;

    return TRUE;
}

void sdlBlitSurface(SDL_Surface *screen, SDL_Surface *bitmap, DWORD top, DWORD left)
{
    SDL_Rect dst={0};

    dst.x = (Sint16)left;
    dst.y = (Sint16)top;

    SDL_BlitSurface(bitmap, NULL,
        screen, &dst);

}

// _tmain1 is a demo code based on single thread for diplay PW_avi file
int _tmain1(int argc, _TCHAR* argv[])
{
    if(argc<1)
    {
        APP_ERROR("Please type %s aviFileName\n", argv[0]);
        return -1;
    }

    int pos;
    MainAVIHeader *psAviH;

    //integrityCheck(argv[1]);

    int nAviStrhPos;
    struct AVI_strh *psAviStrH;
    DWORD nMainAvihSize;

    int nSize;
    BYTE *pHdrl;

    int nVidsNo;

    SDL_Surface *screen; 


    nSize = aviHdrlBufferGet(argv[1], &pHdrl);

    if (nSize < 0)
        return -1;

    pos = aviHeaderGetFromHdrl(pHdrl, nSize, &nMainAvihSize);
    if (pos >= 0)
    {
        psAviH = (MainAVIHeader *)(pHdrl + pos);

        nAviStrhPos = vidsStrhGetFromHdrl(pHdrl, nSize, psAviH->dwStreams, &nVidsNo);
        if (nAviStrhPos > 0)
        {
            psAviStrH = (struct AVI_strh *)(pHdrl + nAviStrhPos);
        }
    }

    //create screen according to W x H.
    screen = sdlMainScreenInit(psAviH->dwWidth, psAviH->dwHeight);

    FILE* file;
    DWORD nMoviSize;

    char moviFOURCC[] = {'m', 'o', 'v', 'i'};

    file = fopen(argv[1], "rb");
    pos = aviListFileSeek(file, moviFOURCC, &nMoviSize);    //nMoviSize @ pos

    DWORD nMoviChunkSize;
    BYTE *pMoviChunk;

    int nMChunkPos;
    int nLeftSize;

    int nImgPos;
    IMG_Info sImgInfo;
    BYTE *pImg;

    SDL_BMP_Surface sBitmapSurface;
    BOOL bRes;

    Uint32 nLastFlipTicks = -1;
    Uint32 nTargetFlipTicks = 0;
    Uint32 nTicksInc = (psAviH->dwMicroSecPerFrame)/1000;
    printf("\tFrame interval is %d Ms\n", nTicksInc);

    nLeftSize = nMoviSize;

    while (nLeftSize > 0)
    {
        nMChunkPos = aviMoviChunkLoad(file, nLeftSize, &nMoviChunkSize, &pMoviChunk);

        if(nMChunkPos < 0)
        {
            printf("aviMoviChunkLoad return %d\n", nMChunkPos);
            break;
        }

        struct AVI_chunk_hdr *pChunkHdr = (struct AVI_chunk_hdr *)pMoviChunk;


        APP_TRACE("\t Found movi chunk [%c%c%c%c][%d bytes].\n", 
            pChunkHdr->chunk_id[0], pChunkHdr->chunk_id[1], pChunkHdr->chunk_id[2], pChunkHdr->chunk_id[3],
            pChunkHdr->sz);

        nImgPos = aviMoviChunkParse(pMoviChunk, nMoviChunkSize, nVidsNo, &sImgInfo);

        if(nImgPos >= 0)
        {
            ASSERT(sImgInfo.ImgType == eImgJpg );
            pImg = pMoviChunk + nImgPos;

            bRes = sdlSurfaceCreateFromJpg(pImg, sImgInfo.nImgSize, &sBitmapSurface);

            if(!bRes)
            {
                printf("Can't create bitmap surface from buffer [%x %x %x %x]\n", pImg[0], pImg[1], pImg[2], pImg[3]);
            }

            sdlBlitSurface(screen, sBitmapSurface.bitmap, sImgInfo.sLTPnt.nTop, sImgInfo.sLTPnt.nLeft); 

            //free alloc buffers;
            SDL_FreeSurface(sBitmapSurface.bitmap);
            free(sBitmapSurface.buffer);

            // Do Delay
            Uint32 ticks;
            Uint32 ticks2;
            ticks = SDL_GetTicks();

            if(ticks < nTargetFlipTicks)
            {
                //wait some time.
                SDL_Delay(nTargetFlipTicks - ticks);
                ticks2 = SDL_GetTicks();
                nLastFlipTicks = ticks2;

            }
            else
            {
                nLastFlipTicks = ticks;
            }

            nTargetFlipTicks = nLastFlipTicks + nTicksInc;

            SDL_Flip(screen);

            printf("\t\tnLastFlipTicks = %d, nTargetFlipTicks = %d\n", nLastFlipTicks, nTargetFlipTicks);
        }

        free(pMoviChunk);

        nLeftSize = nMoviSize - ((nMChunkPos - pos) + nMoviChunkSize);
        APP_TRACE(" Left %d bytes\n", nLeftSize);
    }




    fclose(file);
    free(pHdrl);

}


//_tmain0 is only for test JPEG decode & SDL blit. It doesn't handle PW_avi file
int _tmain0(int argc, _TCHAR* argv[])
{
    SDL_Surface *screen;
    Uint8  video_bpp;
    Uint32 videoflags;

    BOOL bres;
    //LPCSTR lpszPathName,

    DWORD width;
    DWORD height;
    DWORD nchannels;
    BYTE* buffer;
    DWORD pitch;

    char fn[128];

    char *jpg;

    if(argc<1)
    {
        APP_ERROR("Please type %s jpgName\n", argv[0]);
        return -1;
    }

    /* Initialize SDL */
    if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
        APP_ERROR("Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);			/* Clean up on exit */


    video_bpp = 24;

    /* double buffer & full screen */
    //videoflags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN;
    //screen=SDL_SetVideoMode(0, 0, video_bpp,videoflags);

    videoflags = SDL_HWSURFACE | SDL_DOUBLEBUF;
    screen=SDL_SetVideoMode(1280, 800, video_bpp,videoflags);
    if ( screen == NULL ) {
        APP_ERROR("Couldn't set screen x %d video mode: %s\n", video_bpp, SDL_GetError() );
        exit(2);
    }

    APP_INFO("screen size %d x %d\n", screen->w, screen->h);

    buffer = NULL;
    for (int i=0; i<10; i++)
    {
        DWORD nJpgSize;
        BYTE *pJpgBuf;
        sprintf_s(fn, "wxga_%1d.JPG",i%2);
        //sprintf_s(fn, "%s",argv[1], i%2);
        jpg = fn;


        //bres = DecodeJPGFileToGeneralBuffer( jpg, &width, &height, &nchannels, &pitch, &buffer);
        //bres = DecodeJPGFileToGeneralBuffer( argv[1], &width, &height, &nchannels, &pitch, &buffer);

        pJpgBuf = utilFileLoad(jpg, &nJpgSize);
        if (NULL == pJpgBuf)
            continue;
        bres = DecodeFromJPEGBuffer(pJpgBuf, nJpgSize, &width, &height, &nchannels, &pitch, &buffer);
        free(pJpgBuf);


        SDL_Surface *bitmap;
        Uint32 rmask, gmask, bmask, amask;

        //corresponding to IJL_BGR; //IJL_BGR: B -> Byte 0, G -> Byte 1, R -> Byte 2
        rmask = 0x00ff0000;
        gmask = 0x0000ff00;
        bmask = 0x000000ff;
        amask = 0x00000000;

        SDL_Rect dst={0};

        //make dst in center of screen.
        if( (screen->w >= width) && (screen->h >= height) )
        {
            dst.x = (Sint16)(screen->w - width) /2;
            dst.y = (Sint16)(screen->h - height) /2;
        }

        bitmap = SDL_CreateRGBSurfaceFrom(buffer, width, height, 
            24, pitch,
            rmask, gmask, 
            bmask, amask);

        SDL_BlitSurface(bitmap, NULL,
            screen, &dst);

        //free alloc buffers;
        SDL_FreeSurface(bitmap);
        free(buffer);

        Uint32 ticks;
        // Do Delay
        ticks = SDL_GetTicks();
        while(SDL_GetTicks() - ticks < 2000)
        {
            SDL_Delay(1000);
        }

        SDL_Flip(screen);
    }

    /* Shutdown all subsystems */
    SDL_Quit();

    printf("Quiting....\n");

    return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////

#define PW_BLIT_EVENT   (SDL_USEREVENT)
#define PW_FLIP_EVENT (SDL_USEREVENT + 1)
#define PW_QUIT_EVENT (SDL_USEREVENT + 2)
#define PW_FRAME_REPEAT (SDL_USEREVENT+3)


#define TIME_CTRL_0   0                     //not suggested used. code doesn't complete to support 0 size vids trunk which is used for delay one frame time.
#define TIME_CTRL_1   1
#define REFRESH_TIME_CTRL   TIME_CTRL_1

typedef struct 
{
    DWORD x;
    DWORD y;
} POSITION;

typedef struct VideoState 
{
    /*
    int             videoStream, audioStream;

    SDL_Thread      *parse_tid;
    SDL_Thread      *video_tid;
    */

    SDL_BMP_Surface sBitmapSurface;     //a SDL bitmap surface which waiting to be blit to screen. Blit top-left is sBlitLTPoint
    POSITION        sBlitLTPoint;       //see comment of sBitmapSurface.

    int             bBlited;    //Init value = 1;
                                //1: we can overwrite sBitmapSurface & sBlitLTPoint. 0: we need wait previous surface blited before overwrite.

    int             bFliped;    //Init value = 1. 
                                //1: we can blit to screen. 0: we need wait it to 1 before do blit. 

    SDL_mutex       *mutex;
    SDL_cond        *cond;
    SDL_Thread      *parse_tid;
    SDL_Surface     *screen;

    int             nVidsNo;        //the StrNo for vids stream   
    //int             nTicksInc;    //refresh interval. millisecond. no needs.
    //Uint32          nRefreshTicks;  // target refresh tich, millisecond.

    int             nCurFrameCnt;   //trace current frame counter.
    Uint32          nRefTicks;      //nRefTicks is used for TIME_CTRL_1 solution. it recorded the 1'th frame refresh tick.

    char            filename[1024];
    int             quit;
    int             pause;
} VideoState;


int decode_thread(void *arg) 
{
    VideoState *paviState = (VideoState *)arg;

    FILE* file;
    DWORD nMoviSize;
    int pos;

    char moviFOURCC[] = {'m', 'o', 'v', 'i'};

    file = fopen(paviState->filename, "rb");
    pos = aviListFileSeek(file, moviFOURCC, &nMoviSize);    //nMoviSize @ pos

    DWORD nMoviChunkSize;
    BYTE *pMoviChunk;

    int nMChunkPos;
    int nLeftSize;

    int nImgPos;
    IMG_Info sImgInfo;
    BYTE *pImg;

    SDL_BMP_Surface sBitmapSurface;
    BOOL bRes;

    Uint32 nLastFlipTicks = -1;
    Uint32 nTargetFlipTicks = 0;

    nLeftSize = nMoviSize;

    int nVidsNo = paviState->nVidsNo;

    while ( (nLeftSize > 0) && 
            !paviState->quit)
    {
        nMChunkPos = aviMoviChunkLoad(file, nLeftSize, &nMoviChunkSize, &pMoviChunk);

        if(nMChunkPos < 0)
        {
            printf("aviMoviChunkLoad return %d\n", nMChunkPos);
            break;
        }

        struct AVI_chunk_hdr *pChunkHdr = (struct AVI_chunk_hdr *)pMoviChunk;


        APP_TRACE("\t Found movi chunk [%c%c%c%c][%d bytes] @[%d].\n", 
            pChunkHdr->chunk_id[0], pChunkHdr->chunk_id[1], pChunkHdr->chunk_id[2], pChunkHdr->chunk_id[3],
            pChunkHdr->sz, nMChunkPos);

        nImgPos = aviMoviChunkParse(pMoviChunk, nMoviChunkSize, nVidsNo, &sImgInfo);

        if(nImgPos >= 0 
            && (pChunkHdr->sz > 0))         //add to handle 0 size vids trunk 
        {
            ASSERT(sImgInfo.ImgType == eImgJpg );
            pImg = pMoviChunk + nImgPos;

            bRes = sdlSurfaceCreateFromJpg(pImg, sImgInfo.nImgSize, &sBitmapSurface);

            if(!bRes)
            {
                printf("Can't create bitmap surface from buffer [%x %x %x %x]\n", pImg[0], pImg[1], pImg[2], pImg[3]);
            }
            else
            {
                // wait until we store sBitmapSurface & sBlitLTPoint to paviState structure;
                SDL_LockMutex(paviState->mutex);
                //paviState->bBlited = 1: we can overwrite sBitmapSurface & sBlitLTPoint. 
                //0: we need wait previous surface blited before overwrite.
                while( !paviState->bBlited && !paviState->quit) 
                {
                    SDL_CondWait(paviState->cond, paviState->mutex);
                }
                SDL_UnlockMutex(paviState->mutex);

                if(paviState->quit)
                {
                    //free alloc buffers;
                    SDL_FreeSurface(sBitmapSurface.bitmap);
                    free(sBitmapSurface.buffer);
                }
                else
                {

                    paviState->sBitmapSurface = sBitmapSurface;
                    paviState->sBlitLTPoint.x = sImgInfo.sLTPnt.nLeft;
                    paviState->sBlitLTPoint.y = sImgInfo.sLTPnt.nTop;
                    paviState->bBlited        = 0;              //change flag to indicate this surface not be blited.


                    SDL_Event event;
                    // Let main thread to handle blit operation.
                    event.type = PW_BLIT_EVENT;
                    event.user.data1 = paviState;
                    SDL_PushEvent(&event);

                }
            }
        }
        else if((nImgPos >= 0) && 
            (pChunkHdr->sz == 0))    //add to handle 0 size vids trunk, which is used to delay one frame time
        {
            //although we don't need to do blit, we still follow the wait scheme like blit, it would keep not break pipeline's rule
            // wait until we store sBitmapSurface & sBlitLTPoint to paviState structure;
            SDL_LockMutex(paviState->mutex);
            //paviState->bBlited = 1: we can overwrite sBitmapSurface & sBlitLTPoint. 
            //0: we need wait previous surface blited before overwrite.
            while( !paviState->bBlited && !paviState->quit) 
            {
                SDL_CondWait(paviState->cond, paviState->mutex);
            }
            SDL_UnlockMutex(paviState->mutex);

            if(paviState->quit)
            {
                //do nothing;
            }
            else
            {
                SDL_Event event;
                // Let main thread to handle blit operation.
                event.type = PW_FRAME_REPEAT;
                event.user.data1 = paviState;
                SDL_PushEvent(&event);
            }
        }

        free(pMoviChunk);

        nLeftSize = nMoviSize - ((nMChunkPos - pos) + nMoviChunkSize);
        APP_TRACE(" Left %d bytes\n", nLeftSize);

        //pause if needed.
        SDL_LockMutex(paviState->mutex);
        while( (paviState->pause) && !paviState->quit) 
        {
            SDL_CondWait(paviState->cond, paviState->mutex);
        }
        SDL_UnlockMutex(paviState->mutex);
    }

    fclose(file);

    return 0;
}

void _doBlit1(VideoState *paviState)
{
    if (paviState->bBlited)
        return;

    sdlBlitSurface(paviState->screen, paviState->sBitmapSurface.bitmap, 
                    paviState->sBlitLTPoint.y, paviState->sBlitLTPoint.x); 

    //free alloc buffers;
    SDL_FreeSurface(paviState->sBitmapSurface.bitmap);
    free(paviState->sBitmapSurface.buffer);

    //reset sBitmapSurface.
    memset(&(paviState->sBitmapSurface), 0, sizeof (paviState->sBitmapSurface));
}

void blitDoneNotify(VideoState *paviState)
{
    SDL_LockMutex(paviState->mutex);
    paviState->bBlited = 1;         //set flag to indicate previous surface is blited.
    SDL_CondSignal(paviState->cond);
    SDL_UnlockMutex(paviState->mutex);
}

//return 1 if blit complete, 0 if need postpone.
int doBlit(void *arg)
{
    VideoState *paviState = (VideoState *)arg;

    //check lastest blit is be flip or not.
    //if flip, we can blit to screen.
    //otherwise, we can't blit so far. Postpone blit operation after flip.
    SDL_LockMutex(paviState->mutex);
    if( !paviState->bFliped )
    {
        SDL_UnlockMutex(paviState->mutex);
        return 0;
    }
    SDL_UnlockMutex(paviState->mutex);

    //do blit.
    _doBlit1(paviState);

    blitDoneNotify(paviState);

    if ( !paviState->pause )
    {
        paviState->bFliped = 0;
        return 1;
    }
    else
    {
        //if pause, not change bFliped flag. & return 0 to not start refresh operation.
        return 0;
    }
}


int doRefresh(void *arg)
{
    VideoState *paviState = (VideoState *)arg;

    if ( !paviState->pause )
    {
        SDL_Flip(paviState->screen);
    }
    //For pause case, we didn't Flip, then no refresh after pause.

    paviState->bFliped = 1;     //update flag to say that it has fliped.

    //check whether there is pending blit operation
    if (! paviState->bBlited)
    {
        SDL_Event event;
        // Let main thread to handle blit operation.
        event.type = PW_BLIT_EVENT;
        event.user.data1 = paviState;
        SDL_PushEvent(&event);
    }
    return 1;
}

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) 
{
    SDL_Event event;
    event.type = PW_FLIP_EVENT;
    event.user.data1 = opaque;
    SDL_PushEvent(&event);
    return 0; /* 0 means stop timer */
}

// schedule a video refresh in 'delay' ms 
static void schedule_refresh(VideoState *is, int delay) 
{
    if(delay > 0)
    {
        SDL_AddTimer(delay, sdl_refresh_timer_cb, is);
    }
    else
    {
        //post event
        SDL_Event event;
        event.type = PW_FLIP_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
}


int _tmain(int argc, _TCHAR* argv[])
{
    if(argc<2)
    {
        APP_ERROR("Please type %s aviFileName\n", argv[0]);
        return -1;
    }

    int pos;
    MainAVIHeader *psAviH;


    //integrityCheck(argv[1]);

    int nAviStrhPos;
    struct AVI_strh *psAviStrH;
    DWORD nMainAvihSize;

    int nSize;
    BYTE *pHdrl;

    int nVidsNo;

    SDL_Surface *screen; 

    nSize = aviHdrlBufferGet(argv[1], &pHdrl);

    if (nSize < 0)
        return -1;

    pos = aviHeaderGetFromHdrl(pHdrl, nSize, &nMainAvihSize);
    if (pos >= 0)
    {
        psAviH = (MainAVIHeader *)(pHdrl + pos);

        nAviStrhPos = vidsStrhGetFromHdrl(pHdrl, nSize, psAviH->dwStreams, &nVidsNo);
        if (nAviStrhPos > 0)
        {
            psAviStrH = (struct AVI_strh *)(pHdrl + nAviStrhPos);
        }
    }

    //create screen according to W x H.
    screen = sdlMainScreenInit(psAviH->dwWidth, psAviH->dwHeight);

    Uint32 nTicksInc = (psAviH->dwMicroSecPerFrame)/1000;
    Uint32 nTicksIncUs = (psAviH->dwMicroSecPerFrame);
    printf("\tFrame interval is %d Ms\n", nTicksInc);

    //release memory
    free(pHdrl);

    //create decode thread.
    VideoState aviStream;
    memset(&aviStream, 0 , sizeof(aviStream));
    strcpy(aviStream.filename, argv[1]);

    aviStream.screen = screen;
    aviStream.mutex = SDL_CreateMutex();
    aviStream.cond = SDL_CreateCond();
    aviStream.bBlited = 1;
    aviStream.bFliped = 1;
    //aviStream.nTicksInc = nTicksInc;

    aviStream.parse_tid = SDL_CreateThread(decode_thread, &aviStream);
    if(!aviStream.parse_tid) 
    {
        SDL_Quit();
        return -1;
    }

    SDL_Event   event;
    int res;
    Uint32 nRefreshTicks = 0;
    for(;;) 
    {
        SDL_WaitEvent(&event);
        switch(event.type) 
        {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) 
            {
                case SDLK_SPACE: 

                    if(!aviStream.pause)
                    {
                        aviStream.pause = 1;
                        printf("\t\t pause!\n");
                    }
                    else
                    {
                        //reset time control 
                        if(REFRESH_TIME_CTRL == TIME_CTRL_0)
                        {
                            nRefreshTicks = 0;
                        }
                        else
                        {
                            aviStream.nCurFrameCnt = 0;
                            aviStream.nRefTicks = 0;
                        }

                        printf("\n\n resume!\n");
                        SDL_LockMutex(aviStream.mutex);
                        aviStream.pause = 0; 
                        SDL_CondSignal(aviStream.cond);
                        SDL_UnlockMutex(aviStream.mutex);
                    }
                    break;
            }
            break;
        case PW_QUIT_EVENT:
        case SDL_QUIT:
            aviStream.quit = 1;
            SDL_CondSignal(aviStream.cond);
            SDL_WaitThread(aviStream.parse_tid, NULL);

            SDL_Quit();
            SDL_DestroyCond(aviStream.cond);
            SDL_DestroyMutex(aviStream.mutex);
            return 0;
            break;

        case PW_FRAME_REPEAT:
                if(REFRESH_TIME_CTRL == TIME_CTRL_0)
                {
                    //we not plan to support for TIME_CTRL_0 mode.
                }
                else
                {
                    // we just need to advanced the cnt, then next frame refresh would has such delay time.
                    aviStream.nCurFrameCnt++;

                    //for repeat frame case, if it is first frame, we needs update the nRefTicks
                    if(1 == aviStream.nCurFrameCnt)
                    {
                        aviStream.nRefTicks = SDL_GetTicks();
                    }

                    printf("\t\t [%d] frame repeat. frame timing ctrl 1\n", aviStream.nCurFrameCnt);
                }



            break;

        case PW_BLIT_EVENT:
            res = doBlit(event.user.data1);

            //schedule refresh timer if blit success. Or do nothing if postponed.
            if (res)
            {
                if(REFRESH_TIME_CTRL == TIME_CTRL_0)
                {
                    Uint32 ticks;
                    ticks = SDL_GetTicks();
                    int delay; 
                    static int nFrameCnt = 0;

                    delay = (nRefreshTicks > ticks)? (nRefreshTicks - ticks) : 0;  
                    schedule_refresh(&aviStream, delay);

                    printf("\t\t[%d] delay tick = %d Ms. frame timing ctrl 0\n", nFrameCnt, delay);
                    nFrameCnt++;
                }
                else
                {
                    aviStream.nCurFrameCnt++;

                    Uint32 nTargetTicks;
                    nTargetTicks = aviStream.nRefTicks + ((aviStream.nCurFrameCnt)*nTicksIncUs)/1000;

                    Uint32 ticks;
                    ticks = SDL_GetTicks();
                    int delay;

                    delay = (nTargetTicks > ticks)? (nTargetTicks - ticks) : 0;
                    schedule_refresh(&aviStream, delay);

                    //printf("\t\t [%d] delay tick = %d Ms. frame timing ctrl 1\n", aviStream.nCurFrameCnt, delay);
                    printf("\t\t [%d] delay tick = %d Ms. Show at %d Ms. frame timing ctrl 1\n", aviStream.nCurFrameCnt, delay, nTargetTicks-aviStream.nRefTicks);
                }

            }


            break;

        case PW_FLIP_EVENT:
            
            if(REFRESH_TIME_CTRL == TIME_CTRL_0)
            {
                //update nRefreshTicks
                Uint32 ticks;
                ticks = SDL_GetTicks();
                nRefreshTicks = ticks + nTicksInc;
                //printf("\t\t Refresh tick = %d Ms\n", ticks);
            }
            else
            {
                if(1 == aviStream.nCurFrameCnt)
                {
                    aviStream.nRefTicks = SDL_GetTicks();
                }
            }

            doRefresh(event.user.data1);
            break;

        default:
            break;
        }
    }
   
    return 0;

}

