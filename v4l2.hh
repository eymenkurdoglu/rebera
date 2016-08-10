#ifndef V4L2_HH
#define V4L2_HH

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#include <SDL2/SDL.h>
#include <unistd.h> // for usleep

struct buffer {
    void*  start;
    size_t length;
};

class v4l2capture {

	private:

	char const * dev_name;
	int fd;
	struct buffer* buffers;
	unsigned n_buffers;

	int i_width;
	int i_height;
	uint8_t* pY;
	uint8_t* pCb;
	uint8_t* pCr;
	int i_pixels;

	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_RendererInfo info;
	SDL_Surface* image;
	SDL_Texture* texture;
	SDL_Event event;

	public:

	v4l2capture( char const * );
	~v4l2capture( void );
	int xioctl( int, int, void* );
	int read_frame( void );
	int get_fd( void ) { return fd; };
	int get_width( void ) { return i_width; };
	int get_height( void ) { return i_height; };
	void errno_exit( const char * );
	void process_image( const void*, unsigned int );
	void open_device( void );
	void init_device( void );
	void start_capturing( void );
	void stop_capturing ( void );
	void uninit_device( void );
	void close_device( void );
	void init_mmap( void );
	void init_sdl2( void );
	void set_picbuff( uint8_t* _Ybuf_, uint8_t* _Cbbuf_, uint8_t* _Crbuf_ )
	{
		pY  = _Ybuf_;
		pCb = _Cbbuf_;
		pCr = _Crbuf_;
	};
	void display_raw( void ) {
		SDL_UpdateTexture(texture, NULL, pY, i_width);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
	};
};

#endif
