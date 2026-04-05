#ifdef WIN32
   #pragma warning (push)
   #pragma warning (disable  :  4244 4306 4311 4312 )
#endif

/* DCL Explode */

#include <stdio.h>
#include "DCLib.h"

static UInt16 __explode_1 (UInt8 *buf);
static UInt16 __explode_2 (UInt8 *buf);
static UInt16 __explode_3 (UInt8 *buf, UInt16 result);
static UInt16 __explode_4 (UInt8 *buf, UInt32 flag);
static void   __explode_5 (UInt16 count, UInt8 *buf_1, const UInt8 *table, UInt8 *buf_2);
static void   __explode_6 (UInt8 *buf, const UInt8 *table);
       void   __explode_7 (UInt8 *buf, const UInt8 *table, UInt32 count);


//extern UInt8 * explode_buffer;

// ==========================================================================
UInt32 explode(read_data_proc read_data, write_data_proc write_data, void * param)
{	
   UInt32      result;
   UInt16      read_result;
   const UInt8 * table = dcl_table;
   UInt8       * work_buff;


//	UInt8 *work_buff=(UInt8 *)malloc(EXP_BUFFER_SIZE);
	work_buff = (UInt8 *) glb_mpq->explode_buffer;
	
	*((UInt32 *) (work_buff + 0x16)) = (UInt32) read_data;
	*((UInt32 *) (work_buff + 0x1A)) = (UInt32) write_data;
	*((void **) (work_buff + 0x12)) = param;
	*((UInt16 *) (work_buff + 0x0E)) = 0x0800;
	read_result = read_data(work_buff + 0x2222, 0x0800, param);
	if (read_result == DCL_ERROR_4)
	{
		result = DCL_ERROR_3;
	}
	else
	{
		UInt16	flag_0 = *(work_buff + 0x2222),
				flag_1 = *(work_buff + 0x2223),
				flag_2 = *(work_buff + 0x2224);
		
		*((UInt16 *) (work_buff + 0x02)) = flag_0;
		*((UInt16 *) (work_buff + 0x06)) = flag_1;
		*((UInt16 *) (work_buff + 0x0A)) = flag_2;
		*((UInt16 *) (work_buff + 0x0C)) = 0x00;
		*((UInt16 *) (work_buff + 0x0E)) = 0x03;
		*((UInt16 *) (work_buff + 0x10)) = read_result;
		if ((flag_1 < 0x04) || (flag_1 > 0x06))
		{
			result = DCL_ERROR_1;
		}
		else
		{
			 *((UInt16 *) (work_buff + 0x08)) = (UInt16) (0x0000FFFFL >> (0x0010 - flag_1));
			 if (flag_0 > 0x01)
			 {
			 	result = DCL_ERROR_2;
			 }
			 else
			 {
			 	if (flag_0)
			 	{
		 			__explode_7(work_buff + 0x2FA2, table + 0x00D0, 0x0100);
		 			__explode_6(work_buff, table + 0x01D0);
			 	}
			 	__explode_7(work_buff + 0x30E2, table + 0x00B0, 0x0010);
			 	__explode_5(0x0010, work_buff + 0x30E2, table + 0x00C0, work_buff + 0x2B22);
			 	__explode_7(work_buff + 0x30F2, table + 0x0080, 0x0010);
			 	__explode_7(work_buff + 0x3102, table + 0x0090, 0x0020);
			 	__explode_7(work_buff + 0x30A2, table, 0x0040);
			 	__explode_5(0x0040, work_buff + 0x30A2, table + 0x0040, work_buff + 0x2A22);
			 	if (__explode_1(work_buff) != 0x0306)
			 	{
			 		result = DCL_NO_ERROR;
			 	}
			 	else
			 	{
			 		result = DCL_ERROR_4;
			 	}
			 }
		}
	}
//	free(work_buff);
	return result;
}


// ==========================================================================
UInt16 __explode_1(UInt8 *buf)
{
	UInt32			result, temp;
	UInt8			*s, *d;
	write_data_proc	*write_data;
	void			*param;
	
	*((UInt16 *) (buf + 0x04)) = 0x1000;
	while (result = __explode_2(buf), (UInt16) result < 0x0305)
	{
		if ((UInt16) result < 0x0100)
		{
			temp = *((UInt16 *) (buf + 0x04));
			*((UInt16 *) (buf + 0x04)) = (UInt16) (temp + 0x01);
			*(buf + temp + 0x1E) =(UInt8) result;
		}
		else
		{
			result -= 0x00FE;
			s = (UInt8 *) __explode_3(buf, (UInt16)result);
			if (!s)
			{
				result = 0x0306;
				break;
			}
			else
			{
				temp = *((UInt16 *) (buf + 0x04));
				d = temp + 0x1E + buf;
				*((UInt16 *) (buf + 0x04)) =(UInt16) (temp + result);
				s = (UInt8 *)((UInt32)d - (UInt32)s);
				do
				{
					result--;
					*(d++) = *(s++);
				} while (result); 
			}			
		}
		if (*((UInt16 *) (buf + 4)) >= 0x2000)
		{
			result = 0x1000;
			write_data = (write_data_proc *) *((UInt32 *) (buf + 0x1A));
			param = (void *) *((UInt32 *) (buf + 0x12));
			write_data(buf + 0x101E, 0x1000, param);
			__explode_7(buf + 0x001E, buf + 0x101E, *((UInt16 *) (buf + 0x04)) - 0x1000);
			*((UInt16 *) (buf + 0x04)) -= 0x1000;
		}
	}
	write_data = (write_data_proc *) *((UInt32 *) (buf + 0x1A));
	param = (void *) *((UInt32 *) (buf + 0x12));
	write_data(buf + 0x101E, (UInt16)(*((UInt16 *) (buf + 0x04)) - 0x1000), param);	
	return (UInt16) result;
}


// ==========================================================================
UInt16 __explode_2(UInt8 *buf)
{
	UInt32	result, flag, flag_1;

	if (*((UInt16 *) (buf + 0x0A)) & 0x01)
	{
		if (__explode_4(buf, 0x01)) return 0x0306;
		result = *(buf + ((UInt8) *((UInt16 *) (buf + 0x0A))) + 0x2B22);
		if (__explode_4(buf, *(buf + ((UInt16) result) + 0x30E2))) return 0x0306;
		flag = *(buf + ((UInt16) result) + 0x30F2);
		if (flag)
		{
			flag_1 = (*((UInt16 *) (buf + 0x0A))) & ((0x01 << flag) - 0x01);
			if (__explode_4(buf, flag))
			{
				if ((((UInt16) result) + ((UInt16) flag_1)) != 0x010E) return 0x0306;
			}
			result = *((UInt16 *) (buf + (((UInt16) result) << 0x01) + 0x3102)) + flag_1;
		}
		result += 0x0100;
	}
	else
	{
		if (__explode_4(buf, 0x01)) return 0x0306;
		if (!*((UInt16 *) (buf + 0x02)))
		{
			result = (UInt8) *((UInt16 *) (buf + 0x0A));
			if (__explode_4(buf, 0x08)) return 0x0306;
		}
		else
		{
			flag = *((UInt16 *) (buf + 0x0A));
			if ((UInt8) flag)
			{
				result = *(buf + ((UInt8) flag) + 0x2C22);
				if (((UInt16) result) == 0x00FF)
				{
					if (flag & 0x003F)
					{
						if (__explode_4(buf, 0x04)) return 0x0306;
						result = *(buf + ((UInt8) *((UInt16 *) (buf + 0x0A))) + 0x2D22);
					}
					else
					{
						if (__explode_4(buf, 0x06)) return 0x0306;
						result = *(buf + ((*((UInt16 *) (buf + 0x0A))) & 0x007F) + 0x2E22);
					}
				}
			}
			else
			{
				if (__explode_4(buf, 0x08)) return 0x0306;
				result = *(buf + ((UInt8) *((UInt16 *) (buf + 0x0A))) + 0x2EA2);
			}
			flag = *(buf + ((UInt16) result) + 0x2FA2);
			if (__explode_4(buf, flag)) return 0x0306;
		}
	}
	return (UInt16)result;
}


// ==========================================================================
UInt16 __explode_3(UInt8 *buf, UInt16 flag)
{
	UInt32	result, flag_1;
	
	result = *(buf + ((UInt8) *((UInt16 *) (buf + 0x0A))) + 0x2A22);
	if (__explode_4(buf, *(buf + ((UInt16) result) + 0x30A2))) return 0x00;
	if (((UInt16) flag) == 0x02)
	{
		result <<= 0x02;
		result |= *((UInt16 *) (buf + 0x0A)) & 0x03;
		if (__explode_4(buf, 0x02)) return 0x00;
	}
	else
	{
		flag_1 = *((UInt16 *) (buf + 0x06));
		result <<= flag_1;
		result |= *((UInt16 *) (buf + 0x08)) & *((UInt16 *) (buf + 0x0A));
		if (__explode_4(buf, flag_1)) return 0x00;
	}
	return (UInt16) (result + 0x01);
}


// ==========================================================================
UInt16 __explode_4(UInt8 *buf, UInt32 flag)
{
	UInt32			result;
	UInt16			read_result;
	read_data_proc	*read_data = (read_data_proc *) *((UInt32 *) (buf + 0x16));
	void			*param = (void *) *((UInt32 *) (buf + 0x12));
	
	result = *((UInt16 *) (buf + 0x0C));
	if ((UInt16) flag <= result)
	{
		*((UInt16 *) (buf + 0x0A)) >>= flag;
		*((UInt16 *) (buf + 0x0C)) -= (UInt16)flag;
		result = 0x00;
	}
	else
	{
		*((UInt16 *) (buf + 0x0A)) >>= result;
		result = *((UInt16 *) (buf + 0x0E));
		if (result == *((UInt16 *) (buf + 0x10)))
		{
			*((UInt16 *) (buf + 0x0E)) = 0x0800;
			read_result = read_data(buf + 0x2222, 0x0800, param);
			*((UInt16 *) (buf + 0x10)) = read_result;
			if (!read_result) return 0x01;
			*((UInt16 *) (buf + 0x0E)) = 0x00;
		}
		result = *((UInt16 *) (buf + 0x0E)) + 0x2222;
		*((UInt16 *) (buf + 0x0A)) |= *(buf + result) << 0x08;
		*((UInt16 *) (buf + 0x0E)) += 0x01;
		*((UInt16 *) (buf + 0x0A)) >>= flag - *((UInt16 *) (buf + 0x0C));
		*((UInt16 *) (buf + 0x0C)) = (UInt16)(0x08 - (flag - *((UInt16 *) (buf + 0x0C))));
		result = 0x00;
	}
	return (UInt16) result;
}


// ==========================================================================
void __explode_5(UInt16 count, UInt8 *buf_1, const UInt8 *table, UInt8 *buf_2)
{
	SInt16	i = (SInt16)(count - 1);
	UInt32	idx_1, idx_2;
	
	for (; i >= 0; i--)
	{
		idx_1 = *(table + i);
		idx_2 = 0x01 << *(buf_1 + i);
		for (;;)
		{
			*(buf_2 + (UInt16) idx_1) = (UInt8) i;
			idx_1 += idx_2;
			if ((UInt16) idx_1 >= 0x0100) break;
		}
	} 
}


// ==========================================================================
void __explode_6(UInt8 *buf, const UInt8 *table)
{
	SInt16	i;
	UInt32	idx_1, idx_2;
	
	for (i = 0x00FF; i >= 0; i--)
	{		
		idx_1 = *(buf + i + 0x2FA2);
		if (idx_1 <= 0x08 )
		{
			idx_2 = *((UInt16 *) (table + (i << 0x01)));
			idx_1 = 0x01 << idx_1;
			do
			{
				*(buf + idx_2 + 0x2C22) =(UInt8) i;
				idx_2 += idx_1;
			} while ((UInt16) idx_2 < 0x0100);
		}
		else
		{
			idx_2 = *((UInt16 *) (table + (i << 0x01)));
			if ((UInt8) idx_2)
			{
				
				*(buf + (UInt8) idx_2 + 0x2C22) = 0xFF;
				if (*((UInt16 *) (table + (i << 0x01))) & 0x003F)
				{
					*(buf + i + 0x2FA2) -= 0x04;
					idx_1 = 0x01 << *(buf + i + 0x2FA2);
					idx_2 = *((UInt16 *) (table + (i << 0x01))) >> 0x04;
					do
					{
						*(buf + idx_2 + 0x2D22) =(UInt8) i;
						idx_2 += idx_1;
					} while ((UInt16) idx_2 < 0x0100);
				}
				else
				{
					*(buf + i + 0x2FA2) -= 0x06;
					idx_1 = 0x01 << *(buf + i + 0x2FA2);
					idx_2 = *((UInt16 *) (table + (i << 0x01))) >> 0x06;
					do
					{
						*(buf + idx_2 + 0x2E22) = (UInt8)i;
						idx_2 += idx_1;
					} while ((UInt16) idx_2 < 0x0080);
				}
			}
			else
			{
				*(buf + i + 0x2FA2) -= 0x08;
				idx_1 = 0x01 << *(buf + i + 0x2FA2);
				idx_2 = *((UInt16 *) (table + (i << 0x01))) >> 0x08;
				do
				{
					*(buf + idx_2 + 0x2EA2) = (UInt8)i;
					idx_2 += idx_1;
				} while ((UInt16) idx_2 < 0x0100);
			}
		}
	}
}


// ==========================================================================
void __explode_7(UInt8 *buf, const UInt8 *table, UInt32 count)
{
	UInt32	half_count = count >> 0x02;
	UInt8	*buf_end;
	
	if (half_count)
	{
		buf_end = buf + (half_count << 0x02);
		do
		{
			*((UInt32 *) buf) = *((UInt32 *) table);
			buf += 4;
			table += 4;
		} while (buf < buf_end);
	}
	switch (count - (half_count << 0x02))
	{
		case 3:
			*(buf++) = *(table++);
			
		case 2:
			*(buf++) = *(table++);
			
		case 1:
			*buf = *table;
			
		default:
			break;
	}
}

#ifdef WIN32
   #pragma warning (pop)
#endif
