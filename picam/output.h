#ifndef OUTPUT_H
#define OUTPUT_H

#include <string.h>
#include "mmalincludes.h"
#include "vid_util.h"
class Image {
	public:
	unsigned char * data = NULL;
	unsigned int length = 0;
	Image(){ }
	~Image(){
		if(data) delete data;
	}
private:
	void set(unsigned char * _data, unsigned int _length){
		length = _length;
		data = new unsigned char[length];
		memcpy(data, _data, length);
	}
};
class CameraPort {
	void connect(MMAL_PORT_T * port){
	
	}
	public:
	void *(*callback)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T * );
	bool grab(Image * data){
		
	}
};
#endif