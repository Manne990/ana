#ifndef BYTE_BROTHERS_RENDER_H
#define BYTE_BROTHERS_RENDER_H

/* Rendering entry points for the Byte Brothers platform sample. */

void bb_render_reset(void);
void bb_render_invalidate(void);
void bb_render_tile_changed(int tx, int ty);
void bb_render_draw(void);
void bb_render_shutdown(void);

#endif
