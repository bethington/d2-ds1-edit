#ifndef _DCC_INFO_H_

#define _DCC_INFO_H_


#define DCC_MAX_DIR    32
#define DCC_MAX_FRAME 256

#define DCC_MAX_PB_ENTRY 85000


char dcc_error[512]; // if an error is found while using a function of this
                     // dcc "library", this variable contain a string that
                     // explain what happened. You should display it somewhere
                     // to inform the user

int dcc_bits_width_table[16]; // will be initialized by dcc_decode()

typedef struct DCC_BOX_S
{
   long xmin;
   long ymin;
   long xmax;
   long ymax;
   long width;
   long height;
} DCC_BOX_S;

typedef struct DCC_CELL_S
{
   int    x0, y0;  // for frame cells in stage 2
   int    w, h;

   int    last_w, last_h;   // width & size of the last frame cell that used
                            // this buffer cell (for stage 2)
   int    last_x0, last_y0;
   
   BITMAP * bmp;   // sub-bitmap in the buffer bitmap
} DCC_CELL_S; // maybe I'll make 2 kind of cells, 1 for frame-buffer cells,
              // the other for the frame cells...

typedef struct DCC_FRAME_S
{
   UDWORD variable0;
   UDWORD width;
   UDWORD height;
   long   xoffset;
   long   yoffset;
   UDWORD optional_bytes;
   UDWORD coded_bytes;

   UBYTE  * optional_bytes_data;

   // next var : UBYTE should be enough
   //    but my bitstream reading function need a 32 bits wide variable
   UDWORD bottom_up;

   // cells infos
   DCC_CELL_S * cell;
   int        nb_cells_w;
   int        nb_cells_h;

   // not in file, for my own purpose
   DCC_BOX_S box;

   // final bitmap
   BITMAP * bmp;
   
} DCC_FRAME_S;

typedef struct DCC_BITSTREAM_S
{
   UBYTE  * data;      // pointer to the start of the bitstream
   UDWORD size;        // size of the bitstream, in bits
   UDWORD cur_byte;    // byte cursor
   UBYTE  cur_bit;     // bit cursor in the byte of current byte cursor
                       //    (from 0 to 7 : lowest bit to highest bit)
   UDWORD cur_bit_num; // bit cursor in the bitstream (not in the byte)
                       //    this also indicate the # of bits already read
} DCC_BITSTREAM_S;

// PixelBuffer entry
typedef struct DCC_PB_ENTRY_S
{
   UBYTE      val[4];
   int        frame;
   int        frame_cell_index;
} DCC_PB_ENTRY_S;

typedef struct DCC_DIRECTION_S
{
   UDWORD outsize_coded;
   UDWORD compression_flag;

   // next var : UBYTE should be enough
   //    but my bitstream reading function need a 32 bits wide variable
   UDWORD          variable0_bits;
   UDWORD          width_bits;
   UDWORD          height_bits;
   UDWORD          xoffset_bits;
   UDWORD          yoffset_bits;
   UDWORD          optional_bytes_bits;
   UDWORD          coded_bytes_bits;

   // bitstreams
   UDWORD          equal_cell_bitstream_size;
   UDWORD          pixel_mask_bitstream_size;
   UDWORD          encoding_type_bitstream_size;
   UDWORD          raw_pixel_bitstream_size;
   DCC_BITSTREAM_S equal_cell_bitstream;
   DCC_BITSTREAM_S pixel_mask_bitstream;
   DCC_BITSTREAM_S encoding_type_bitstream;
   DCC_BITSTREAM_S raw_pixel_bitstream;
   DCC_BITSTREAM_S pixel_code_and_displacment_bitstream;
   
   // frame buffer size, in # of cells, used in stage 2
   int             nb_cells_w;
   int             nb_cells_h;
   BITMAP          * bmp;        // frame buffer bitmap
   DCC_CELL_S      * buffer_ptr; // frame buffer cells
   
   // pixel buffer (1 entry = 4 pixels code & some other datas)
   DCC_PB_ENTRY_S  * pixel_buffer;
   int             pb_nb_entry;

   // colors present in the frame
   UBYTE           pixel_values[256]; // for pixel code color conversion

   // not in file, for my own purpose
   DCC_BOX_S       box;
} DCC_DIRECTION_S;

typedef struct DCC_HEADER_S
{
   UBYTE file_signature;
   UBYTE version;
   UBYTE directions;
   long  frames_per_dir;
   long  tag; // always 1. A data's presence bitfield maybe,
              //    like 0x01 = "final_dc6_size data present" ?
   long  final_dc6_size;
   long  dir_offset [DCC_MAX_DIR];

   int   already_decoded;
} DCC_HEADER_S;

typedef struct DCC_S
{
   UBYTE           * ptr; // copy of the dcc in mem
   long            size;  // size of the dcc in mem (in bytes)
   DCC_HEADER_S    header;
   DCC_DIRECTION_S direction [DCC_MAX_DIR];
   DCC_FRAME_S     frame     [DCC_MAX_DIR] [DCC_MAX_FRAME];
   DCC_BOX_S       box;
} DCC_S;

void    dcc_init                   (void);
int     dcc_read_bytes             (DCC_BITSTREAM_S * bs, int bytes_number, void * dest);
int     dcc_read_bits              (DCC_BITSTREAM_S * bs, int bits_number, int is_signed, UDWORD * dest );
int     dcc_frame_header_bitstream (DCC_S * dcc, DCC_BITSTREAM_S * bs, int d, int f);
void    dcc_init_dir_bitstream     (DCC_S * dcc, DCC_BITSTREAM_S * bs, int d);
int     dcc_optional_datas         (DCC_S * dcc, DCC_BITSTREAM_S * bs, int d);
int     dcc_other_bitstream_size   (DCC_S * dcc, DCC_BITSTREAM_S * bs, int d);
int     dcc_pixel_values_key       (DCC_S * dcc, DCC_BITSTREAM_S * bs, int d);
int     dcc_prepare_buffer_cells   (DCC_S * dcc, int d);
int     dcc_prepare_frame_cells    (DCC_S * dcc, int d, int f);
int     dcc_fill_pixel_buffer      (DCC_S * dcc, int d);
int     dcc_save_frame             (DCC_S * dcc, int d, int f, BITMAP * frm_bmp);
int     dcc_make_frames            (DCC_S * dcc, int d);
int     dcc_dir_bitstream          (DCC_S * dcc, int d);
int     dcc_file_header            (DCC_S * dcc);
int     dcc_decode                 (DCC_S * dcc, long dir_bitfield);
void    dcc_debug                  (DCC_S * dcc);
DCC_S * dcc_disk_load              (char * dcc_name);
DCC_S * dcc_mem_load               (void * mem_ptr, int mem_size);
void    dcc_destroy                (DCC_S * dcc);

#endif
