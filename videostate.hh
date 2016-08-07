#ifndef VIDEOSTATE_HH
#define VIDEOSTATE_HH

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

#define MAX_VIDEOQUEUE_SIZE (5*256*1024)
#define VIDEO_PICTURE_QUEUE_SIZE 5

enum {
	RBR_INITWND = SDL_USEREVENT,
	RBR_UPDATEYUV
};

typedef struct {
//	SDL_Overlay* pYUV; // needs to change
	Uint16 		 nWidth;
	Uint16 		 nHeight;
	double 		 lfPTS; // get rid of this
	Sint32 		 bAllocated;
} VideoPicture;

class ReberaDecoder {

private:

	bool m_bRunning;
	int b_gotdimensions = 0;

public:

	class AVPacketQueue {

	protected:
		AVPacketList* ffmpeg_avpacketlist_head;
		AVPacketList* ffmpeg_avpacketlist_rear;
		unsigned	  m_nPackets;
		unsigned	  m_nSize;
		SDL_cond*	  m_pCond;
		SDL_mutex*	  m_pMutex;
		ReberaDecoder*   m_pVideoState;

	public:
		AVPacketQueue( ReberaDecoder* );
		~AVPacketQueue(void);

		int  	 GetAVPacket( AVPacket* );
		unsigned GetSizeBytes() { return m_nSize; };
		unsigned GetSizePackets() { return m_nPackets; };

		void PutAVPacket( AVPacket* );
	};

	Uint32 PictureQSize;
	Uint32 PictureQReadIndex;
	Uint32 PictureQWriteIndex;

	SDL_cond*  m_PictureQCond;
	SDL_mutex* m_PictureQMutex;

	SDL_Window* 	m_pWindow;
	SDL_Renderer* 	m_pRenderer;
	SDL_Texture* 	m_pTexture;

	AVCodec* 			FFmpeg_avcodec;
	AVCodecContext*     m_pFFmpeg_avcodeccontxt;
	AVFrame* 			FFmpeg_avframe;
	AVPacketQueue*      m_pAVPacketQ;
	VideoPicture        m_PictureQ[VIDEO_PICTURE_QUEUE_SIZE];

    ReberaDecoder( void );
   ~ReberaDecoder( void );

    VideoPicture* 	 GetPictRead () 		{ return &m_PictureQ[PictureQReadIndex]; };
    VideoPicture* 	 GetPictWrite() 		{ return &m_PictureQ[PictureQWriteIndex]; };

    AVCodecContext*  GetVideoCodecContext() { return m_pFFmpeg_avcodeccontxt;  };
    int 	 		 GetVideoHeight() 		{ return GetVideoCodecContext()->height; };
    int 	 		 GetVideoWidth() 		{ return GetVideoCodecContext()->width ; };
    AVPacketQueue* 	 GetAVPacketQueue() 	{ return m_pAVPacketQ; };
    SDL_Renderer*	 GetSDLRenderer()		{ return m_pRenderer; };
    SDL_Texture*	 GetSDLTexture()		{ return m_pTexture; };

    bool IsRunning() 		{ return m_bRunning; };
    bool IsVideoQueueFull() { return m_pAVPacketQ->GetSizeBytes() >= MAX_VIDEOQUEUE_SIZE; };
    int  knows_dimensions() { return b_gotdimensions; };

    void SetQuitFlag() { m_bRunning = false; };
    void SetVideoCodecContext( AVCodecContext* pCodec ) { m_pFFmpeg_avcodeccontxt = pCodec; };

	/* ??? */
	void LockPictureSDLMutex()  { SDL_LockMutex(m_PictureQMutex);   };
	void UnlockPictureSDLMutex(){ SDL_UnlockMutex(m_PictureQMutex); };
	void SignalPictureSDLCond() { SDL_CondSignal(m_PictureQCond); };
	void WaitPictureSDLCond() 	{ SDL_CondWait(m_PictureQCond, m_PictureQMutex); };

	void InitializeSDLWindow( void );
    void refresh_display( void );
	int decode_frame( AVPacket* );

	int WriteFrame(const AVPicture* pPict, Uint32 nHeight);
	int WriteInternalFrame(Uint32 nTimes, Uint32 nHeight);
};

#endif
