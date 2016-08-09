#ifndef ENCODER_HH
#define ENCODER_HH

#include <stdint.h>
#include <stdio.h>
#include "v4l2.hh"
#include "util.hh"
extern "C" {
#include <x264.h>
#include <x264_config.h>
}

#define RBR_MAX_NUM_REF 2
#define RBR_MIN_ENCODING_RATE 96
#define RBR_MAX_ENCODING_RATE 3000

class ReberaEncoder {

	private:

	x264_t* 		encoder;
	x264_nal_t* 	nal;
	x264_param_t 	param;
    x264_picture_t 	pic_in, pic_out;  

    int 		i_frame_size;
    int 		i_nal;
    unsigned 	u_luma_size;
    unsigned 	u_chroma_size;

    int			i_frame_index;  // index of the frame in the intra-period
    int			i_last_idr = 0; // i_enc_frame number of the last IDR frame 
    int 		i_trim = 1;		// FR reduction factor, default: no reduction
    
    const double d_ipr = 16.0/15; 	// intra-period duration in seconds
	double d_fr; 					// encoding frame rate
	int			i_ipr = 32;
    uint64_t	ul_raw_ts = 0;
    double		d_interval = 0;
        	
	/* input methods for the encoder */
    FILE* readfrom = NULL;
	int i_readuntil = 0;
	v4l2capture* vidcap = NULL;

	public:

    int i_raw_frame = 0;
    int i_enc_frame = 0;

	ReberaEncoder( int, int );
	~ReberaEncoder();
	
	int get_frame( void );
	int compress_frame( void );
	int change_bitrate( double );
	
	void attach_file ( FILE* _file_ ) { readfrom = _file_; vidcap = NULL; };
	void attach_camera ( v4l2capture* _dev_ ) { vidcap = _dev_; readfrom = NULL; };
	void reduce_fr_by( int _trim_ ) { i_trim = _trim_; };
	int get_frame_index( void ) { return i_frame_index; };
	double get_ipr( void ) { return d_ipr; };
	double	get_fr( void ) { return d_fr; };
	
	uint8_t* get_nal( void ) { return nal->p_payload; };
	uint8_t* get_Ybuff( void ) { return pic_in.img.plane[0]; };
	uint8_t* get_Cbbuff( void ) { return pic_in.img.plane[1]; };
	uint8_t* get_Crbuff( void ) { return pic_in.img.plane[2]; };
};

#endif
