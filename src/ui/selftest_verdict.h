/* selftest_verdict.h -- the pass/fail rule --selftest applies to the frames it
 * timed. Kept apart from interface.c so it can be tested without a display.
 */
#ifndef DS1EDIT_SELFTEST_VERDICT_H
#define DS1EDIT_SELFTEST_VERDICT_H

/* A frame that costs the editor this much of its own time is a collapse, not
   a slow machine: a healthy frame composes in milliseconds. */
#define SELFTEST_RENDER_LIMIT_MS 250.0

/* CPU time the calling thread has used, in ms (0 when unavailable). Time
   spent waiting for a core is not counted. */
double selftest_thread_cpu_ms(void);

/* NULL when the run passes, otherwise the reason it fails.
 *
 * frames is the number of frames timed, cpu_ms_total the main thread's CPU
 * time spent drawing them, guard_hits the number of MEMORY bitmaps drawn onto
 * a VIDEO target.
 *
 * The time rule is on CPU time, not wall time. Wall time measures the host as
 * much as the editor: with parallel test lanes, act1_town at 1:8 used about
 * 50 ms of CPU per frame while its frames took 200 ms to several seconds of
 * wall time, and a mean over 250 ms failed healthy runs. A MEMORY bitmap drawn
 * every frame is the editor's own work and is caught directly by guard_hits;
 * the CPU rule backs that up for draws the guard cannot see. */
const char *selftest_render_verdict(int frames, double cpu_ms_total,
                                    unsigned guard_hits);

#endif
