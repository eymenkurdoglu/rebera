#ifndef ENCODER_HH
#define ENCODER_HH

#include <stdint.h>
#include <stdio.h>
extern "C" {
#include <x264.h>
#include <x264_config.h>
}

#define RBR_INTRAPERIOD 32
#define RBR_MAX_NUM_REF 2
#define RBR_INIT_BITRATE 200

class x264encoder {

	private:

	x264_param_t 	param;
    x264_picture_t 	pic_in, pic_out;
    x264_t* 		encoder;
    x264_nal_t* 	nal;

    int i_frame;
    int i_frame_size;
    int i_nal;

    FILE* readfrom;
    FILE* writeto;
	int i_readuntil;

	public:

	x264encoder( int, int );
	x264encoder( int, int, FILE* );
	~x264encoder();

	void set_input( FILE* );
	void init( int, int );
	int encode_frame( void );
	int read_frame( void );
	int change_bitrate( int );
	uint8_t* get_nal( void ) { return nal->p_payload; };

	uint8_t* get_Ybuff( void ) { return pic_in.img.plane[0]; };
	uint8_t* get_Cbbuff( void ) { return pic_in.img.plane[1]; };
	uint8_t* get_Crbuff( void ) { return pic_in.img.plane[2]; };
};

#endif