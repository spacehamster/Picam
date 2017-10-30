#ifndef GL_SCENE_H
#define GL_SCENE_H
#define VCOS_LOG_CATEGORY (&raspitex_log_category)
#include <stdio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "EGL/eglext_brcm.h"
#include "interface/mmal/mmal.h"
#include "interface/vcos/vcos.h"
extern VCOS_LOG_CAT_T raspitex_log_category;

/* Uncomment to enable extra GL error checking */
//#define CHECK_GL_ERRORS
#if defined(CHECK_GL_ERRORS)
#define GLCHK(X) \
do { \
    GLenum err = GL_NO_ERROR; \
    X; \
   while ((err = glGetError())) \
   { \
      vcos_log_error("GL error 0x%x in " #X "file %s line %d", err, __FILE__,__LINE__); \
      vcos_assert(err == GL_NO_ERROR); \
      exit(err); \
   } \
} \
while(0)
#else
#define GLCHK(X) X
#endif /* CHECK_GL_ERRORS */


struct RASPITEX_STATE;
typedef struct RASPITEX_SCENE_OPS
{
   int (*create_native_window)(struct RASPITEX_STATE *state);
   int (*gl_init)(struct RASPITEX_STATE *state);
   int (*update_texture)(struct RASPITEX_STATE *state, EGLClientBuffer mm_buf);
   int (*update_model)(struct RASPITEX_STATE *state);
   int (*redraw)(struct RASPITEX_STATE *state);
   void (*gl_term)(struct RASPITEX_STATE *state);
   void (*destroy_native_window)(struct RASPITEX_STATE *state);
   void (*close)(struct RASPITEX_STATE *state);
} RASPITEX_SCENE_OPS;

/**
 * Contains the internal state and configuration for the GL rendered
 * preview window.
 */
typedef struct RASPITEX_STATE
{
   MMAL_PORT_T *preview_port;          /// Source port for preview opaque buffers
   MMAL_POOL_T *preview_pool;          /// Pool for storing opaque buffer handles
   MMAL_QUEUE_T *preview_queue;        /// Queue preview buffers to display in order
   VCOS_THREAD_T preview_thread;       /// Preview worker / GL rendering thread
   uint32_t preview_stop;              /// If zero the worker can continue

   /* Copy of preview window params */
   int32_t preview_x;                  /// x-offset of preview window
   int32_t preview_y;                  /// y-offset of preview window
   int32_t preview_width;              /// preview y-plane width in pixels
   int32_t preview_height;             /// preview y-plane height in pixels

   /* Display rectangle for the native window */
   int32_t x;                          /// x-offset in pixels
   int32_t y;                          /// y-offset in pixels
   int32_t width;                      /// width in pixels
   int32_t height;                     /// height in pixels
   int opacity;                        /// Alpha value for display element
   int gl_win_defined;                 /// Use rect from --glwin instead of preview

   /* DispmanX info. This might be unused if a custom create_native_window
    * does something else. */
   DISPMANX_DISPLAY_HANDLE_T disp;     /// Dispmanx display for GL preview
   EGL_DISPMANX_WINDOW_T win;          /// Dispmanx handle for preview surface

   EGLNativeWindowType* native_window; /// Native window used for EGL surface
   EGLDisplay display;                 /// The current EGL display
   EGLSurface surface;                 /// The current EGL surface
   EGLContext context;                 /// The current EGL context
   const EGLint *egl_config_attribs;   /// GL scenes preferred EGL configuration

   GLuint texture;                     /// Name for the preview texture
   EGLImageKHR egl_image;              /// The current preview EGL image

  
   MMAL_BUFFER_HEADER_T *preview_buf;  /// MMAL buffer currently bound to texture(s)

   RASPITEX_SCENE_OPS ops;             /// The interface for the current scene
   int verbose;                        /// Log FPS

} RASPITEX_STATE;
//-----------------------------------------------------
//Square.c
//-----------------------------------------------------

static int square_init(RASPITEX_STATE *state);
static int square_update_model(RASPITEX_STATE *state);
static int square_redraw(RASPITEX_STATE *state);
int square_open(RASPITEX_STATE *state);
//-----------------------------------------------------
//Raspitex_util.c
//-----------------------------------------------------
void raspitexutil_gl_term(RASPITEX_STATE *raspitex_state);
int raspitexutil_create_native_window(RASPITEX_STATE *raspitex_state);
void raspitexutil_destroy_native_window(RASPITEX_STATE *raspitex_state);
static int raspitexutil_gl_common(RASPITEX_STATE *raspitex_state, const EGLint attribs[], const EGLint context_attribs[]);
int raspitexutil_create_textures(RASPITEX_STATE *raspitex_state);
int raspitexutil_gl_init_1_0(RASPITEX_STATE *raspitex_state);
int raspitexutil_do_update_texture(EGLDisplay display, EGLenum target, EGLClientBuffer mm_buf, GLuint *texture, EGLImageKHR *egl_image);
int raspitexutil_update_texture(RASPITEX_STATE *raspitex_state, EGLClientBuffer mm_buf);
void raspitexutil_close(RASPITEX_STATE* raspitex_state);
//Raspitex_util.c
int raspitex_start(RASPITEX_STATE *state);
void raspitex_stop(RASPITEX_STATE *state);
static void *preview_worker(void *arg);
static int preview_process_returned_bufs(RASPITEX_STATE* state);
static int raspitex_draw(RASPITEX_STATE *state, MMAL_BUFFER_HEADER_T *buf);
static int check_egl_image(RASPITEX_STATE *state);

//Custom
int gl_start(MMAL_PORT_T * port, MMAL_POOL_T * pool);
void gl_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
void gl_stop();

#endif