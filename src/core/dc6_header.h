#ifndef _DC6_HEADER_H_
#define _DC6_HEADER_H_

#define DC6_HEADER_MIN_BYTES 24

// Inspect a DC6 file's header bytes to count its frames. The header
// layout starts with version/flags/encoding/termination (16 bytes), then
// `directions` (4 bytes) and `frames_per_direction` (4 bytes). Total
// frame count is the product. A "single-frame" DC6 means
// directions == 1 AND frames_per_direction == 1, which is the static
// inventory-icon shape we want to keep when exporting to PNG.
//
// Returns:
//    1 if the file is single-frame
//    0 if multi-frame
//   -1 if the header is too short or values look invalid
//
// Pure inspection of a byte buffer; no file I/O. Intended to be called
// after reading the first DC6_HEADER_MIN_BYTES bytes from a file.
int dc6_header_is_single_frame(const void *bytes, long len);

#endif
