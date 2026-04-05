#ifdef WIN32
   #pragma warning (push)
   #pragma warning (disable  :  4057 )
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpqTypes.h"
#include "Dclib.h"

#include "../structs.h"
#include "../error.h"


extern UInt32 ExtWavUnp1 (UInt32,UInt32,UInt32,UInt32); // Call for metod: 0x01
extern UInt32 ExtWavUnp2 (UInt32,UInt32,UInt32,UInt32); // Call for metod: 0x40
extern UInt32 ExtWavUnp3 (UInt32,UInt32,UInt32,UInt32); // Call for metod: 0x80

//  declaration of functions
int    test_prepare_archive (void);
DWORD  test_tell_entry      (char * filename);

void   BuildBaseMassive     (void);
int    InitializeLocals     (void);
void   FreeLocals           (void);
DWORD  Crc                  (char * string, DWORD * massive_base, DWORD massive_base_offset);
void   Decode               (DWORD * data_in, DWORD * massive_base, DWORD crc, DWORD lenght);
UInt16 read_data            (UInt8 * buffer, UInt16 size, void  * crap);
void   write_data           (UInt8 * buffer, UInt16 size, void  * crap);
int    ExtractTo            (FILE * fp_new, DWORD entry);
int    ExtractToMem         (void * mp_new, DWORD entry);

// a struct typedef
typedef struct params
{
	char * buf_in;
	char * buf_out;
} params;

GLB_MPQ_S * glb_mpq = NULL; // pointer to current mpq file structure


// ==========================================================================
// read from mod directory, NOT from a mpq
int mod_load_in_mem(
   char * moddir,
   char * filename,
   void ** buffer,
   long * buf_len,
   int output
)
{
   FILE * in        = NULL;
   long size        = 0;
   char strtmp[256] = "";


   // init
   * buffer  = (void *) NULL;
   * buf_len = 0;

   // open file
   sprintf(strtmp, "%s\\%s", moddir, filename);
   in = fopen(strtmp, "rb");
   if (in == NULL)
      return -1; // not read

   // get size, for malloc()
   fseek(in, 0, SEEK_END);
   size = ftell(in) + 1;
   fseek(in, 0, SEEK_SET);
   * buffer = (void *) malloc(size);
   if (* buffer == NULL)
   {
      fclose(in);
      sprintf(strtmp, "not enough mem (%li bytes) for %s (from Mod Directory)",
         size, filename);
      ds1edit_error(strtmp);
   }
   size--;
   * buf_len = size;

   // copy the file in mem
   fread(* buffer, size, 1, in);
   * ( ((char *) (* buffer)) + size) = 0;

   //end
   if (output)
      printf("ok (%li bytes)", size);
   fclose(in);
   return 0; // not equal to -1 means it's ok
}

   
// ==========================================================================
void mpq_batch_open(char * mpqname)
{
   char strtmp[256];
   
   // in case
   if (glb_mpq == NULL)
   {
      sprintf(strtmp, "mpq_batch_open_mpq() : pointer glb_mpq == NULL");
      ds1edit_error(strtmp);
   }
   if (glb_mpq->is_open != FALSE)
   {
      sprintf(strtmp, "mpq_batch_open_mpq() : glb_mpq->is_open != FALSE");
      ds1edit_error(strtmp);
   }
   if (mpqname == NULL)
   {
      sprintf(strtmp, "mpq_batch_open_mpq() : mpqname == NULL");
      ds1edit_error(strtmp);
   }

   // ok, let's go
   glb_mpq->avail_metods[0] = 0x08;
   glb_mpq->avail_metods[1] = 0x01;
   glb_mpq->avail_metods[2] = 0x40;
   glb_mpq->avail_metods[3] = 0x80;

   // open mpq
   strcpy(glb_mpq->file_name, mpqname);
   glb_mpq->fpMpq = fopen(glb_mpq->file_name, "rb");
   if( ! glb_mpq->fpMpq)
   {
      sprintf(strtmp, "\nmpq_batch_open_mpq() :  can't open %s", glb_mpq->file_name);
      ds1edit_error(strtmp);
   }

   // prepare mpq to be read
   if (test_prepare_archive())
      exit(DS1ERR_INITMPQ);

   // flag this mpq to have some memory to free at exit
   glb_mpq->is_open = TRUE;
}


// ==========================================================================
int mpq_batch_load_in_mem(char * filename, void ** buffer, long * buf_len,
                          int output)
{
   int  num_entry, size_unpack;
   void * mp_new;
   char strtmp[256];
   
   // in case
   if (glb_mpq == NULL)
   {
      sprintf(strtmp, "mpq_batch_load_in_mem_mpq() : pointer glb_mpq == NULL");
      ds1edit_error(strtmp);
   }
   if (glb_mpq->is_open == FALSE)
   {
      sprintf(strtmp, "mpq_batch_load_in_mem_mpq() : glb_mpq->is_open == FALSE");
      ds1edit_error(strtmp);
   }
   if (filename == NULL)
   {
      sprintf(strtmp, "mpq_batch_load_in_mem_mpq() : filename == NULL");
      ds1edit_error(strtmp);
   }
   if (strlen(filename) == 0)
   {
      sprintf(strtmp, "mpq_batch_load_in_mem_mpq() : strlen(filename) == 0");
      ds1edit_error(strtmp);
   }

   // let's go
   num_entry = test_tell_entry(filename);
   if (num_entry != -1)
   {
      size_unpack = * (glb_mpq->block_table + (num_entry * 4) + 2);
      if(num_entry < 0 || num_entry > (int) glb_mpq->count_files - 1)
      {
         sprintf(strtmp, "Error! Invalid entry number %i (Valid entry numbers are 1-%i)\n",
            num_entry + 1,
            glb_mpq->count_files
         );
         ds1edit_error(strtmp);
      }
      else
      {
         // load the file into memory
         mp_new = (void *) malloc(size_unpack);
         if (mp_new == NULL)
         {
            sprintf(strtmp, "mpq_batch_load_in_mem() : not enough memory for %i bytes\n", size_unpack);
            ds1edit_error(strtmp);
         }
         ExtractToMem(mp_new, num_entry);
         * buffer = mp_new;
         * buf_len = size_unpack;
         if (output)
            printf("ok (%i bytes)", size_unpack);
      }
   }
   return num_entry;
}


// ==========================================================================
void mpq_batch_close()
{
   fclose(glb_mpq->fpMpq);
   FreeLocals();
}


// ==========================================================================
// open and prepare the archive
int test_prepare_archive(void)
{
	DWORD	mpq_header[2] = {0x1a51504d, 0x00000020};
	DWORD	detected = 0;
	DWORD	tmp;
   char  name_htable[] = "(hash table)";
   char  name_btable[] = "(block table)";

   // search in the mpq the bytes 4D 50 51 1A 20 00 00 00
   //     = search the string : "MPQ" + 0x1A + " " + 0x00 + 0x00 + 0x00
	while(fread(&tmp,sizeof(DWORD),1,glb_mpq->fpMpq)) {
		if(mpq_header[0]==tmp) {
			fread(&tmp,sizeof(DWORD),1,glb_mpq->fpMpq);
			if(mpq_header[1]==tmp) {
				detected=1;
				break;
			}
		}
	}
	if(!detected) {
		printf("\nError: File \'%s\' is not a valid MPQ archive", glb_mpq->file_name);
		fclose(glb_mpq->fpMpq);
		return -1;
	}

   // found the start of mpq infos in the archive
   // it might be the start of archive
	glb_mpq->offset_mpq = ftell(glb_mpq->fpMpq) - 8;

   // read 4 bytes : Lenght of MPQ file data
	fread(&glb_mpq->lenght_mpq_part,sizeof(DWORD),1,glb_mpq->fpMpq);
   
   // SKIP 4 bytes
	fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq + 16,SEEK_SET);

   // read 4 bytes : Offset to hash_table of MPQ
	fread(&glb_mpq->offset_htbl,sizeof(DWORD),1,glb_mpq->fpMpq);

   // read 4 bytes : Offset to block_table of MPQ
	fread(&glb_mpq->offset_btbl,sizeof(DWORD),1,glb_mpq->fpMpq);

   // read 4 bytes : Lenght of hash table
	fread(&glb_mpq->lenght_htbl,sizeof(DWORD),1,glb_mpq->fpMpq);
	glb_mpq->lenght_htbl *= 4;

   // read 4 bytes : Lenght of block table
	fread(&glb_mpq->lenght_btbl,sizeof(DWORD),1,glb_mpq->fpMpq);

   // Number of files in MPQ (calculated from size of block_table)
   glb_mpq->count_files = glb_mpq->lenght_btbl;
	glb_mpq->lenght_btbl *= 4; // in fact, length of block table is in # of file

   // prepare Crc
	BuildBaseMassive();
	if(InitializeLocals()) {
		fclose(glb_mpq->fpMpq);
		return -2;
	}

   // read hash table
	fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+glb_mpq->offset_htbl,SEEK_SET);
	fread(glb_mpq->hash_table,sizeof(DWORD),glb_mpq->lenght_htbl,glb_mpq->fpMpq);

   // read block table
	fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+glb_mpq->offset_btbl,SEEK_SET);
	fread(glb_mpq->block_table,sizeof(DWORD),glb_mpq->lenght_btbl,glb_mpq->fpMpq);
   

   // calculate crc of "(hash table)", then decode its datas
   tmp=Crc(name_htable,glb_mpq->massive_base,0x300);
	Decode(glb_mpq->hash_table,glb_mpq->massive_base,tmp,glb_mpq->lenght_htbl);
   
   // calculate crc of "(block table)", then decode its datas
	tmp=Crc(name_btable,glb_mpq->massive_base,0x300);
	Decode(glb_mpq->block_table,glb_mpq->massive_base,tmp,glb_mpq->lenght_btbl);
 
   // end
	return 0;
}

// ==========================================================================
// search which entry in the archive is the file
DWORD test_tell_entry(char * filename)
{
   DWORD scrc1, scrc2, scrc3, pointer_ht;

   scrc1 = Crc(filename, glb_mpq->massive_base, 0);
   scrc2 = Crc(filename, glb_mpq->massive_base, 0x100);
   scrc3 = Crc(filename, glb_mpq->massive_base, 0x200);
   pointer_ht = (scrc1 & (glb_mpq->lenght_htbl/4-1) ) * 4;
   for(; pointer_ht<glb_mpq->lenght_htbl; pointer_ht += 4)
   {
      if( (* (glb_mpq->hash_table+pointer_ht)   == scrc2) &&
          (* (glb_mpq->hash_table+pointer_ht+1) == scrc3)
      )
         return * (glb_mpq->hash_table + pointer_ht + 3);
   }
   return (DWORD) -1;
}


/******************************************************************************
*
*	FUNCTION:	InitializeLocals() - Allocation of memory for hash_table,block_table,
*                                    filename_table,identify_table and working buffers 
*                                    to decompress files
*				                      
******************************************************************************/
int InitializeLocals()
{
   glb_mpq->global_buffer = (UInt8 *) malloc(0x60000); // Allocation 384 KB for global_buffer
	if(!glb_mpq->global_buffer) {
		printf("\nError! Insufficient memory");
		return -1;
	}
	glb_mpq->read_buffer_start=glb_mpq->global_buffer;					// 4 KB for read_buffer
	glb_mpq->write_buffer_start=glb_mpq->global_buffer+0x1000;			// 4 KB for write_buffer
	glb_mpq->explode_buffer=glb_mpq->global_buffer+0x2000;				// 16 KB for explode_buffer
	glb_mpq->file_header=(DWORD *)(glb_mpq->global_buffer+0x6000);		// 360 KB for file_header (max size of unpacked file can't exceed 360 MB)

	glb_mpq->hash_table=(DWORD *)malloc(glb_mpq->lenght_htbl*4);
	glb_mpq->block_table=(DWORD *)malloc(glb_mpq->lenght_btbl*4);
	glb_mpq->filename_table=(char *)calloc(glb_mpq->lenght_btbl/4,MPQTYPES_MAX_PATH);
	glb_mpq->identify_table=(char *)calloc(glb_mpq->lenght_btbl/4,sizeof (char));
	if(glb_mpq->hash_table && glb_mpq->block_table && glb_mpq->filename_table && glb_mpq->identify_table)
		return 0;
	else {
		printf("\nError! Insufficient memory");
		return -1;
	}
}

/******************************************************************************
*
*	FUNCTION:	FreeLocals() - free memory
*				                      
******************************************************************************/
void FreeLocals()
{	
	if(glb_mpq->global_buffer)
		free(glb_mpq->global_buffer);
	if(glb_mpq->hash_table)
		free(glb_mpq->hash_table);
	if(glb_mpq->block_table)
		free(glb_mpq->block_table);
	if(glb_mpq->filename_table)
		free(glb_mpq->filename_table);
	if(glb_mpq->identify_table)
		free(glb_mpq->identify_table);
	return;
}

/******************************************************************************
*
*	FUNCTION:	BuildBaseMassive() - fill massive_base
*
******************************************************************************/
void BuildBaseMassive()
{
	DWORD	s1;
	int	i,j;
	ldiv_t divres;

	divres.rem=0x100001;
	for(i=0;i<0x100;i++) {
		for(j=0;j<5;j++) {
			divres=ldiv(divres.rem*125+3,0x002AAAAB);
			s1=(divres.rem&0xFFFFL)<<0x10;
			divres=ldiv(divres.rem*125+3,0x002AAAAB);
			s1=s1|(divres.rem&0xFFFFL);
			glb_mpq->massive_base[i+0x100*j]=s1;
		}
	}
	return;
}

/******************************************************************************
*
*	FUNCTION:	Crc(char *,DWORD *,DWORD) - calculate crc
*
******************************************************************************/
DWORD Crc(char *string,DWORD *massive_base,DWORD massive_base_offset)
{
 char	byte;
 DWORD	crc=0x7fed7fed;
 DWORD	s1=0xEEEEEEEE;
 
 byte=*string;
 while(byte) {
	if(byte>0x60 && byte<0x7B)
		byte-=0x20;
	crc=*(massive_base+massive_base_offset+byte)^(crc+s1);
	s1+=crc+(s1<<5)+byte+3;
	string++;
	byte=*string;
 }
 return crc;
}

/******************************************************************************
*
*	FUNCTION:	read_data(UInt8 *,UInt16,void *) (called by explode)
*
******************************************************************************/
UInt16 read_data(UInt8 *buffer,UInt16 size,void *crap)
{
	params *param=(params *)crap;
	memcpy(buffer,param->buf_in,size);
	param->buf_in+=size;
	return size;
}
/******************************************************************************
*
*	FUNCTION:	write_data(UInt8 *,UInt16,void *) (called by explode)
*
******************************************************************************/
void write_data(UInt8 *buffer,UInt16 size,void *crap)
{
	params *param=(params *)crap;
	memcpy(param->buf_out,buffer,size);
	param->buf_out+=size;
	glb_mpq->lenght_write+=size;
}


/******************************************************************************
*
*	FUNCTION:	Decode(DWORD *,DWORD *,DWORD,DWORD) - decode data
*
******************************************************************************/
void Decode(DWORD *data_in, DWORD *massive_base, DWORD crc, DWORD lenght)
{
 DWORD	i,dec;
 DWORD	s1=0xEEEEEEEE;
 for(i=0;i<lenght;i++) {
	s1+=*(massive_base+0x400+(crc&0xFFL));
	dec=*(data_in+i)^(s1+crc);
	s1+=dec+(s1<<5)+3;
	*(data_in+i)=dec;
	crc=(crc>>0x0b)|(0x11111111+((crc^0x7FFL)<<0x15));
 }
 return;
}

/******************************************************************************
*
*	FUNCTION:	GetUnknowCrc(DWORD) - calculate crc for file without name
*
******************************************************************************/
DWORD GetUnknowCrc(DWORD entry)
{	
	DWORD	tmp,i,j,coded_dword,crc_file=0;
	DWORD	flag,size_pack,size_unpack,num_block,offset_body;
	DWORD	sign_riff1=0x46464952; // 'RIFF'
	DWORD	sign_riff3=0x45564157; // 'WAVE'
	DWORD	sign_mpq1=0x1a51504d; // 'MPQ'
	DWORD	sign_mpq2=0x00000020;
	ldiv_t	divres;

	offset_body=*(glb_mpq->block_table+entry*4);								// get offset of analized file
	flag=*(glb_mpq->block_table+entry*4+3);									// get flag of analized file
	fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
	fread(&coded_dword,sizeof(DWORD),1,glb_mpq->fpMpq);						// read first coded dword from file

	if(flag&0x200 || flag&0x100) {								// IF FILE PACKED:
		size_unpack=*(glb_mpq->block_table+entry*4+2);						// . get size of unpacked file
		size_pack=*(glb_mpq->block_table+entry*4+1);							// . get size of packed file
		divres=ldiv(size_unpack-1,0x1000);
		num_block=divres.quot+2;									// . calculate lenght of file header
		for(j=0;j<=0xff;j++) {										// . now we're gonna find crc_file of 0x100 possible variants
			crc_file=((num_block*4)^coded_dword)-0xeeeeeeee - *(glb_mpq->massive_base+0x400+j);// . calculate possible crc
			if((crc_file&0xffL) == j) {								// . IF FIRST CHECK is succesfull - do second one
				fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
				fread(glb_mpq->file_header,sizeof(DWORD),num_block,glb_mpq->fpMpq);	// . read file header 
				Decode(glb_mpq->file_header,glb_mpq->massive_base,crc_file,num_block);// . decode file header with possible crc
				tmp=num_block*4;									// . tmp = size header (bytes)
				if(tmp == *glb_mpq->file_header) {							// . IF SECOND CHECK is succesfull - do third one
					for(i=0;i<num_block-1;i++) {
	  					tmp+=*(glb_mpq->file_header+i+1)-*(glb_mpq->file_header+i);
						if(*(glb_mpq->file_header+i+1)-*(glb_mpq->file_header+i)>0x1000) {
							tmp=0xffffffff;
							break;
						}
					}
					if(tmp!=0xffffffff) {							// . IF THIRD CHECK is succesfull
						crc_file++;									// . great! we got right crc_file
						break;
					}
				}
			}
			crc_file=0;												// . if its impossible to get right crc return 0
		}

	} else {													// IF FILE IS NOT PACKED:
		for(j=0;j<=0xff;j++) {										// Calculate crc as if it was WAV FILE
			crc_file=(sign_riff1^coded_dword)-0xeeeeeeee - *(glb_mpq->massive_base+0x400+j);// . calculate possible crc
			if((crc_file&0xff)==j) {								// . IF FIRST CHECK is succesfull - do second one
				fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
				fread(glb_mpq->file_header,sizeof(DWORD),3,glb_mpq->fpMpq);			// . read file file_header 
				Decode(glb_mpq->file_header,glb_mpq->massive_base,crc_file,3);		// . decode file file_header with possible crc
				if(sign_riff1==*glb_mpq->file_header) {
					if(sign_riff3==*(glb_mpq->file_header+2))				// . IF SECOND CHECK is succesfull - we got right crc
						break;
				}
			}
			crc_file=0;												// . if its impossible to get right crc return 0
		}
		if(!crc_file) {												// Calculate crc as if it was MPQ FILE
			for(j=0;j<=0xff;j++) {
				crc_file=(sign_mpq1^coded_dword)-0xeeeeeeee - *(glb_mpq->massive_base+0x400+j);
				if((crc_file&0xffL) == j) {
					fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
					fread(glb_mpq->file_header,sizeof(DWORD),2,glb_mpq->fpMpq);
					Decode(glb_mpq->file_header,glb_mpq->massive_base,crc_file,2);
					if(sign_mpq1 == *glb_mpq->file_header) {
						if(sign_mpq2 == *(glb_mpq->file_header+1))
							break;
					}
				}
				crc_file=0;
			}
		}
	}
	return crc_file;
}

/******************************************************************************
*
*	FUNCTION:	ExtractTo(FILE,DWORD) - extract file from archive
*
******************************************************************************/
int ExtractTo(FILE *fp_new,DWORD entry)
{
	DWORD	size_pack,size_unpack;
	UInt8	*read_buffer,*write_buffer;
	UInt32	i,j,offset_body,flag,crc_file=0;
	UInt32	num_block,lenght_read,iteration;
	UInt8	*szNameFile;
	UInt8	metod;
	ldiv_t	divres;
	params	param;
 
	offset_body=*(glb_mpq->block_table+entry*4);							// get offset of file in mpq
	size_unpack=*(glb_mpq->block_table+entry*4+2);						// get unpacked size of file
	flag=*(glb_mpq->block_table+entry*4+3);								// get flags for file

	if(flag&0x30000) {										// If file is coded, calculate its crc
		if(*(glb_mpq->identify_table+entry)&0x1) {						// . Calculate crc_file for identified file:
			szNameFile=glb_mpq->filename_table+MPQTYPES_MAX_PATH*entry;			// . . get name of file
			if(strrchr(szNameFile,'\\'))
				szNameFile=strrchr(szNameFile,'\\')+1;
			crc_file=Crc(szNameFile,glb_mpq->massive_base,0x300);		// . . calculate crc_file (for Diablo I MPQs)
			if((flag&0x20200) == 0x20200)									// . . if flag indicates Starcraft MPQ
				crc_file=(crc_file+offset_body)^size_unpack;	// . . calculate crc_file (for Starcraft MPQs)
		}
		else
			crc_file=GetUnknowCrc(entry);						// . calculate crc_file for not identified file:
	}

	if(flag&0x200 || flag&0x100) {							// IF FILE IS PACKED:
		divres=ldiv(size_unpack-1,0x1000);
		num_block=divres.quot+2;								// . calculate lenght of file header
		fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
		fread(glb_mpq->file_header,sizeof(DWORD),num_block,glb_mpq->fpMpq);		// . read file header 
		if(flag&0x30000)
			Decode(glb_mpq->file_header,glb_mpq->massive_base,(crc_file-1),num_block);// . decode file header (if file is coded)
		read_buffer=glb_mpq->read_buffer_start;
		for(j=0;j<(num_block-1);j++) {
			lenght_read=*(glb_mpq->file_header+j+1)-*(glb_mpq->file_header+j);	// . get lenght of block to read
			fread(read_buffer,sizeof(char),lenght_read,glb_mpq->fpMpq);	// . read block
			if(flag&0x30000)
				Decode((DWORD *)read_buffer,glb_mpq->massive_base,crc_file,lenght_read/4);			// . decode block (if file is coded)
			if(lenght_read==0x1000 || (j==num_block-2 && lenght_read==(size_unpack&0xFFF)))	// . if block is unpacked (its lenght=0x1000 or its last block and lenght=remainder)
//
					fwrite(read_buffer,sizeof(char),lenght_read,fp_new);					// . write block "as is"
			else {												// . block is packed
				if(flag&0x200) {								// . If this file is from Starcraft MPQ (or Diablo 2), then
					metod=*read_buffer;							// . . first byte determinates metod of packing
					iteration=0;
					for(i=0;i<4;i++) {							// . . calculate number of iterations
						if(metod&glb_mpq->avail_metods[i])
							iteration++;
					}
					read_buffer+=1;
					lenght_read-=1;
				} else {										// . Else: file is from Diablo I MPQ, then
					iteration=1;
					metod=8;									// . .file is compressed with DCL
				}
				write_buffer=glb_mpq->write_buffer_start;
				if(metod&0x08) {
					param.buf_in =read_buffer;
					param.buf_out=write_buffer;
					glb_mpq->lenght_write=0;
					explode(&read_data,&write_data,&param);
					lenght_read=glb_mpq->lenght_write;
					iteration--;
					if(iteration) {
						read_buffer=write_buffer;
						write_buffer=glb_mpq->read_buffer_start;
					}
				}
				if(metod&0x01) {
					lenght_read=ExtWavUnp1((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
					iteration--;
					if(iteration) {
						read_buffer=write_buffer;
						write_buffer=glb_mpq->read_buffer_start;
					}
				}
				if(metod&0x40)
					lenght_read=ExtWavUnp2((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
				if(metod&0x80)
					lenght_read=ExtWavUnp3((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
//
				fwrite(write_buffer,1,lenght_read,fp_new);
				read_buffer=glb_mpq->read_buffer_start;
			}
			crc_file++;											// . calculate crc_file for next block
		}
	}

	else {													// IF FILE IS NOT PACKED
		size_pack=*(glb_mpq->block_table+entry*4+1);					// get size  of file
		if(flag&0x30000)
			lenght_read=0x1000;								// if file is coded, lenght_read=0x1000 (4 KB)
		else
			lenght_read=0x60000;							// if file is not coded, lenght_read=0x60000 (384KB)
		if(size_pack<lenght_read)
			lenght_read=size_pack;							// if size of file < lenght_read, lenght read = size of file
		read_buffer=glb_mpq->read_buffer_start;
		if(lenght_read) {
			divres=ldiv(size_pack,lenght_read);					// .
			num_block=divres.quot;								// .
		} else {												// .
			num_block=0;										// .
			divres.rem=0;										// .
		}
		fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
		for (j=0;j<num_block;j++) {
			fread(read_buffer,1,lenght_read,glb_mpq->fpMpq);
			if(flag&0x30000) {
				Decode((DWORD *)read_buffer,glb_mpq->massive_base,crc_file,lenght_read/4);	// if file is coded, decode block
				crc_file++;										// and calculate crc_file for next block
			}
//
			fwrite(read_buffer,1,lenght_read,fp_new);
		}
		if(divres.rem) {
			fread(read_buffer,1,divres.rem,glb_mpq->fpMpq);
			if(flag&0x30000)
				Decode((DWORD *)read_buffer,glb_mpq->massive_base,crc_file,divres.rem/4);
//
			fwrite(read_buffer,1,divres.rem,fp_new);
		}
	}
	return 0;
}

/******************************************************************************
*
*	FUNCTION:	ExtractToMem(FILE,DWORD) - extract file from archive
*
******************************************************************************/
int ExtractToMem(void * mp_new, DWORD entry)
{
	DWORD  size_pack,size_unpack;
	UInt8  * read_buffer,*write_buffer;
	UInt32 i,j,offset_body,flag,crc_file=0;
	UInt32 num_block,lenght_read,iteration;
	UInt8	 * szNameFile;
	UInt8  metod;
	ldiv_t divres;
	params param;
   UBYTE  * buff_ptr = mp_new;
 
	offset_body=*(glb_mpq->block_table+entry*4);							// get offset of file in mpq
	size_unpack=*(glb_mpq->block_table+entry*4+2);						// get unpacked size of file
	flag=*(glb_mpq->block_table+entry*4+3);								// get flags for file

	if(flag&0x30000) {										// If file is coded, calculate its crc
		if(*(glb_mpq->identify_table+entry)&0x1) {						// . Calculate crc_file for identified file:
			szNameFile=glb_mpq->filename_table+MPQTYPES_MAX_PATH*entry;			// . . get name of file
			if(strrchr(szNameFile,'\\'))
				szNameFile=strrchr(szNameFile,'\\')+1;
			crc_file=Crc(szNameFile,glb_mpq->massive_base,0x300);		// . . calculate crc_file (for Diablo I MPQs)

         /* edit by Sloan Roy 17 Nov 2002 : must be
               if ((flag & 0x20200) == 0x20200)
            instead of
               if (flag & 0x20200) */
               
         if ((flag & 0x20200) == 0x20200)									// . . if flag indicates Starcraft MPQ
         
				crc_file=(crc_file+offset_body)^size_unpack;	// . . calculate crc_file (for Starcraft MPQs)
		}
		else
			crc_file=GetUnknowCrc(entry);						// . calculate crc_file for not identified file:
	}

	if(flag&0x200 || flag&0x100) {							// IF FILE IS PACKED:
		divres=ldiv(size_unpack-1,0x1000);
		num_block=divres.quot+2;								// . calculate lenght of file header
		fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
		fread(glb_mpq->file_header,sizeof(DWORD),num_block,glb_mpq->fpMpq);		// . read file header 
		if(flag&0x30000)
			Decode(glb_mpq->file_header,glb_mpq->massive_base,(crc_file-1),num_block);// . decode file header (if file is coded)
		read_buffer=glb_mpq->read_buffer_start;
		for(j=0;j<(num_block-1);j++) {
			lenght_read=*(glb_mpq->file_header+j+1)-*(glb_mpq->file_header+j);	// . get lenght of block to read
			fread(read_buffer,sizeof(char),lenght_read,glb_mpq->fpMpq);	// . read block
			if(flag&0x30000)
				Decode((DWORD *)read_buffer,glb_mpq->massive_base,crc_file,lenght_read/4);			// . decode block (if file is coded)
			if(lenght_read==0x1000 || (j==num_block-2 && lenght_read==(size_unpack&0xFFF)))	// . if block is unpacked (its lenght=0x1000 or its last block and lenght=remainder)
         {
//
//					fwrite(read_buffer,sizeof(char),lenght_read,fp_new);					// . write block "as is"
					memcpy(buff_ptr, read_buffer, lenght_read);
               buff_ptr += lenght_read;
         }
			else {												// . block is packed
				if(flag&0x200) {								// . If this file is from Starcraft MPQ (or Diablo 2), then
					metod=*read_buffer;							// . . first byte determinates metod of packing
					iteration=0;
					for(i=0;i<4;i++) {							// . . calculate number of iterations
						if(metod&glb_mpq->avail_metods[i])
							iteration++;
					}
					read_buffer+=1;
					lenght_read-=1;
				} else {										// . Else: file is from Diablo I MPQ, then
					iteration=1;
					metod=8;									// . .file is compressed with DCL
				}
				write_buffer=glb_mpq->write_buffer_start;
				if(metod&0x08) {
					param.buf_in =read_buffer;
					param.buf_out=write_buffer;
					glb_mpq->lenght_write=0;
					explode(&read_data,&write_data,&param);
					lenght_read=glb_mpq->lenght_write;
					iteration--;
					if(iteration) {
						read_buffer=write_buffer;
						write_buffer=glb_mpq->read_buffer_start;
					}
				}
				if(metod&0x01) {
					lenght_read=ExtWavUnp1((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
					iteration--;
					if(iteration) {
						read_buffer=write_buffer;
						write_buffer=glb_mpq->read_buffer_start;
					}
				}
				if(metod&0x40)
					lenght_read=ExtWavUnp2((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
				if(metod&0x80)
					lenght_read=ExtWavUnp3((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
//
//				fwrite(write_buffer,1,lenght_read,fp_new);
				memcpy(buff_ptr, write_buffer, lenght_read);
            buff_ptr += lenght_read;

				read_buffer=glb_mpq->read_buffer_start;
			}
			crc_file++;											// . calculate crc_file for next block
		}
	}

	else {													// IF FILE IS NOT PACKED
		size_pack=*(glb_mpq->block_table+entry*4+1);					// get size  of file
		if(flag&0x30000)
			lenght_read=0x1000;								// if file is coded, lenght_read=0x1000 (4 KB)
		else
			lenght_read=0x60000;							// if file is not coded, lenght_read=0x60000 (384KB)
		if(size_pack<lenght_read)
			lenght_read=size_pack;							// if size of file < lenght_read, lenght read = size of file
		read_buffer=glb_mpq->read_buffer_start;
		if(lenght_read) {
			divres=ldiv(size_pack,lenght_read);					// .
			num_block=divres.quot;								// .
		} else {												// .
			num_block=0;										// .
			divres.rem=0;										// .
		}
		fseek(glb_mpq->fpMpq,glb_mpq->offset_mpq+offset_body,SEEK_SET);
		for (j=0;j<num_block;j++) {
			fread(read_buffer,1,lenght_read,glb_mpq->fpMpq);
			if(flag&0x30000) {
				Decode((DWORD *)read_buffer,glb_mpq->massive_base,crc_file,lenght_read/4);	// if file is coded, decode block
				crc_file++;										// and calculate crc_file for next block
			}
//
//			fwrite(read_buffer,1,lenght_read,fp_new);
			memcpy(buff_ptr, read_buffer, lenght_read);
         buff_ptr += lenght_read;
		}
		if(divres.rem) {
			fread(read_buffer,1,divres.rem,glb_mpq->fpMpq);
			if(flag&0x30000)
				Decode((DWORD *)read_buffer,glb_mpq->massive_base,crc_file,divres.rem/4);
//
//			fwrite(read_buffer,1,divres.rem,fp_new);
			memcpy(buff_ptr, read_buffer, divres.rem);
         buff_ptr += divres.rem;
		}
	}
	return 0;
}

#ifdef WIN32
   #pragma warning (pop)
#endif
