#include <string.h>
#include "structs.h"
#include "error.h"
#include "misc.h"
#include "dt1misc.h"
#include "mpq/MpqView.h"
#include "txtread.h"


// ==========================================================================
// prepare the reading of a col value
// give del_char TRUE to replace the TAB and CR/LF chars by 0
//
// function set nb_char to # of characters there's in that value
// function set is_new_line if after col was a LF of CR/LF
// function return NULL if after col is EOF, else pointer on next col / line
// note : handle both LF and CR/LF line type
char * txt_gets(char * bptr, int * nb_char, int * is_new_line, int del_char)
{
   * is_new_line = FALSE;
   * nb_char = 0;


   for (;;)
   {
      if (bptr[* nb_char] == 0)
      {
         // end of file
         return NULL;
      }
      else if (bptr[* nb_char] == '\t')
      {
         // tab
         if (del_char == TRUE)
            bptr[* nb_char] = 0;
         return bptr + (* nb_char) + 1;
      }
      else if (bptr[* nb_char] == 0x0A)
      {
         // end of LF line
         * is_new_line = TRUE;
         if (del_char == TRUE)
            bptr[* nb_char] = 0;
         return bptr + (* nb_char) + 1;
      }
      else if (bptr[* nb_char] == 0x0D)
      {
         // end of CR / LF line
         * is_new_line = TRUE;
         if (del_char == TRUE)
            bptr[* nb_char] = 0;
         return bptr + (* nb_char) + 2;
      }
      else
         (* nb_char)++;
   }
}


// ==========================================================================
void txt_count_header_cols(char * cur_col, int * col_count)
{
   int  nb_char, is_new_line;
   char * next_col = NULL;


   if ((cur_col == NULL) || (col_count == NULL))
      return;

   (* col_count) = 0;
   while (cur_col != NULL)
   {
      next_col = txt_gets(cur_col, & nb_char, & is_new_line, FALSE);

      // don't count empty headers or "*eol" column
      if (nb_char)
      {
         if (strnicmp(cur_col, "*eol", 4) != 0)
            (* col_count)++;
      }

      // next col
      if ((is_new_line) || (next_col == NULL))
         return;
      else
         cur_col = next_col;
   }
}


// ==========================================================================
char * txt_read_header(char * cur_col, TXT_S * txt)
{
   int  nb_char, i, col_pos = 0, is_new_line;
   char * next_col = NULL;


   while (cur_col != NULL)
   {
      next_col = txt_gets(cur_col, & nb_char, & is_new_line, TRUE);
      if (nb_char)
      {
         // search if that col header is one of the required (or one of the user defined filters)
         for (i=0; i < txt->col_num; i++)
         {
            if (stricmp(cur_col, txt->col[i].name) == 0)
            {
               // equal
               txt->col[i].pos = col_pos;
            }
         }
      }
      // this col done
      col_pos++;

      // next col
      if (is_new_line)
         return next_col;
      else
         cur_col = next_col;
   }
   return NULL;
}


// ==========================================================================
int txt_check_type_and_size(char * cur_col, TXT_S * txt)
{
   int  nb_char, i, col_pos = 0, is_new_line = FALSE, x;
   char * next_col;
   
   if (cur_col == NULL)
      return 1;
      
   // for all lines, check the string size of the interesting columns
   txt->line_num = 0;
   col_pos = 0;
   while (cur_col != NULL)
   {
      // read this col
      next_col = txt_gets(cur_col, & nb_char, & is_new_line, FALSE);
      
      if (nb_char)
      {
         // does this col is one of the required ?
         for (i=0; i < txt->col_num; i++)
         {
            if (col_pos == txt->col[i].pos)
            {
               // update its maximum size if needed
               if (nb_char > txt->col[i].size)
                  txt->col[i].size = nb_char;

               // check its type
               if (i >= txt->nb_required_cols)
               {
                  // user defined filter : force to be string
                  if (txt->col[i].type == CT_NULL)
                     txt->col[i].type = CT_STR;
               }
               else
               {
                  if (txt->col[i].type == CT_NULL)
                     txt->col[i].type = CT_NUM;

                  if (txt->col[i].type != CT_STR)
                  {
                     for (x=0; x < nb_char; x++)
                     {
                        if ( ((cur_col[x] < '0') || (cur_col[x] > '9'))
                             &&
                             cur_col[x] != '-')
                        {
                           txt->col[i].type = CT_STR;
                           x = nb_char;
                        }
                     }
                  }
               }
            }
         }
      }
      
      // this col done
      col_pos++;

      // next col
      if (is_new_line)
      {
         txt->line_num++;
         col_pos = 0;
      }
      cur_col = next_col;
   }
   return 0;
}


// ==========================================================================
int txt_fill_data(char * cur_col, TXT_S * txt)
{
   int  nb_char, i, col_pos = 0, is_new_line, cur_line = 0;
   char * next_col, * data_ptr, * sptr;
   long * lptr;
   
   if ((cur_col == NULL) || (txt->data == NULL))
      return 1;
      
   // for all lines, read the data
   col_pos = 0;
   data_ptr = txt->data;
   while (cur_col != NULL)
   {
      // read this col
      next_col = txt_gets(cur_col, & nb_char, & is_new_line, TRUE);
      
      if (nb_char)
      {
         // does this col is one of the required ?
         for (i=0; i < txt->col_num; i++)
         {
            if (col_pos == txt->col[i].pos)
            {
               // copy it
               sptr = data_ptr + txt->col[i].offset;
               if (txt->col[i].type == CT_STR)
                  strcpy(sptr, cur_col);
               else if (txt->col[i].type == CT_NUM)
               {
                  lptr = (long *) sptr;
                  * lptr = atol(cur_col);
               }
            }
         }
      }
      
      // this col done
      col_pos++;

      // next col
      if (is_new_line)
      {
         cur_line++;
         if (cur_line > txt->line_num)
            return 1;
         else
         {
            data_ptr += txt->line_size;
            col_pos = 0;
         }
      }
      cur_col = next_col;
   }
   return 0;
}


// ==========================================================================
TXT_S * txt_destroy(TXT_S * txt)
{
   if (txt == NULL)
      return NULL;
   if (txt->data != NULL)
      free(txt->data);
   if (txt->col != NULL)
      free(txt->col);
   free(txt);
   return NULL;
}


// ==========================================================================
void txt_get_user_filter_cols(char * cur_col, TXT_S * txt)
{
   int  nb_char, is_new_line;
   char * next_col = NULL;
   int  i = 0;
   char tmp[500] = "";
   int  size = 0;


   if ((cur_col == NULL) || (txt == NULL))
      return;

   while (cur_col != NULL)
   {
      next_col = txt_gets(cur_col, & nb_char, & is_new_line, FALSE);

      // don't count empty headers or "*eol" column
      if (nb_char)
      {
         if (strnicmp(cur_col, "*eol", 4) != 0)
         {
            // is this column already in the required cols ?
            for (i=0; i < txt->nb_required_cols; i++)
            {
               size = 0;
               while ( (cur_col[size] != 0x00) && (cur_col[size] != '\t') &&
                       (cur_col[size] != 0x0D) && (cur_col[size] != 0x0A) )
               {
                  size++;
               }
               
               if (strnicmp(txt->col[i].name, cur_col, size) == 0)
                  break;
            }

            if (i >= txt->nb_required_cols)
            {
               // this header is NOT already in the required cols, add it as a user defined filter

               // search first free filter slot
               while ((i < txt->col_num) && (strlen(txt->col[i].name) > 0))
                  i++;

               if (i < txt->col_num)
               {
                  if (size >= TXT_COL_NAME_LENGTH)
                  {
                     sprintf(
                      tmp
                      ,
                      "txt_get_user_filter_cols() : found a user defined filter header name that is more than %i "
                      "characters : \"%.*s...\" (%i characters)"
                      ,
                      TXT_COL_NAME_LENGTH - 1,
                      TXT_COL_NAME_LENGTH - 1,
                      cur_col,
                      size
                     );
                     ds1edit_error(tmp);
                  }
                  strncpy(txt->col[i].name, cur_col, size);
               }
            }
         }
      }

      // next col
      if ((is_new_line) || (next_col == NULL))
         return;
      else
         cur_col = next_col;
   }
}


// ==========================================================================
// note : the mem buffer MUST end by a 0
TXT_S * txt_load(char * mem, RQ_ENUM enum_txt, char * filename)
{
   TXT_S * txt = NULL;
   int   i=0, size=0, all_col_ok = TRUE;
   char  * first_line = NULL, tmp[150] = "";
   char  ** required_col = NULL;


   if (((enum_txt < 0)) || (enum_txt >= RQ_MAX))
   {
      sprintf(tmp, "txt_load() : parameter 'enum_txt' = %i, should be between 0 and %i", enum_txt, RQ_MAX - 1);
      ds1edit_error(tmp);
   }

   required_col = glb_txt_req_ptr[enum_txt];

   printf("\ntxt_load() : loading %s\n", filename);
   fflush(stdout);

   // create a new TXT_S structure
   size = sizeof(TXT_S);
   txt = (TXT_S *) malloc(size);
   if (txt == NULL)
   {
      sprintf(tmp, "txt_load() : can't allocate %i bytes", size);
      ds1edit_error(tmp);
   }
   memset(txt, 0, size);

   // count the required columns
   txt->nb_required_cols = 0;
   while (required_col[txt->nb_required_cols] != NULL)
      txt->nb_required_cols++;

   // count all the columns
   txt->col_num = 0;
   if (enum_txt == RQ_OBJ)
   {
      // editor's data/obj.txt : count all columns that have a header, to include user filter columns
      txt_count_header_cols(mem, & txt->col_num);
   }
   else
      txt->col_num = txt->nb_required_cols;
   
   txt->col = (TXT_COL_S *) calloc(txt->col_num + 1, sizeof(TXT_COL_S));
   if (txt->col == NULL)
   {
      txt = txt_destroy(txt);
      sprintf(tmp, "txt_load() : calloc() error for %i element * %i bytes each", txt->col_num + 1, sizeof(TXT_COL_S));
      ds1edit_error(tmp);
   }

   // init the cols, required cols first
   for (i=0; i < txt->col_num; i++)
   {
      txt->col[i].pos = -1;
      if (i < txt->nb_required_cols)
         strncpy(txt->col[i].name, required_col[i], TXT_COL_NAME_LENGTH);
   }

   // finish the init of cols, non-required cols last
   if (txt->nb_required_cols != txt->col_num)
      txt_get_user_filter_cols(mem, txt);

   // stage 1, process header   
   first_line = txt_read_header(mem, txt);
   for (i=0; i < txt->col_num; i++)
   {
      if (txt->col[i].pos == -1)
      {
         printf("txt_load() : didn't found required column %i (%s)\n", i, txt->col[i].name);
         fflush(stdout);
         all_col_ok = FALSE;
      }
   }

   if (all_col_ok != TRUE)
   {
      txt = txt_destroy(txt);
      sprintf(tmp, "txt_load() : not all columns have been found");
      ds1edit_error(tmp);
   }

   // stage 2, search max size of all string columns except header
   if (txt_check_type_and_size(first_line, txt))
   {
      txt = txt_destroy(txt);
      sprintf(tmp, "txt_load() : stage 2, error shouldn't have happen");
      ds1edit_error(tmp);
   }

   // stage 3, make the struct
   size = 0;
   for (i=0; i < txt->col_num; i++)
   {
      txt->col[i].offset = size;
      if (txt->col[i].type == CT_NUM)
         size += sizeof(long);
      else if (txt->col[i].type == CT_STR)
         size += (txt->col[i].size + 1);
   }
   txt->line_size = size;

   txt->data = (char *) calloc(txt->line_num + 1, txt->line_size);
   if (txt->data == NULL)
   {
      txt = txt_destroy(txt);
      sprintf(tmp, "txt_load() : calloc() error for %i elements * %i bytes each", txt->line_num + 1, txt->line_size);
      ds1edit_error(tmp);
   }
   
   // stage 4, load the txt into the struct
   if (txt_fill_data(first_line, txt))
   {
      txt = txt_destroy(txt);
      sprintf(tmp, "txt_load() : stage 4, error shouldn't have happen");
      ds1edit_error(tmp);
   }

   return txt;
}


// ==========================================================================
// load a .txt from a mpq (or mod dir) into mem
void * txt_read_in_mem(char * txtname)
{
   void * buff = NULL, * new_buff;
   int  entry;
   long len;
   char tmp[150];

   printf("\nwant to read a txt from mpq : %s\n", txtname);
   entry = misc_load_mpq_file(txtname, (char **) & buff, & len, TRUE);
   if ((entry == -1) || (buff == NULL))
   {
      sprintf(tmp, "txt_read_in_mem() : file %s not found", txtname);
      ds1edit_error(tmp);
   }

   len++;
   new_buff = realloc(buff, len);
   if (new_buff == NULL)
   {
      sprintf(tmp, "txt_read_in_mem() : can't reallocate %i bytes for %s",
         len, txtname);
      if (buff != NULL)
         free(buff);
      ds1edit_error(tmp);
   }

   if (new_buff != buff)
      memcpy(new_buff, buff, len - 1);
   
   * (((char *) new_buff) + len - 1) = 0;

   return buff;
}


// ==========================================================================
void txt_convert_slash(char * str)
{
   int i, s = strlen(str);


   for (i=0; i < s; i++)
      if (str[i] == '/')
         str[i] = '\\';
}


// ==========================================================================
// load lvlTypes.txt in mem, then load each dt1 for a 1 ds1
void txt_debug(char * file_path_mem, char * file_path_def, TXT_S * txt)
{
   FILE * out = NULL;
   int  i     = 0;


   if (glb_ds1edit.cmd_line.debug_mode != TRUE)
      return;
   
   if ((file_path_mem == NULL) || (file_path_def == NULL) || (txt == NULL))
      return;

   out = fopen(file_path_mem, "wb");
   if (out != NULL)
   {
      fwrite(txt->data, txt->line_size, txt->line_num, out);
      fclose(out);
   }

   out = fopen(file_path_def, "wt");
   if (out != NULL)
   {
      fprintf(out, "memory_col col_type %-*s txt_col_num data_type txt_data_size memory_offset\n",  TXT_COL_NAME_LENGTH, "header_name");
      fprintf(out, "---------- -------- %-.*s ----------- --------- ------------- -------------\n", TXT_COL_NAME_LENGTH, "------------------------------");
                               
      for (i=0; i < txt->col_num; i++)
      {
         fprintf(
            out,
            "%10i %-8s %-*s %11i %-9s %13i %13i\n",
            i,
            (i >= txt->nb_required_cols) ? "Filter" : "Required",
            TXT_COL_NAME_LENGTH,
            txt->col[i].name,
            txt->col[i].pos,
            txt->col[i].type == CT_NUM ? "NUM" : "STR",
            txt->col[i].size,
            txt->col[i].offset
         );
      }
      fclose(out);
   }
}


// ==========================================================================
// load lvlTypes.txt in mem, then load each dt1 for a 1 ds1
int read_lvltypes_txt(int ds1_idx, int type)
{
   TXT_S * txt = NULL;
   char  lvltypes[] = "Data\\Global\\Excel\\LvlTypes.txt",
         ds1edt_file[] = "ds1edit.dt1", * buff, * sptr;
   int   i, f;
   long  * lptr = NULL, act;
   char  tmp[150] = "", name[256] = "";
   int   col_idx = 0, file_1_idx = 0;
   long  * tmp_long = NULL;


   // load the file in mem
   if (glb_ds1edit.lvltypes_buff == NULL)
   {
      buff = txt_read_in_mem(lvltypes);
      if (buff == NULL)
         return -1;

      // load the .txt in a more handy struct
      txt = txt_load(buff, RQ_LVLTYPE, lvltypes);
      glb_ds1edit.lvltypes_buff = txt;
      free(buff);
      if (txt == NULL)
         return -1;

      // debug files
      if (glb_ds1edit.cmd_line.debug_mode == TRUE)
         txt_debug(glb_path_lvltypes_mem, glb_path_lvltypes_def, txt);
   }
   else
      txt = glb_ds1edit.lvltypes_buff;

   // search the good Id
   for (i=0; i < txt->line_num; i++)
   {
      sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_LVLTYPE, "ID")].offset;
      lptr = (long *) sptr;
      if ((* lptr) == type)
      {
         // Id found
         printf("Found ID %i in LvlTypes.txt at row %i, col %i\n",
            type, i + 1, txt->col[misc_get_txt_column_num(RQ_LVLTYPE, "ID")].pos);
         fflush(stdout);

         // check act
         sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_LVLTYPE, "Act")].offset;
         lptr = (long *) sptr;
         act  = * lptr;
         if ((act != glb_ds1[ds1_idx].act) && (glb_ds1edit.cmd_line.no_check_act == FALSE))
         {
            sprintf(tmp, "read_lvltypes_txt() : Acts from LvlTypes.txt (%li) and the Ds1 (%li) "
               "are different",
               act,
               glb_ds1[ds1_idx].act
            );
            ds1edit_error(tmp);
         }

         // opening dt1
         printf("Found dt1files :\n");
         file_1_idx = misc_get_txt_column_num(RQ_LVLTYPE, "File 1");
         for (f=0; f<33; f++)
         {
            glb_ds1[ds1_idx].dt1_idx[f] = -1;
            if (f==0)
            {
               sprintf(tmp, "%s%s", glb_ds1edit_data_dir, ds1edt_file);
               printf("\nwant to read a dt1 : %s\n", tmp);
               glb_ds1[ds1_idx].dt1_idx[f] = dt1_add_special(tmp);
            }
            else
            {
               col_idx = file_1_idx + (f - 1);
               sptr = txt->data + (i * txt->line_size) + txt->col[col_idx].offset;
               if (txt->col[col_idx].type == CT_STR)
                  strcpy(tmp, sptr);
               else if (txt->col[col_idx].type == CT_NUM)
               {
                  tmp_long = (long *) sptr;
                  sprintf(tmp, "%li", * tmp_long);
               }
               txt_convert_slash(tmp);
               strcpy(name, glb_tiles_path);
               strcat(name, tmp);
               if (glb_ds1[ds1_idx].dt1_mask[f])
               {
                  printf("\nwant to read a dt1 from mpq : %s\n", name);
                  glb_ds1[ds1_idx].dt1_idx[f] = dt1_add(name);
               }
               else
               {
                  if ((tmp[0] != '0') && tmp[1] != 0)
                     printf("\n(skip %s)\n", tmp);
               }
            }
         }

         // end
         return act;
      }
   }
   sprintf(tmp, "couldn't find the ID %i in LvlTypes.txt\n", type);
   ds1edit_error(tmp);
   return -1;
}


// ==========================================================================
// load lvlPrest.txt into mem, then search the Dt1Mask, given the Def Id
int read_lvlprest_txt(int ds1_idx, int def)
{
   TXT_S * txt;
   char  * buff, * lvlprest = "Data\\Global\\Excel\\LvlPrest.txt";
   int   i, b;
   long  * lptr;
   char  * sptr, tmp[150], * found_ptr = NULL;
   int   mask, filename_colidx[6], filename_idx, length, c, found_nb, last_found=0,
         last_slash, found_col=0;


   // load the file in mem
   if (glb_ds1edit.lvlprest_buff == NULL)
   {
      buff = txt_read_in_mem(lvlprest);
      if (buff == NULL)
         return -1;

      // load the .txt in a more handy struct
      txt = txt_load(buff, RQ_LVLPREST, lvlprest);
      glb_ds1edit.lvlprest_buff = txt;
      free(buff);
      if (txt == NULL)
         return -1;

      // debug files
      if (glb_ds1edit.cmd_line.debug_mode == TRUE)
         txt_debug(glb_path_lvlprest_mem, glb_path_lvlprest_def, txt);
   }
   else
      txt = glb_ds1edit.lvlprest_buff;

   // search the good Def
   if (def == -1)
   {
      printf("AUTOMATIC LVLPREST.DEF DETECTION\n");
      fflush(stdout);

      // automatic search, base on filename
      filename_colidx[0] = misc_get_txt_column_num(RQ_LVLPREST, "File1");
      filename_colidx[1] = misc_get_txt_column_num(RQ_LVLPREST, "File2");
      filename_colidx[2] = misc_get_txt_column_num(RQ_LVLPREST, "File3");
      filename_colidx[3] = misc_get_txt_column_num(RQ_LVLPREST, "File4");
      filename_colidx[4] = misc_get_txt_column_num(RQ_LVLPREST, "File5");
      filename_colidx[5] = misc_get_txt_column_num(RQ_LVLPREST, "File6");

      found_nb = 0;
      for (i=0; i < txt->line_num; i++)
      {
         for (filename_idx = 0; filename_idx < 6; filename_idx++)
         {
            sptr = txt->data + (i * txt->line_size) + txt->col[ filename_colidx[filename_idx] ].offset;
            if (sptr != NULL)
            {
               length = strlen(sptr);
               if(length > 0)
               {
                  // search last '/' or last '\'
                  last_slash = 0;
                  for (c=0; c < length; c++)
                  {
                     if ((sptr[c] == '/') || (sptr[c] == '\\'))
                        last_slash = c;
                  }

                  // compare this filename with our ds1 filename
                  if (last_slash)
                     last_slash++;
                  if (last_slash >= length)
                     last_slash = length - 1;
                  if (stricmp(sptr + last_slash, glb_ds1[ds1_idx].filename) == 0)
                  {
                     // found one
                     found_nb++;
                     last_found = i;
                     found_ptr  = sptr;
                     found_col  = filename_idx;
                  }
               }
            }
         }
      }

      if (found_nb == 1)
      {
         // only one, so no error

         i = last_found;

         sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_LVLPREST, "Def")].offset;
         lptr = (long *) sptr;

         printf("Found \"%s\" in LvlPrest.txt at row %i (DEF = %li, 'File%i' = \"%s\")\n",
            glb_ds1[ds1_idx].filename,
            i + 1,
            * lptr,
            found_col + 1,
            found_ptr
         );
         fflush(stdout);

         // dt1mask
         sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_LVLPREST, "Dt1Mask")].offset;
         lptr = (long *) sptr;
         mask = * lptr;

         printf("DT1MASK = %li in LvlPrest.txt at row %i, col %i\n",
            mask, i + 1, txt->col[misc_get_txt_column_num(RQ_LVLPREST, "Dt1Mask")].pos);
         fflush(stdout);
      
         for (b=0; b < DT1_IN_DS1_MAX; b++)
         {
            if (b == 0)
               glb_ds1[ds1_idx].dt1_mask[b] = TRUE;
            else
               glb_ds1[ds1_idx].dt1_mask[b] = mask & (1 << (b-1)) ? TRUE : FALSE;
         }

         // end
         return 0;
      }
      else
      {
         if (found_nb >= 2)
         {
            sprintf(tmp, "couldn't found ds1 filename \"%s\" in LvlPrest.txt for sure : present %i times\n",
               glb_ds1[ds1_idx].filename,
               found_nb
            );
            ds1edit_error(tmp);
         }
         else
         {
            sprintf(tmp, "couldn't found ds1 filename \"%s\" in LvlPrest.txt\n",
               glb_ds1[ds1_idx].filename
            );
            ds1edit_error(tmp);
         }
      }
   }
   else
   {
      for (i=0; i < txt->line_num; i++)
      {
         sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_LVLPREST, "Def")].offset;
         lptr = (long *) sptr;
         if ((* lptr) == def)
         {
            // Def found
            printf("Found DEF %i in LvlPrest.txt at row %i, col %i\n",
               def, i + 1, txt->col[misc_get_txt_column_num(RQ_LVLPREST, "Def")].pos);
            fflush(stdout);

            // dt1mask
            sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_LVLPREST, "Dt1Mask")].offset;
            lptr = (long *) sptr;
            mask = * lptr;

            printf("DT1MASK = %li in LvlPrest.txt at row %i, col %i\n",
               mask, i + 1, txt->col[misc_get_txt_column_num(RQ_LVLPREST, "Dt1Mask")].pos);
            fflush(stdout);
         
            for (b=0; b < DT1_IN_DS1_MAX; b++)
            {
               if (b == 0)
                  glb_ds1[ds1_idx].dt1_mask[b] = TRUE;
               else
                  glb_ds1[ds1_idx].dt1_mask[b] = mask & (1 << (b-1)) ? TRUE : FALSE;
            }

            // end
            return 0;
         }
      }
   }
   
   // end
   sprintf(tmp, "couldn't found the DEF %i in LvlPrest.txt\n", def);
   ds1edit_error(tmp);
   return -1; // useless, just to not have a warning under VC6
}


// ==========================================================================
// load the obj.txt into mem
int read_obj_txt(void)
{
   TXT_S * txt;
   char  * buff, obj[] = "Obj.txt";
   int   i, size;
   long  * lptr, len;
   char  * sptr, tmp[150];


   // load the file in mem
   if (glb_ds1edit.obj_buff == NULL)
   {
      printf("\nwant to read %s from %s\n", obj, glb_ds1edit_data_dir);
      if (mod_load_in_mem(glb_ds1edit_data_dir, obj, &buff, &len) == -1)
      {
         sprintf(tmp, "read_obj_txt() : file %s not found", obj);
         ds1edit_error(tmp);
      }
      printf(", found in .\n");

      // load the .txt in a more handy struct
      txt = txt_load(buff, RQ_OBJ, obj);
      glb_ds1edit.obj_buff = txt;
      free(buff);
      if (txt == NULL)
         return -1;

      // debug files
      if (glb_ds1edit.cmd_line.debug_mode == TRUE)
         txt_debug(glb_path_obj_mem, glb_path_obj_def, txt);
   }
   else
   {
      txt = glb_ds1edit.obj_buff;
   }

   // malloc
   glb_ds1edit.obj_desc_num = txt->line_num;
   size = glb_ds1edit.obj_desc_num * sizeof(OBJ_DESC_S);
   glb_ds1edit.obj_desc = (OBJ_DESC_S *) malloc(size);
   if (glb_ds1edit.obj_desc == NULL)
   {
      sprintf(tmp, "read_obj_txt() : not enough mem for %i bytes", size);
      ds1edit_error(tmp);
   }
   memset(glb_ds1edit.obj_desc, 0, size);

   // read the obj.txt
   for (i=0; i < txt->line_num; i++)
   {
      // act
      sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Act")].offset;
      lptr = (long *) sptr;
      glb_ds1edit.obj_desc[i].act = * lptr;
      
      // type
      sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Type")].offset;
      lptr = (long *) sptr;
      glb_ds1edit.obj_desc[i].type = * lptr;
      
      // id
      sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "ID")].offset;
      lptr = (long *) sptr;
      glb_ds1edit.obj_desc[i].id = * lptr;

      // description
      sptr = txt->data + (i * txt->line_size) + txt->col[misc_get_txt_column_num(RQ_OBJ, "Description")].offset;
      glb_ds1edit.obj_desc[i].desc = sptr;
   }

   // lookups, to avoid to search them again and again
   glb_ds1edit.col_obj_id = misc_get_txt_column_num(RQ_OBJ, "Objects.txt_ID");

   return 0;
}


// ==========================================================================
// load the objects.txt into mem
int read_objects_txt(void)
{
   TXT_S * txt;
   char  * buff, * objects = "Data\\Global\\Excel\\Objects.txt";


   // load the file in mem
   if (glb_ds1edit.objects_buff == NULL)
   {

      buff = txt_read_in_mem(objects);
      if (buff == NULL)
         return -1;

      // load the .txt in a more handy struct
      txt = txt_load(buff, RQ_OBJECTS, objects);
      glb_ds1edit.objects_buff = txt;
      free(buff);
      if (txt == NULL)
         return -1;

      // debug files
      if (glb_ds1edit.cmd_line.debug_mode == TRUE)
         txt_debug(glb_path_objects_mem, glb_path_objects_def, txt);
   }
   else
      txt = glb_ds1edit.objects_buff;

   // lookups, to avoid to search them again and again

   glb_ds1edit.col_objects_id     = misc_get_txt_column_num(RQ_OBJECTS, "Id");

   glb_ds1edit.col_frame_delta[0] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta0");
   glb_ds1edit.col_frame_delta[1] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta1");
   glb_ds1edit.col_frame_delta[2] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta2");
   glb_ds1edit.col_frame_delta[3] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta3");
   glb_ds1edit.col_frame_delta[4] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta4");
   glb_ds1edit.col_frame_delta[5] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta5");
   glb_ds1edit.col_frame_delta[6] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta6");
   glb_ds1edit.col_frame_delta[7] = misc_get_txt_column_num(RQ_OBJECTS, "FrameDelta7");

   glb_ds1edit.col_orderflag[0] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag0");
   glb_ds1edit.col_orderflag[1] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag1");
   glb_ds1edit.col_orderflag[2] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag2");
   glb_ds1edit.col_orderflag[3] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag3");
   glb_ds1edit.col_orderflag[4] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag4");
   glb_ds1edit.col_orderflag[5] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag5");
   glb_ds1edit.col_orderflag[6] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag6");
   glb_ds1edit.col_orderflag[7] = misc_get_txt_column_num(RQ_OBJECTS, "OrderFlag7");
   return 0;
}
