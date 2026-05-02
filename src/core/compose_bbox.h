#ifndef _COMPOSE_BBOX_H_
#define _COMPOSE_BBOX_H_

// Bounding-box union helper for the composer.
//
// Each DCC layer in a composed (direction, frame) tuple has its own
// bounding box (per-direction xmin/ymin/width/height read from the
// DCC). To produce a single composed APNG frame, all layers must
// blit into a common canvas. This module computes:
//   1. The canvas size: union of all layer rects.
//   2. Per-layer blit position: where in the canvas each layer goes.
//
// The math is pure -- no Allegro, no global state. Used by the
// upcoming composer driver to size the output bitmap and place
// each layer at the right pixel coordinates.

typedef struct COMPOSE_LAYER_RECT_S
{
   int offset_x;
   int offset_y;
   int width;
   int height;
} COMPOSE_LAYER_RECT_S;

// Compute the union bounding box of `rect_count` layer rectangles.
// Zero-sized rects (width<=0 or height<=0) are silently skipped.
//
//   rects             input layer rectangles in their natural
//                     coordinate system (D2 DCC offset_x/offset_y +
//                     bitmap width/height)
//   rect_count        number of rectangles
//   canvas_w/h        output: canvas dimensions
//   layer_blit_x/y    output: blit position of each layer within the
//                     canvas (canvas_relative_x = offset_x - min_x).
//                     Caller-allocated arrays of length rect_count.
//                     Entries for skipped (zero-sized) layers receive
//                     -1.
//
// Returns 1 on success, 0 if all rects are skipped (canvas is empty)
// or arguments are invalid.
int compose_bbox_union(const COMPOSE_LAYER_RECT_S *rects, int rect_count,
                       int *canvas_w, int *canvas_h,
                       int *layer_blit_x, int *layer_blit_y);

#endif
