#include "tex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
TexPort::TexPort(int width, int height){

}
TexPort::~TexPort(){
	close();
}
void TexPort::open(){
	vcos_init();

	vcos_log_register("RaspiTex", VCOS_LOG_CATEGORY);
	vcos_log_set_level(VCOS_LOG_CATEGORY, false ? VCOS_LOG_INFO : VCOS_LOG_WARN); //TODO fix
	vcos_log_trace("%s", VCOS_FUNCTION);
}
void TexPort::stop()
{
}
void TexPort::close(){
	vcos_log_trace("%s", VCOS_FUNCTION);
	if (this->pool)
	{
		mmal_pool_destroy(this->pool);
		this->pool = NULL;
	}
}

static void tex_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer){
	PORT_USERDATA * state = (PORT_USERDATA*) port->userdata;
	if(state->userCallback)	state->userCallback(port, buffer);
}
void TexPort::connect(MMAL_PORT_T * port){
	MMAL_STATUS_T status;
	vcos_log_trace("%s port %p", VCOS_FUNCTION, port);

	/* Enable ZERO_COPY mode on the preview port which instructs MMAL to only
	 * pass the 4-byte opaque buffer handle instead of the contents of the opaque
	 * buffer.
	 * The opaque handle is resolved on VideoCore by the GL driver when the EGL
	 * image is created.
	 */
	status = mmal_port_parameter_set_boolean(port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
	if (status != MMAL_SUCCESS) throw CameraException("Failed to enable zero copy on camera preview port");

	status = mmal_port_format_commit(port);
	if (status != MMAL_SUCCESS) throw CameraException("camera viewfinder format couldn't be set");

	/* For GL a pool of opaque buffer handles must be allocated in the client.
	 * These buffers are used to create the EGL images.
	 */
	this->port = port;
	port->buffer_num = port->buffer_num_recommended;
	port->buffer_size = port->buffer_size_recommended;

	vcos_log_trace("Creating buffer pool for GL renderer num %d size %d", port->buffer_num, port->buffer_size);

	/* Pool + queue to hold preview frames */
	this->pool = mmal_port_pool_create(port, port->buffer_num, port->buffer_size);
	if (! this->pool) throw CameraException(MMAL_ENOMEM, "Error allocating pool");

	/* Enable preview port callback */
	callbackData.pool = this->pool;
	callbackData.userCallback = callback;
	port->userdata = (MMAL_PORT_USERDATA_T*) &callbackData;
	status = mmal_port_enable(port, tex_callback);
	if (status != MMAL_SUCCESS) throw CameraException("Failed to camera preview port");
}
void TexPort::enable(){

}
void TexPort::capture(){
	//TODO maby get resource?
}