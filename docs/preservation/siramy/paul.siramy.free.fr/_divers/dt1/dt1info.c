#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include <string.h>

#define MAX_BLOCK 1024
#define MAX_SUBBLOCK 256


struct
{
   long offset;
   long length;
   long sub_block;
   long sub_length[MAX_SUBBLOCK]
} block[MAX_BLOCK];


// ==========================================================================
void tab_txt_output(FILE * in)
{
   FILE      * out;
   char      b_name[20]  = "dt1info-b.txt", sb_name[20] = "dt1info-sb.txt";
   long      x1, x2, n, nb_block, ptr;
   int       b, i, c;
   short int word;
   


   // =============== main header ==================

   // signature
   fread(&x1, 1, 4, in);
   fread(&x2, 1, 4, in);
   if ( (x1 != 7) || (x2 != 6) )
   {
      printf("wrong dt1's signature (%li %li)\n", x1, x2);
      return;
   }

   // zeros
   for (i=0; i<260; i++)
      c = fgetc(in);

   // # block
   fread(&nb_block, 1, 4, in);
   if (nb_block > MAX_BLOCK)
   {
      printf("too many blocks (%li, max %i)\n", nb_block, MAX_BLOCK);
      return;
   }
      
   // pointer to start of block headers
   fread(&ptr, 1, 4, in);
   fseek(in, ptr, SEEK_SET);



   // =============== block headers ================

   out = fopen(b_name, "wt");
   if (out == NULL)
   {
      printf("can't write %s\n", b_name);
      return;
   }
   printf("write %s\n", b_name);
   fprintf(out, "block_num\t"
                "direction\t"
                "roof_y\t"
                "sound\t"
                "animated\t"
                "size_y\t"
                "size_x\t"
                "zeros_1\t"
                "orientation\t"
                "main_index\t"
                "sub_index\t"
                "frame\t"
                "unknown_a\t"
                "unknown_b\t"
                "unknown_c\t"
                "unknown_d\t"
                "floor_flags\t"
                "zeros_2\t"
                "data_ptr\t"
                "length\t"
                "sub_blocks\t"
                "zeros_3\n"
   );
   for (b=0; b<nb_block; b++)
   {
      // block number
      fprintf(out, "%i\t", b);

      // direction
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // roof_y
      fread(&word, 1, 2, in);
      fprintf(out, "%i\t", word);
      
      // sound
      fread(&c, 1, 1, in);
      fprintf(out, "%i\t", c);
      
      // animated
      fread(&c, 1, 1, in);
      fprintf(out, "%i\t", c);
      
      // size_y
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // size_x
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // zeros_1
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // orientation
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // main_index
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // sub_index
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // frame
      fread(&n, 1, 4, in);
      fprintf(out, "%li\t", n);

      // unknown_a
      c = fgetc(in);
      fprintf(out, "%i\t", c);
      
      // unknown_b
      c = fgetc(in);
      fprintf(out, "%i\t", c);
      
      // unknown_c
      c = fgetc(in);
      fprintf(out, "%i\t", c);
      
      // unknown_d
      c = fgetc(in);
      fprintf(out, "%i\t", c);
      
      // floor flags
      for (i=0; i<25; i++)
      {
         c = fgetc(in);
         fprintf(out, "%02X ", c);
      }
      fprintf(out, "\t", c);

      // zeros_2
      for (i=0; i<7; i++)
      {
         c = fgetc(in);
         fprintf(out, "%02X ", c);
      }
      fprintf(out, "\t", c);

      // data ptr
      fread(&block[b].offset, 1, 4, in);
      fprintf(out, "%li\t", block[b].offset);

      // length
      fread(&block[b].length, 1, 4, in);
      fprintf(out, "%li\t", block[b].length);

      // sub-block
      fread(&block[b].sub_block, 1, 4, in);
      fprintf(out, "%li\t", block[b].sub_block);

      // zeros_3
      for (i=0; i<12; i++)
      {
         c = fgetc(in);
         fprintf(out, "%02X ", c);
      }
      fprintf(out, "\n", c);
   }
   fclose(out);



   // =============== sub-block headers ================

   out = fopen(sb_name, "wt");
   if (out == NULL)
   {
      printf("can't write %s\n", sb_name);
      return;
   }
   printf("write %s\n", sb_name);
   fprintf(out, "block_num\t"
                "sub_block_num\t"
                "x_pos\t"
                "y_pos\t"
                "zeros_1\t"
                "grid_x\t"
                "grid_y\t"
                "tile_format\t"
                "sub_length\t"
                "zeros_2\t"
                "data_offset\n"
   );
   for (b=0; b < nb_block; b++)
   {
      fseek(in, block[b].offset, SEEK_SET);
      if (block[b].sub_block > MAX_SUBBLOCK)
      {
         fclose(out);
         printf("too many sub blocks in block %li (%li, max %i)\n",
            b,
            block[b].sub_block,
            MAX_SUBBLOCK
         );
         return;
      }
      for (i=0; i<block[b].sub_block; i++)
      {
         // block_num
         fprintf(out, "%i\t", b);

         // sub_block_num
         fprintf(out, "%i\t", i);

         // x_pos
         fread(&word, 1, 2, in);
         fprintf(out, "%i\t", word);

         // y_pos
         fread(&word, 1, 2, in);
         fprintf(out, "%i\t", word);

         // zeros_1
         fread(&word, 1, 2, in);
         fprintf(out, "%i\t", word);

         // grid_x
         c = fgetc(in);
         fprintf(out, "%i\t", c);

         // grid_y
         c = fgetc(in);
         fprintf(out, "%i\t", c);

         // tile_format
         fread(&word, 1, 2, in);
         fprintf(out, "%04X\t", word);

         // sub_length
         fread(&block[b].sub_length[i], 1, 4, in);
         fprintf(out, "%li\t", block[b].sub_length[i]);

         // zeros_2
         fread(&word, 1, 2, in);
         fprintf(out, "%i\t", word);

         // data_offset
         fread(&n, 1, 4, in);
         fprintf(out, "%li\n", n);

      }
   }
   fclose(out);
}


// ==========================================================================
int main (int argc, char * argv[])
{
   FILE * in;

   printf("DT1INFO v 0.97 beta, by Paul Siramy, Freeware\n"
          "=============================================\n");
   if (argc != 2)
   {
      printf("syntaxe : dt1info <file.dt1>\n");
      exit(0);
   }
   in = fopen(argv[1], "rb");
   if (in == NULL)
   {
      printf("can't open %s\n", argv[1]);
      exit(1);
   }
   tab_txt_output(in);
   fclose(in);
}

