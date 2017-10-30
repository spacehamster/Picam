#ifndef RESIZER_H
#define RESIZER_H

#include "mmalincludes.h"
#include "vid_util.h"
#include "raw.h"

class ResizePort {
	PORT_USERDATA callbackData;
public:
	enum ResizeMode { ISP, VPU };
	MMAL_COMPONENT_T *resize_component = NULL;
	MMAL_PORT_T *resize_input_port = NULL;
	MMAL_PORT_T *resize_output_port = NULL;
	MMAL_CONNECTION_T *resize_connection = NULL;
	MMAL_POOL_T * resize_pool;
	ResizeMode resizeMode = ISP;
	RAW_OUTPUT_FMT rawFormat = RAW_OUTPUT_FMT_YUV;
	bool wantCallback = true;
	void (*callback)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) = NULL;
	int width;
	int height;
	ResizePort(int width, int height);
	~ResizePort();
	void open(MMAL_PORT_T* port);
	void setPortFormat(MMAL_PORT_T* port);
	void checkDisable();
	void close();
	void connect(MMAL_PORT_T * port);
	void enable();
};


#endif