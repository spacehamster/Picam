/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "gl_scene.h"
#include "picam/vid_util.h"

//-----------------------------------------------------
//Square.c
//-----------------------------------------------------
/* Vertex co-ordinates:
 *
 * v0----v1
 * |     |
 * |     |
 * |     |
 * v3----v2
 */

static const GLfloat vertices[] =
{
#define V0  -0.8,  0.8,  0.8,
#define V1   0.8,  0.8,  0.8,
#define V2   0.8, -0.8,  0.8,
#define V3  -0.8, -0.8,  0.8,
   V0 V3 V2 V2 V1 V0
};

/* Texture co-ordinates:
 *
 * (0,0) b--c
 *       |  |
 *       a--d
 *
 * b,a,d d,c,b
 */
static const GLfloat tex_coords[] =
{
   0, 0, 0, 1, 1, 1,
   1, 1, 1, 0, 0, 0
};

static GLfloat angle;
static uint32_t anim_step;

static int square_init(RASPITEX_STATE *state) {
   int rc = raspitexutil_gl_init_1_0(state);

   if (rc != 0)
      goto end;

   angle = 0.0f;
   anim_step = 0;

   glClearColor(0, 0, 0, 0);
   glClearDepthf(1);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();

end:
   return rc;
}

static int square_update_model(RASPITEX_STATE *state)
{
   int frames_per_rev = 30 * 15;
   angle = (anim_step * 360) / (GLfloat) frames_per_rev;
   anim_step = (anim_step + 1) % frames_per_rev;

   return 0;
}

static int square_redraw(RASPITEX_STATE *state)
{
   /* Bind the OES texture which is used to render the camera preview */
   GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
   glLoadIdentity();
   glRotatef(angle, 0.0, 0.0, 1.0);
   glEnableClientState(GL_VERTEX_ARRAY);
   glVertexPointer(3, GL_FLOAT, 0, vertices);
   glDisableClientState(GL_COLOR_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);
   GLCHK(glDrawArrays(GL_TRIANGLES, 0, vcos_countof(tex_coords) / 2));
   return 0;
}

int square_open(RASPITEX_STATE *state)
{
   state->ops.gl_init = square_init;
   state->ops.update_model = square_update_model;
   state->ops.redraw = square_redraw;
   state->ops.update_texture = raspitexutil_update_texture;
   
   return 0;
}
//-----------------------------------------------------
//Raspitex_util.c
//-----------------------------------------------------
VCOS_LOG_CAT_T raspitex_log_category;

/**
 * \file RaspiTexUtil.c
 *
 * Provides default implementations for the raspitex_scene_ops functions
 * and general utility functions.
 */

/**
 * Deletes textures and EGL surfaces and context.
 * @param   raspitex_state  Pointer to the Raspi
 */
void raspitexutil_gl_term(RASPITEX_STATE *raspitex_state)
{
   vcos_log_trace("%s", VCOS_FUNCTION);

   /* Delete OES textures */
   glDeleteTextures(1, &raspitex_state->texture);
   eglDestroyImageKHR(raspitex_state->display, raspitex_state->egl_image);
   raspitex_state->egl_image = EGL_NO_IMAGE_KHR;

   /* Terminate EGL */
   eglMakeCurrent(raspitex_state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(raspitex_state->display, raspitex_state->context);
   eglDestroySurface(raspitex_state->display, raspitex_state->surface);
   eglTerminate(raspitex_state->display);
}

/** Creates a native window for the GL surface using dispmanx
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful, otherwise, -1 is returned.
 */
int raspitexutil_create_native_window(RASPITEX_STATE *raspitex_state)
{
   VC_DISPMANX_ALPHA_T alpha = {DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0};
   VC_RECT_T src_rect = {0};
   VC_RECT_T dest_rect = {0};
   uint32_t disp_num = 0; // Primary
   uint32_t layer_num = 0;
   DISPMANX_ELEMENT_HANDLE_T elem;
   DISPMANX_UPDATE_HANDLE_T update;

   alpha.opacity = raspitex_state->opacity;
   dest_rect.x = raspitex_state->x;
   dest_rect.y = raspitex_state->y;
   dest_rect.width = raspitex_state->width;
   dest_rect.height = raspitex_state->height;

   vcos_log_trace("%s: %d,%d,%d,%d %d,%d,0x%x,0x%x", VCOS_FUNCTION,
         src_rect.x, src_rect.y, src_rect.width, src_rect.height,
         dest_rect.x, dest_rect.y, dest_rect.width, dest_rect.height);

   src_rect.width = dest_rect.width << 16;
   src_rect.height = dest_rect.height << 16;

   raspitex_state->disp = vc_dispmanx_display_open(disp_num);
   if (raspitex_state->disp == DISPMANX_NO_HANDLE)
   {
      vcos_log_error("Failed to open display handle");
      goto error;
   }

   update = vc_dispmanx_update_start(0);
   if (update == DISPMANX_NO_HANDLE)
   {
      vcos_log_error("Failed to open update handle");
      goto error;
   }

   elem = vc_dispmanx_element_add(update, raspitex_state->disp, layer_num,
         &dest_rect, 0, &src_rect, DISPMANX_PROTECTION_NONE, &alpha, NULL,
         DISPMANX_NO_ROTATE);
   if (elem == DISPMANX_NO_HANDLE)
   {
      vcos_log_error("Failed to create element handle");
      goto error;
   }

   raspitex_state->win.element = elem;
   raspitex_state->win.width = raspitex_state->width;
   raspitex_state->win.height = raspitex_state->height;
   vc_dispmanx_update_submit_sync(update);

   raspitex_state->native_window = (EGLNativeWindowType*) &raspitex_state->win;

   return 0;
error:
   return -1;
}

/** Destroys the pools of buffers used by the GL renderer.
 * @param raspitex_state A pointer to the GL preview state.
 */
void raspitexutil_destroy_native_window(RASPITEX_STATE *raspitex_state)
{
   vcos_log_trace("%s", VCOS_FUNCTION);
   if (raspitex_state->disp != DISPMANX_NO_HANDLE) {
      vc_dispmanx_display_close(raspitex_state->disp);
      raspitex_state->disp = DISPMANX_NO_HANDLE;
   }
}

/** Creates the EGL context and window surface for the native window
 * using specified arguments.
 * @param raspitex_state  A pointer to the GL preview state. This contains
 *                        the native_window pointer.
 * @param attribs         The config attributes.
 * @param context_attribs The context attributes.
 * @return Zero if successful.
 */
static int raspitexutil_gl_common(RASPITEX_STATE *raspitex_state, const EGLint attribs[], const EGLint context_attribs[])
{
   EGLConfig config;
   EGLint num_configs;

   vcos_log_trace("%s", VCOS_FUNCTION);

   if (raspitex_state->native_window == NULL) {
      vcos_log_error("%s: No native window", VCOS_FUNCTION);
      goto error;
   }

   raspitex_state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (raspitex_state->display == EGL_NO_DISPLAY) {
      vcos_log_error("%s: Failed to get EGL display", VCOS_FUNCTION);
      goto error;
   }

   if (! eglInitialize(raspitex_state->display, 0, 0)) {
      vcos_log_error("%s: eglInitialize failed", VCOS_FUNCTION);
      goto error;
   }

   if (! eglChooseConfig(raspitex_state->display, attribs, &config, 1, &num_configs)) {
      vcos_log_error("%s: eglChooseConfig failed", VCOS_FUNCTION);
      goto error;
   }

   raspitex_state->surface = eglCreateWindowSurface(raspitex_state->display, config, raspitex_state->native_window, NULL);
   if (raspitex_state->surface == EGL_NO_SURFACE){
      vcos_log_error("%s: eglCreateWindowSurface failed", VCOS_FUNCTION);
      goto error;
   }

   raspitex_state->context = eglCreateContext(raspitex_state->display, config, EGL_NO_CONTEXT, context_attribs);
   if (raspitex_state->context == EGL_NO_CONTEXT) {
      vcos_log_error("%s: eglCreateContext failed", VCOS_FUNCTION);
      goto error;
   }

   if (!eglMakeCurrent(raspitex_state->display, raspitex_state->surface, raspitex_state->surface, raspitex_state->context)) {
      vcos_log_error("%s: Failed to activate EGL context", VCOS_FUNCTION);
      goto error;
   }

   return 0;

error:
   vcos_log_error("%s: EGL error 0x%08x", VCOS_FUNCTION, eglGetError());
   raspitex_state->ops.gl_term(raspitex_state);
   return -1;
}

/* Creates the RGBA and luma textures with some default parameters
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
int raspitexutil_create_textures(RASPITEX_STATE *raspitex_state){
   GLCHK(glGenTextures(1, &raspitex_state->texture));
   return 0;
}

/**
 * Creates an OpenGL ES 1.X context.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
int raspitexutil_gl_init_1_0(RASPITEX_STATE *raspitex_state)
{
   int rc;
   const EGLint* attribs = raspitex_state->egl_config_attribs;
   const EGLint default_attribs[] =
   {
      EGL_RED_SIZE,   8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE,  8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
      EGL_NONE
   };

   const EGLint context_attribs[] =
   {
      EGL_CONTEXT_CLIENT_VERSION, 1,
      EGL_NONE
   };

   if (! attribs)
      attribs = default_attribs;

   rc = raspitexutil_gl_common(raspitex_state, attribs, context_attribs);
   if (rc != 0) return rc;

   GLCHK(glEnable(GL_TEXTURE_EXTERNAL_OES));
   rc = raspitexutil_create_textures(raspitex_state);

   return rc;
}
/**
 * Advances the texture and EGL image to the next MMAL buffer.
 *
 * @param display The EGL display.
 * @param target The EGL image target e.g. EGL_IMAGE_BRCM_MULTIMEDIA
 * @param mm_buf The EGL client buffer (mmal opaque buffer) that is used to
 * create the EGL Image for the preview texture.
 * @param egl_image Pointer to the EGL image to update with mm_buf.
 * @param texture Pointer to the texture to update from EGL image.
 * @return Zero if successful.
 */
int raspitexutil_do_update_texture(EGLDisplay display, EGLenum target, EGLClientBuffer mm_buf, GLuint *texture, EGLImageKHR *egl_image) {
   vcos_log_trace("%s: mm_buf %u", VCOS_FUNCTION, (unsigned) mm_buf);
   GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, *texture));
   if (*egl_image != EGL_NO_IMAGE_KHR) {
      /* Discard the EGL image for the preview frame */
      eglDestroyImageKHR(display, *egl_image);
      *egl_image = EGL_NO_IMAGE_KHR;
   }

   *egl_image = eglCreateImageKHR(display, EGL_NO_CONTEXT, target, mm_buf, NULL);
   GLCHK(glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, *egl_image));

   return 0;
}

/**
 * Updates the RGBX texture to the specified MMAL buffer.
 * @param raspitex_state A pointer to the GL preview state.
 * @param mm_buf The MMAL buffer.
 * @return Zero if successful.
 */
int raspitexutil_update_texture(RASPITEX_STATE *raspitex_state, EGLClientBuffer mm_buf) {
   return raspitexutil_do_update_texture(raspitex_state->display, EGL_IMAGE_BRCM_MULTIMEDIA, mm_buf, &raspitex_state->texture, &raspitex_state->egl_image);
}

/**
 * Default is a no-op
 * @param raspitex_state A pointer to the GL preview state.
 */
void raspitexutil_close(RASPITEX_STATE* raspitex_state)
{
   (void) raspitex_state;
}
//-----------------------------------------------------
//Raspitex_util.c
//-----------------------------------------------------

/**
 * Starts the worker / GL renderer thread.
 * @pre raspitex_init was successful
 * @pre raspitex_configure_preview_port was successful
 * @param state Pointer to the GL preview state.
 * @return Zero on success, otherwise, -1 is returned
 * */
int raspitex_start(RASPITEX_STATE *state)
{
   vcos_log_trace("%s", VCOS_FUNCTION);
   VCOS_STATUS_T status = vcos_thread_create(&state->preview_thread, "preview-worker", NULL, preview_worker, state);
   if (status != VCOS_SUCCESS) vcos_log_error("%s: Failed to start worker thread %d", VCOS_FUNCTION, status);
   return (status == VCOS_SUCCESS ? 0 : -1);
}
/* Stops the rendering loop and destroys MMAL resources
 * @param state  Pointer to the GL preview state.
 */
void raspitex_stop(RASPITEX_STATE *state)
{
   if (! state->preview_stop)
   {
      vcos_log_trace("Stopping GL preview");
      state->preview_stop = 1;
      vcos_thread_join(&state->preview_thread, NULL);
   }
}
/** Preview worker thread.
 * Ensures camera preview is supplied with buffers and sends preview frames to GL.
 * @param arg  Pointer to state.
 * @return NULL always.
 */
static void *preview_worker(void *arg)
{
   RASPITEX_STATE* state = (RASPITEX_STATE*) arg;
   MMAL_PORT_T *preview_port = state->preview_port;
   MMAL_BUFFER_HEADER_T *buf;
   MMAL_STATUS_T st;
   int rc;

   vcos_log_trace("%s: port %p", VCOS_FUNCTION, preview_port);

   rc = state->ops.create_native_window(state);
   if (rc != 0)
      goto end;

   rc = state->ops.gl_init(state);
   if (rc != 0)
      goto end;

   while (state->preview_stop == 0)
   {
      /* Send empty buffers to camera preview port */
      while ((buf = mmal_queue_get(state->preview_pool->queue)) != NULL)
      {
         st = mmal_port_send_buffer(preview_port, buf);
         if (st != MMAL_SUCCESS)
         {
            vcos_log_error("Failed to send buffer to %s", preview_port->name);
         }
      }
      /* Process returned buffers */
      if (preview_process_returned_bufs(state) != 0)
      {
         vcos_log_error("Preview error. Exiting.");
         state->preview_stop = 1;
      }
   }

end:
   /* Make sure all buffers are returned on exit */
   while ((buf = mmal_queue_get(state->preview_queue)) != NULL)
      mmal_buffer_header_release(buf);

   /* Tear down GL */
   state->ops.gl_term(state);
   vcos_log_trace("Exiting preview worker");
   return NULL;
}
/**
 * Process preview buffers.
 *
 * Dequeue each available preview buffer in order and call current redraw
 * function. If no new buffers are available then the render function is
 * invoked anyway.
 * @param   state The GL preview window state.
 * @return Zero if successful.
 */
static int preview_process_returned_bufs(RASPITEX_STATE* state)
{
   MMAL_BUFFER_HEADER_T *buf;
   int new_frame = 0;
   int rc = 0;

   while ((buf = mmal_queue_get(state->preview_queue)) != NULL)
   {
      if (state->preview_stop == 0)
      {
         new_frame = 1;
         rc = raspitex_draw(state, buf);
         if (rc != 0)
         {
            vcos_log_error("%s: Error drawing frame. Stopping.", VCOS_FUNCTION);
            state->preview_stop = 1;
            return rc;
         }
      }
   }

   /* If there were no new frames then redraw the scene again with the previous
    * texture. Otherwise, go round the loop again to see if any new buffers
    * are returned.
    */
   if (! new_frame)
      rc = raspitex_draw(state, NULL);
   return rc;
}
/**
 * Draws the next preview frame. If a new preview buffer is available then the
 * preview texture is updated first.
 *
 * @param state RASPITEX STATE
 * @param video_frame MMAL buffer header containing the opaque buffer handle.
 * @return Zero if successful.
 */
static int raspitex_draw(RASPITEX_STATE *state, MMAL_BUFFER_HEADER_T *buf)
{
   int rc = 0;

   /* If buf is non-NULL then there is a new viewfinder frame available
    * from the camera so the texture should be updated.
    *
    * Although it's possible to have multiple textures mapped to different
    * viewfinder frames this can consume a lot of GPU memory for high-resolution
    * viewfinders.
    */
   if (buf)
   {
      /* Update the texture to the new viewfinder image which should */
      if (state->ops.update_texture)
      {
         rc = state->ops.update_texture(state, (EGLClientBuffer) buf->data);
         if (rc != 0)
         {
            vcos_log_error("%s: Failed to update RGBX texture %d",
                  VCOS_FUNCTION, rc);
            goto end;
         }
      }

      /* Now return the PREVIOUS MMAL buffer header back to the camera preview. */
      if (state->preview_buf)
         mmal_buffer_header_release(state->preview_buf);

      state->preview_buf = buf;
   }

   /*  Do the drawing */
   if (check_egl_image(state) == 0)
   {
      rc = state->ops.update_model(state);
      if (rc != 0)
         goto end;

      rc = state->ops.redraw(state);
      if (rc != 0)
         goto end;

		 eglSwapBuffers(state->display, state->surface);
   }
   else
   {
      // vcos_log_trace("%s: No preview image", VCOS_FUNCTION);
   }

end:
   return rc;
}

/**
 * Checks if there is at least one valid EGL image.
 * @param state RASPITEX STATE
 * @return Zero if successful.
 */
static int check_egl_image(RASPITEX_STATE *state)
{
   if (state->egl_image == EGL_NO_IMAGE_KHR)
      return -1;
   else
      return 0;
}
RASPITEX_STATE raspitex_state;
int gl_frame_count = 0;

int gl_start(MMAL_PORT_T * port, MMAL_POOL_T * pool){
	gl_frame_count = 0;
   memset(&raspitex_state, 0, sizeof(raspitex_state));
   raspitex_state.display = EGL_NO_DISPLAY;
   raspitex_state.surface = EGL_NO_SURFACE;
   raspitex_state.context = EGL_NO_CONTEXT;
   raspitex_state.egl_image = EGL_NO_IMAGE_KHR;
   raspitex_state.opacity = 255;
   raspitex_state.width = 640;
   raspitex_state.height = 480;
   
   //state->ops.gl_init = raspitexutil_gl_init_1_0;
   //state->ops.update_model = raspitexutil_update_model;
   //state->ops.redraw = raspitexutil_redraw;
   //Squrare
   raspitex_state.ops.gl_init = square_init;
   raspitex_state.ops.update_model = square_update_model;
   raspitex_state.ops.redraw = square_redraw;
   raspitex_state.ops.update_texture = raspitexutil_update_texture;
   
   raspitex_state.ops.create_native_window = raspitexutil_create_native_window;
   raspitex_state.ops.gl_term = raspitexutil_gl_term;
   raspitex_state.ops.destroy_native_window = raspitexutil_destroy_native_window;
   raspitex_state.ops.close = raspitexutil_close;
   
   raspitex_state.preview_port = port;          /// Source port for preview opaque buffers
   raspitex_state.preview_pool = pool;          /// Pool for storing opaque buffer handles
   raspitex_state.preview_queue = NULL;        /// Queue preview buffers to display in order
   
   raspitex_state.preview_queue = mmal_queue_create();
   if (! raspitex_state.preview_queue) vcos_log_error("Error allocating queue");
   
   //Raspitex_init
   vcos_init();

   vcos_log_register("RaspiTex", VCOS_LOG_CATEGORY);
   vcos_log_set_level(VCOS_LOG_CATEGORY, raspitex_state.verbose ? VCOS_LOG_INFO : VCOS_LOG_WARN);
   vcos_log_trace("%s", VCOS_FUNCTION);
   
   raspitex_start(&raspitex_state);
}
void gl_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer){
	gl_frame_count++;
	RASPITEX_STATE *state = &raspitex_state;
	PORT_USERDATA * userdata = (PORT_USERDATA*) port->userdata;
	//state->preview_port = port;
	//state->preview_pool = userdata->pool;
	
	if (buffer->length == 0)
	{
		vcos_log_trace("%s: zero-length buffer => EOS", port->name);
		state->preview_stop = 1;
		mmal_buffer_header_release(buffer);
	}
	else if (buffer->data == NULL)
	{
		vcos_log_trace("%s: zero buffer handle", port->name);
		mmal_buffer_header_release(buffer);
	}
	else {
		// Enqueue the preview frame for rendering and return to avoid blocking MMAL core.
		mmal_queue_put(state->preview_queue, buffer);
	}
}
void gl_stop(){
	printf("gl_frame_count %d\n", gl_frame_count);
	raspitex_stop(&raspitex_state);
	if (raspitex_state.preview_queue) {
		mmal_queue_destroy(raspitex_state.preview_queue);
		raspitex_state.preview_queue = NULL;
	}
}

