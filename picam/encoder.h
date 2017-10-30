#ifndef ENCODER_H
#define ENCODER_H

#include "mmalincludes.h"
#include "vid_util.h"

/*
	MMAL_ENCODING_MJPEG
	MMAL_ENCODING_H264
	?MPG2, maby needs licence, maby is MMAL_ENCODING_MP2V?
   {"jpg", MMAL_ENCODING_JPEG},
   {"bmp", MMAL_ENCODING_BMP},
   {"gif", MMAL_ENCODING_GIF},
   {"png", MMAL_ENCODING_PNG}
   */
class EncoderPort {
public:
	MMAL_COMPONENT_T *encoder_component = 0;
	MMAL_PORT_T *encoder_input_port = NULL;
	MMAL_PORT_T *encoder_output_port = NULL;
    MMAL_POOL_T *encoder_pool = NULL;
    MMAL_CONNECTION_T* encoder_connection = NULL;
	MMAL_FOURCC_T encoding; /// Requested codec video encoding (MJPEG or H264)
	uint32_t framerate;
    int width;
    int height;
	uint32_t bitrate; /// Requested bitrate
	 /// Requested frame rate (fps)
	 //H264
	uint32_t intraperiod; /// Intra-refresh period (key frame rate)
	uint32_t quantisationParameter; /// Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate
	bool bInlineHeaders; /// Insert inline headers to stream (SPS, PPS)
	MMAL_VIDEO_PROFILE_T profile; /// H264 profile to use for encoding
	MMAL_VIDEO_LEVEL_T level; /// H264 level to use for encoding
	bool immutableInput;	/// Flag to specify whether encoder works in place or creates a new buffer. Result is preview can display either
							/// the camera output or the encoder output (with compression artifacts)
	int intra_refresh_type;
	PORT_USERDATA callbackData;

	void setH264Format();
	void setH264Parameters();
	void setMJPEGFormat();
	void setJPEGParameters();
	void (*callback)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) = NULL;
	EncoderPort(int width, int height);
	~EncoderPort();
	void checkDisable();
	void destroyConnection();
	void disableComponent();
	void destroyComponent();
	void open();
	void close();
	void connect(MMAL_PORT_T * port);
	void enable();
	void capture(Buffer * data);
};


#endif