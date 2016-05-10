#include "v4l2.hh"
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

void v4l2capture::errno_exit(const char *s) {
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

int v4l2capture::xioctl( int fh, int request, void* arg ) {
/*
POSIX specification defines that when signal (such as Ctrl+C) is caught, the blocking function returns EINTR error.
*/
    int r;
    do { r = ioctl(fh, request, arg); }
    while (-1 == r && EINTR == errno);

return r;
}

v4l2capture::v4l2capture( char* device )
: dev_name( device )
, fd( -1 )
, buffers( NULL )
, n_buffers( 0 )
{
	open_device();
	init_device();
	init_sdl2();
}

v4l2capture::~v4l2capture( void ) {
	//~ if ( pY ) delete pY;
	uninit_device();
	close_device();
}

int v4l2capture::read_frame( void ) {
/*
 * To query the current image format applications set the type field of a struct v4l2_format to V4L2_BUF_TYPE_VIDEO_CAPTURE or V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE and call the VIDIOC_G_FMT ioctl with a pointer to this structure.
 * Applications call the VIDIOC_DQBUF ioctl to dequeue a filled (capturing) or displayed (output) buffer from the driver's outgoing queue. They just set the type, memory and reserved fields of a struct v4l2_buffer as above, when VIDIOC_DQBUF is called with a pointer to this structure the driver fills the remaining fields or returns an error code.
 * To enqueue a memory mapped buffer, applications set the memory field to V4L2_MEMORY_MMAP.
 * By default VIDIOC_DQBUF blocks when no buffer is in the outgoing queue. When the O_NONBLOCK flag was given to the open() function, VIDIOC_DQBUF returns immediately with an EAGAIN error code when no buffer is available.
*/
	struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if ( -1 == xioctl(fd, VIDIOC_DQBUF, &buf) ) {
        switch (errno) {
            case EAGAIN:
                return -1;
            case EIO:
            default:
                errno_exit("VIDIOC_DQBUF");
        }
    }

    process_image( buffers[buf.index].start, buf.bytesused );

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
        errno_exit("VIDIOC_QBUF");
	}

return 0;
}

void v4l2capture::stop_capturing( void ) {
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if ( -1 == xioctl(fd, VIDIOC_STREAMOFF, &type) )
        errno_exit("VIDIOC_STREAMOFF");
}

void v4l2capture::start_capturing( void ) {

    for ( unsigned i = 0; i < n_buffers; ++i )
    {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");
    }
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");
}

void v4l2capture::open_device( void ) {
    struct stat st; // #include <sys/stat.h>

	// stat - get file status: int stat(const char* path, struct stat* buf);
    if (-1 == stat(dev_name, &st)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n", dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) { //Test for a character special file, non-zero if yes
        fprintf(stderr, "%s is no device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (fd == -1) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void v4l2capture::init_mmap( void ) {
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 32;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } else
            errno_exit("VIDIOC_REQBUFS");
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
        exit(EXIT_FAILURE);
    }

    buffers = (buffer*)calloc(req.count, sizeof(*buffers));

    if (!buffers) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {

        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
        mmap(NULL /* start anywhere */,
             buf.length,
             PROT_READ | PROT_WRITE /* required */,
             MAP_SHARED /* recommended */,
             fd,
             buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit("mmap");
    }
}

void v4l2capture::init_device( void ) {
    struct v4l2_capability 	cap;

    if ( -1 == xioctl(fd, VIDIOC_QUERYCAP, &cap) ) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\n", dev_name);
            exit(EXIT_FAILURE);
        } else
            errno_exit("VIDIOC_QUERYCAP");
    }

    fprintf(stderr, "  Driver: \"%s\"\n"
            "  Device: \"%s\"\n"
            "  Bus: \"%s\"\n"
            "  Version: %u.%u.%u\n"
            "  Capabilities: 0x%08x\n",
            cap.driver,
            cap.card,
            cap.bus_info,
            (cap.version>>16) & 0xff, (cap.version>>8) & 0xff, cap.version & 0xFF,
            cap.capabilities);

    if ( !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ) {
        fprintf(stderr, "%s is no video capture device\n", dev_name);
        exit(EXIT_FAILURE);
    }

	if ( !(cap.capabilities & V4L2_CAP_STREAMING) ) {
		fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
		exit(EXIT_FAILURE);
	}

	struct v4l2_fmtdesc fmtdesc;
    CLEAR(fmtdesc);
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    char fourcc[5] = {0};
    char c, e;
    fprintf(stderr,"> Supported formats:\n");
    while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
        strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
        c = fmtdesc.flags & 1 ? 'C' : ' ';
        e = fmtdesc.flags & 2 ? 'E' : ' ';
        fprintf(stderr, "  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
        fmtdesc.index++;
    }

	struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	if ( -1 == xioctl(fd, VIDIOC_S_FMT, &fmt) )
		errno_exit("VIDIOC_S_FMT");

	strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
    fprintf(stderr, "> Selected Camera Mode:\n"
            "  Width: %d\n"
            "  Height: %d\n"
            "  PixFmt: %s\n"
            "  Field: %d\n",
            fmt.fmt.pix.width,
            fmt.fmt.pix.height,
            fourcc,
            fmt.fmt.pix.field);

	i_width  = fmt.fmt.pix.width;
	i_height = fmt.fmt.pix.height;
	i_pixels = i_width*i_height;
	//~ pY = new uint8_t[(i_pixels*3)>>1];
	//~ pCb = pY + i_pixels;
	//~ pCr = pCb + (i_pixels>>2);

	struct v4l2_frmivalenum fr;
    CLEAR(fr);

    fr.pixel_format = fmt.fmt.pix.pixelformat;
    fr.width = fmt.fmt.pix.width;
    fr.height = fmt.fmt.pix.height;
    fr.index = 0;

    if (-1 == xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fr))
        errno_exit("VIDIOC_ENUM_FRAMEINTERVALS");

    if (fr.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		struct v4l2_streamparm 	strmp;
		CLEAR(strmp);
		strmp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		strmp.parm.capture.timeperframe.numerator = fr.discrete.numerator;
		strmp.parm.capture.timeperframe.denominator = fr.discrete.denominator;
		if (-1 == xioctl(fd, VIDIOC_S_PARM, &strmp))
			errno_exit("VIDIOC_S_PARM");
		fprintf(stderr, "> Frame rate: %f fps\n", (float)(strmp.parm.capture.timeperframe.denominator)/strmp.parm.capture.timeperframe.numerator);
    }
    init_mmap();
}

void v4l2capture::close_device(void) {
    if ( -1 == close(fd) )
        errno_exit("close");

    fd = -1;
}

void v4l2capture::uninit_device( void ) {
    unsigned int i;

    for (i = 0; i < n_buffers; ++i)
        if (-1 == munmap(buffers[i].start, buffers[i].length))
            errno_exit("munmap");

    free(buffers);
}

void v4l2capture::process_image( const void *p, unsigned int size ) {

	//~ uint8_t* pY = buff;
	//~ uint8_t* pCb = pY + i_pixels;
	//~ uint8_t* pCr = pCb + (i_pixels>>2);

	unsigned int n = 0;
	uint8_t* pCurr = (uint8_t*)p;
	int halfwidth = i_width>>1;

	for ( int y = 0; y < i_height; y += 2, pCurr += i_width*4) {
		uint8_t* pHO2 = pCurr;
		for ( int x = 0; x < halfwidth; pHO2 += 4, x++, n++) {
			uint8_t* pdummy = pY + n*4 - x*2;
			pdummy[0] = pHO2[0];
			pdummy[1] = pHO2[2];
			pdummy[i_width] = pHO2[i_width*2];
			pdummy[i_width+1] = pHO2[i_width*2+2];
			pCb[n] = (uint8_t) ((pHO2[1] + pHO2[2*i_width+1] + 1)>>1);
			pCr[n] = (uint8_t) ((pHO2[3] + pHO2[2*i_width+3] + 1)>>1);
		}
	}

    SDL_UpdateTexture(texture, NULL, pY, i_width);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

	//~ if ( out_buf ) {
		//~ fwrite(pY, 1, (i_pixels*3)>>1, stdout);
	//~ }
}

void v4l2capture::init_sdl2( void ) {
	if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		fprintf( stderr, "Cannot initialize SDL: %s\n", SDL_GetError() );
		exit( EXIT_SUCCESS );
	}
	window = SDL_CreateWindow( "Rebera Sender - NYU Video Lab", 300, 300, i_width, i_height, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE );
	renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED );
	SDL_GetRendererInfo( renderer, &info );
	fprintf( stderr, "> Using %s rendering\n\n", info.name );
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, i_width, i_height);
}