#ifndef _GLOB_MATCH_H_
#define _GLOB_MATCH_H_

// Match a glob pattern against a path. Returns 1 on match, 0 otherwise.
//
// Wildcard syntax:
//   *   matches any run of characters except \  (matches the empty string)
//   ?   matches exactly one character except \
//   **  as a complete path component (surrounded by \ or at start/end)
//       matches any number of path components including zero.
//       Anywhere else (e.g. inv**.dc6) is treated as *.
//
// Matching is case-insensitive. Both pattern and path are normalized to
// backslash separators before comparison (forward slashes are accepted in
// both inputs as equivalent to backslash).
int glob_match(const char *pattern, const char *path);

#endif
