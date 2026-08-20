# Lighting and light falloff

How the editor produces the effect of a bright pool around a light source that
fades to black with distance — the "Night preview" toy in Paul Siramy's
original, reached with the **L** key.

Everything here lives in [`src/render/preview.c`](../../src/render/preview.c).

> **Read the last section before relying on this.** The falloff *maths* is
> intact, but the Allegro 5 port lost the smooth gradient it used to feed. What
> ships today is a three-step approximation, not a continuous fade.

---

## The model

Light mode (`MOD_L`) treats **the mouse cursor as a single point light**. There
is no light entity in the DS1 and nothing is stored — move the cursor and the
lighting follows it. That is why it was always described as a toy: it previews
what a lit scene looks like rather than editing lighting data.

Each tile is lit by sampling brightness at its **four corners**, then shading
across the tile between those four values. The corners are, in order:

```
        c1 ──────── c2          c1 = (mx,                my)
         │           │          c2 = (mx + tile_width,   my)
         │   tile    │          c3 = (mx + tile_width,   my + tile_h)
         │           │          c4 = (mx,                my + tile_h)
        c4 ──────── c3
```

Sampling per corner rather than per tile is what makes the pool look round
rather than blocky: a tile straddling the edge of the light gets a bright
corner and a dark one, and the shading between them hides the tile boundary.

---

## Distance, and why Y is doubled

```c
int wpreview_light_dist(int x0, int y0, int mx, int my)
{
   double dx, dy;

   dx = mx - x0;
   dx *= dx;

   dy = (my - y0) * 2;      /* <- the important bit */
   dy *= dy;

   return ceil(sqrt(dx + dy));
}
```

This is an ordinary Euclidean distance with **the vertical difference doubled
before squaring**.

Diablo II draws in a 2:1 isometric projection: a tile is 160 pixels wide and 80
tall, so one pixel of screen Y covers **twice** the world distance of one pixel
of screen X. Measuring in raw screen pixels would make the light pool an ellipse
— wide and squashed — because the same world distance costs half as many pixels
vertically.

Doubling Y converts screen space to world-proportional space, so the falloff is
circular *on the ground plane*, which is what the eye expects:

```
   raw screen distance          Y doubled
   ┌─────────────────┐       ┌─────────────────┐
   │    ▄▄███████▄▄  │       │      ▄█████▄    │
   │  ███████████████│       │    █████████    │
   │    ▀▀███████▀▀  │       │      ▀█████▀    │
   └─────────────────┘       └─────────────────┘
    ellipse (wrong)           circle on the ground (right)
```

---

## The falloff curve

Distance becomes a 0–255 intensity through a piecewise-linear ramp with two
radii — an inner **plateau** and an outer **cutoff**:

```c
if (dist < 200)       c = 255;                                  /* full bright */
else if (dist > 500)  c = 0;                                    /* black       */
else                  c = 255 - ((dist - 200) * 255 / (500 - 200));
```

```
 255 ┤██████████████
     │              ╲
     │               ╲
 128 ┤                ╲
     │                 ╲
     │                  ╲
   0 ┤                   ╲████████████████
     └────┬───────┬───────┬───────┬────────
          0      200     500    distance (world-corrected px)
             plateau   ramp        cutoff
```

Three things follow from this shape:

- **Inside 200px everything is equally bright.** There is no hot spot at the
  exact cursor position; the light has a flat core, which reads as a lamp with
  a physical size rather than a mathematical point.
- **The ramp is linear, not inverse-square.** Real light falls off as 1/d²,
  which would plunge to near-black almost immediately and leave a tiny bright
  dot. The linear ramp across a wide 300px band is the stylistic choice that
  makes the pool large and readable.
- **Past 500px tiles are not merely dark, they are skipped.** The renderer
  returns before drawing (see below), so the cost of light mode falls away with
  distance instead of drawing thousands of black tiles.

Both constants are hardcoded, and appear at eight sites in `preview.c` (four
corners across the floor and wall passes). Changing the pool size means changing
all eight.

---

## Applying the intensity

The four corner values go to `wpreview_gouraud_f` for floors and
`wpreview_gouraud_w` for walls:

```c
wpreview_gouraud_f(tmp_bmp, x, y, ds1_idx, c1, c2, c3, c4);
```

In the original Allegro 4 build these fed `draw_gouraud_sprite()`, which
interpolated brightness across the tile per pixel — a genuine smooth gradient,
and the reason the effect looked like light rather than like tinted squares.

---

## What actually happens today

Allegro 5 has no `draw_gouraud_sprite`, and the port did not replace it. The
current implementation is:

```c
void wpreview_gouraud_f(ALLEGRO_BITMAP *tmp_bmp, int x0, int y0, int ds1_idx,
                        int c1, int c2, int c3, int c4)
{
   if ((c1 == c2) && (c2 == c3) && (c3 == c4))
   {
      if (c1 <= 7)   return;                                        /* skip  */
      if (c1 >= 248) wpreview_draw_bitmap(tmp_bmp, x0, y0);         /* solid */
      else           wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
   }
   else
   {
      wpreview_draw_trans_bitmap(tmp_bmp, x0, y0, a5_trans_alpha);
   }
}
```

`a5_trans_alpha` is a global that defaults to `0.5f`
([`src/globals.c`](../../src/globals.c)) and is **not** derived from `c1..c4`.
So the corner intensities, carefully computed four times per tile, are used
only to pick between three outcomes:

| Corner values | Result |
|---|---|
| all ≤ 7 | tile not drawn (black) |
| all ≥ 248 | tile drawn at full brightness |
| anything else | tile drawn at a flat 50% alpha |

The gradient is gone. Instead of a smooth pool there is a bright disc, a
uniform half-lit annulus, and darkness — with visible steps at the 200px and
500px boundaries. The wall variant (`wpreview_gouraud_w`) is the same minus the
`<= 7` skip, so distant walls draw at 50% rather than vanishing.

**`night_mode` is also inert.** The **N** key cycles
`glb_ds1edit.night_mode` between 0 and 1, and the only thing that reads it is
the status-bar label at `preview.c:3165`, which shows "Night 1" or "Night 2".
Neither value changes any rendering.

---

## Restoring the gradient

Allegro 5 can do this; it just needs different primitives. In rough order of
effort:

1. **`al_draw_prim` with per-vertex colour.** Build an `ALLEGRO_VERTEX[4]` for
   the tile quad, set each vertex's colour to `al_map_rgb(c, c, c)` from the
   matching corner, and draw with `ALLEGRO_PRIM_TRIANGLE_FAN` against the tile
   bitmap as texture. The GPU interpolates exactly as `draw_gouraud_sprite` did.
   This is the direct replacement and needs no shader.

2. **A fragment shader** taking the four corner intensities as uniforms. More
   flexible — an inverse-square or smoothstep falloff becomes a one-line change
   — but pulls in shader source and a GLSL/HLSL path per backend.

3. **Per-tile tint only**, via `al_draw_tinted_bitmap` with the average of the
   four corners. Cheapest, keeps tiles visibly quantised, but at least restores
   a continuous *range* of brightness instead of three buckets.

Option 1 is the one that matches the original behaviour. Whichever is chosen,
the corner sampling and the falloff curve above stay as they are — they were
never the broken part.
