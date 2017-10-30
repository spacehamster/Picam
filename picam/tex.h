#ifndef EGL_H
#define EGL_H

#include <stdio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "EGL/eglext_brcm.h"
#include "interface/mmal/mmal.h"

#define RASPITEX_VERSION_MAJOR 1
#define RASPITEX_VERSION_MINOR 0

#include "mmalincludes.h"
#include "vid_util.h"
/*
usage
	raspitex_init(&state.raspitex_state);
	create_camera_component(&state){
		status = mmal_component_enable(camera);
		raspitex_configure_preview_port(&state->raspitex_state, preview_port);
	}
	raspitex_start(&state.raspitex_state)
	raspitex_stop(&state.raspitex_state);
	raspitex_destroy(&state.raspitex_state);
	
	
Tex tex;
tex.open();
tex.connect(preview_port);
text.start();
tex.stop();
tex.destroy();

*/
class TexPort {

	int32_t x       = 0;
	int32_t y       = 0;
	int32_t width   = 1024;
	int32_t height  = 768;
	/* Also pass the preview information through so GL renderer can determine
	 * the real resolution of the multi-media image */
	int32_t preview_x       = 0;
	int32_t preview_y       = 0;
	int32_t preview_width   = 1024;
	int32_t preview_height  = 768;
	int opacity         = 255;

public:
	MMAL_PORT_T * port;
	MMAL_POOL_T * pool;
	MMAL_QUEUE_T * queue;
	
	PORT_USERDATA callbackData;
    void (*callback)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) = NULL;
	TexPort(int width, int height);
	~TexPort();
	void open();
	void close();
	void stop();
	void connect(MMAL_PORT_T * port);
	void enable();
	void capture();
};


#endif