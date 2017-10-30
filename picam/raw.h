#ifndef RAW_H
#define RAW_H

#include "mmalincludes.h"
#include "vid_util.h"

typedef enum {
	RAW_OUTPUT_FMT_YUV = 1,
	RAW_OUTPUT_FMT_RGB,
	RAW_OUTPUT_FMT_GRAY,
 } RAW_OUTPUT_FMT;
 /*
 MMAL_ENCODING_I420
 MMAL_ENCODING_BGR24
 MMAL_ENCODING_RGB24
 */
class RawPort {
	PORT_USERDATA callbackData;
	int width;
	int height;
public:
	MMAL_PORT_T *port;
	MMAL_POOL_T *pool;
	void (*callback)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) = NULL;
	RAW_OUTPUT_FMT raw_output_fmt;
	RawPort(int width, int height);
    ~RawPort();
	void open();
	void setPortFormat(MMAL_PORT_T * port);
	void checkDisable();
    void close();
    void connect(MMAL_PORT_T * port);
	void enable();
};


#endif