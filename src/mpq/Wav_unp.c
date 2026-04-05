#ifdef WIN32
   #pragma warning (push)
   #pragma warning (disable  :  4100 4127 4311 4312 4127)
#endif

/* WAVE decompressor */

#include "mpqTypes.h"
#include "Wav_unp.h"
#include <stdlib.h>

UInt32 ExtWavUnp1   (UInt32,UInt32,UInt32,UInt32);
UInt32 ExtWavUnp2   (UInt32,UInt32,UInt32,UInt32);
UInt32 ExtWavUnp3   (UInt32,UInt32,UInt32,UInt32);
void   Sub_WavUnp1  (UInt32,UInt32);
UInt32 Sub_WavUnp2  (UInt32,UInt32,UInt32);
void   Sub_WavUnp3  (UInt32);
void   Sub_WavUnp4  (UInt32,UInt32,UInt32,UInt32);
UInt32 Sub_WavUnp5  (UInt32);
void   Sub_WavUnp6  (UInt32);
UInt32 Sub_WavUnp7  (UInt32);
UInt32 Sub_WavUnp8  (UInt32);
void   Sub_WavUnp9  (UInt32,UInt32);
void   Sub_WavUnp10 (UInt32,UInt32);
UInt32 Sub_WavUnp11 (UInt32,UInt32,UInt32,UInt32);
void   Sub_WavUnp12 (UInt32);
UInt32 Sub_WavUnp13 (UInt32,UInt32,UInt32,UInt32,UInt32);


// ==========================================================================
UInt32 ExtWavUnp3(UInt32 buf_in,UInt32 size_in,UInt32 buf_out,UInt32 size_out)
{
	return Sub_WavUnp13(buf_in,size_in,2,buf_out,size_out);
}


// ==========================================================================
UInt32 ExtWavUnp2(UInt32 buf_in,UInt32 size_in,UInt32 buf_out,UInt32 size_out)
{
	return Sub_WavUnp13(buf_in,size_in,1,buf_out,size_out);
}


// ==========================================================================
UInt32 Sub_WavUnp13(UInt32 buf_in,UInt32 size_in,UInt32 flag,UInt32 buf_out,UInt32 size_out)
{
	UInt32	var14=0,var18=0,var1c=0;
	UInt32	tmp0=0,tmp1=0,tmp2=0,tmp3=0,tmp4=0;
   UInt32	var8[2]={0, 0};
	UInt32	var10[2]={0, 0};


	var14=buf_in;
	size_in+=buf_in;
	tmp0=var14+2;
	var1c=size_out;
	tmp3=buf_out;
	var18=buf_out;
	var10[0]=0x2C;
	var10[1]=0x2C;
	if((SInt32)flag>0) {
		for(tmp2=0;tmp2<flag;tmp2++) {
			var8[tmp2]=(SInt32)*((SInt16 *)tmp0);
			tmp0+=2;
			if(size_out<2)
				return tmp3-var18;
			*((UInt16 *)tmp3)=(UInt16) var8[tmp2];
			tmp3+=2;
			var1c-=2;
		}
	}
	tmp2=flag-1;
	while(tmp0<size_in) {	
		if(flag==2)	{
			if(tmp2==0)
				tmp2=1;
			else
				tmp2=0;
		}
		tmp1=(UInt32)*((UInt8 *)tmp0);
		tmp0++;
		buf_in=tmp0;
		if(tmp1&0x80) {
			tmp1&=0x7F;
			tmp4=1;
			if(tmp1==0)	{
				if(var10[tmp2])
					var10[tmp2]--;
				if(var1c<2)
					return tmp3-var18;
				*((UInt16 *)tmp3)=(UInt16)var8[tmp2];
				tmp3+=2;
				var1c-=2;
				if((tmp4==0)&&(flag==2)) {
					if(tmp2==0)
						tmp2=1;
					else
						tmp2=0;
				}
			} else {
				tmp1--;
				if(tmp1==0)	{
					var10[tmp2]+=8;
					tmp4=var10[tmp2];
					if(tmp4>0x58)
						var10[tmp2]=0x58;
					tmp4=0;
					if(tmp4==0 && flag==2) {
						if(tmp2==0)
							tmp2=1;
						else
							tmp2=0;
					}
				} else {
					tmp1--;
					if(tmp1==0)	{
						var10[tmp2]+=0xFFFFFFF8;
						if((SInt32)var10[tmp2]<0)
							var10[tmp2]=0;
						tmp4=0;
					}
					if((tmp4==0)&&(flag==2)) {
						if(tmp2==0)
							tmp2=1;
						else
							tmp2=0;
					}
				}
			}
		} else {
			tmp0=small_tbl1[var10[tmp2]];			
			tmp4=tmp0>>(*((UInt8 *)(var14+1)));
			if(tmp1&1)
				tmp4+=tmp0;
			if(tmp1&2)
				tmp4+=tmp0>>1;
			if(tmp1&4)
				tmp4+=tmp0>>2;
			if(tmp1&8)
				tmp4+=tmp0>>3;
			if(tmp1&0x10)
				tmp4+=tmp0>>4;
			if(tmp1&0x20)
				tmp4+=tmp0>>5;
			tmp0=var8[tmp2];
			if((tmp1&0xFF)&0x40) {
				tmp0-=tmp4;
				if((SInt32)tmp0<=(SInt32)0xFFFF8000)
					tmp0=0xFFFF8000;
			} else {
				tmp0+=tmp4;
				if((SInt32)tmp0>=0x7FFF)
					tmp0=0x7FFF;
			}
			var8[tmp2]=tmp0;
			tmp0=var1c;
			if(tmp0<2)
				return tmp3-var18;
			*((UInt16 *)tmp3)=(UInt16)var8[tmp2];
			tmp1&=0x1F;
			tmp3+=2;
			var1c-=2;
			tmp4=var10[tmp2]+small_tbl2[tmp1];
			var10[tmp2]=tmp4;
			if((SInt32)tmp4<0)
				var10[tmp2]=0;
			else {
				if(tmp4>0x58)
					var10[tmp2]=0x58;
			}
			tmp0=buf_in;
		}
	}
	return tmp3-var18;
}


// ==========================================================================
UInt32 ExtWavUnp1(UInt32 buf_in, UInt32 size_in, UInt32 buf_out, UInt32 size_out)
{
	UInt32	work_buff=0,base=0;
	UInt32	tmp1=0,tmp2=0,tmp3=0;
	SInt32	i=0;


	work_buff=(UInt32)(UInt8 *)malloc(0x3a80);
	*((UInt32 *)work_buff)=buf_in+4;
	*((UInt32 *)(work_buff+4))=*((UInt32 *)buf_in);
	*((UInt32 *)(work_buff+8))=0x20;
	base=work_buff+0xC;
	Sub_WavUnp12(base);
	size_out=Sub_WavUnp11(buf_out,size_out,work_buff,base);
	while(1) {
		tmp2=*((UInt32 *)(work_buff+0x3070));
		if((SInt32)tmp2<=0)
			break;
		tmp3=Sub_WavUnp5(tmp2);
		*((UInt32 *)tmp3)=*((UInt32 *)tmp2);
		tmp3=*((UInt32 *)tmp2);
		*((UInt32 *)(tmp3+4))=*((UInt32 *)(tmp2+4));
		*((UInt32 *)tmp2)=0;
		*((UInt32 *)(tmp2+4))=0;
	}
	if(*((UInt32 *)(work_buff+0x306C))) {
		tmp3=Sub_WavUnp5(*((UInt32 *)(work_buff+0x306C)));
		*((UInt32 *)tmp3)=*((UInt32 *)(work_buff+0x306C));
		tmp3=*((UInt32 *)(work_buff+0x306C));
		*((UInt32 *)(tmp3+4))=*((UInt32 *)(work_buff+0x3070));
		*((UInt32 *)(work_buff+0x306C))=0;
		*((UInt32 *)(work_buff+0x3070))=0;
	}
	while(1) {
		tmp2=*((UInt32 *)(work_buff+0x3064));
		if((SInt32)tmp2<=0)
			break;
		tmp3=Sub_WavUnp5(tmp2);
		*((UInt32 *)tmp3)=*((UInt32 *)tmp2);
		tmp3=*((UInt32 *)tmp2);
		*((UInt32 *)(tmp3+4))=*((UInt32 *)(tmp2+4));
		*((UInt32 *)tmp2)=0;
		*((UInt32 *)(tmp2+4))=0;
	}
	if(*((UInt32 *)(work_buff+0x3060))) {
		tmp3=Sub_WavUnp5(*((UInt32 *)(work_buff+0x3060)));
		*((UInt32 *)tmp3)=*((UInt32 *)(work_buff+0x3060));
		tmp3=*((UInt32 *)(work_buff+0x3060));
		*((UInt32 *)(tmp3+4))=*((UInt32 *)(work_buff+0x3064));
		*((UInt32 *)(work_buff+0x3060))=0;
		*((UInt32 *)(work_buff+0x3064))=0;
	}
	tmp2=work_buff+0x305C;
	for(i=0x203;i!=0;i--) {
		tmp3=*((UInt32 *)(tmp2-0x18));
		tmp2-=0x18;
		if(tmp3) {
			tmp3=Sub_WavUnp5(tmp2);
			*((UInt32 *)tmp3)=*((UInt32 *)tmp2);
			tmp3=*((UInt32 *)tmp2);
			*((UInt32 *)(tmp3+4))=*((UInt32 *)(tmp2+4));
			*((UInt32 *)tmp2)=0;
			*((UInt32 *)(tmp2+4))=0;
		}
		tmp1=*((UInt32 *)tmp2);
		if(tmp1) {
			tmp3=*((UInt32 *)(tmp2+4));
			if((SInt32)tmp3<0)
				tmp3=~tmp3;
			else
				tmp3=tmp2+tmp3-*((UInt32 *)(tmp1+4));
			*((UInt32 *)tmp3)=tmp1;
			tmp3=*((UInt32 *)tmp2);
			*((UInt32 *)(tmp3+4))=*((UInt32 *)(tmp2+4));
			*((UInt32 *)tmp2)=0;
			*((UInt32 *)(tmp2+4))=0;
		}
	}
	free((UInt8 *)work_buff);
	return size_out;
}


// ==========================================================================
void Sub_WavUnp12(UInt32 base)
{
	UInt32	tmp=0;
	SInt32	i=0;


	Sub_WavUnp6(base);
	tmp=base+0x3474;
	for(i=0x80;i!=0;i--) {
		*((UInt32 *) tmp)=0;
		tmp+=0xC;
	}
	return;
}


// ==========================================================================
UInt32 Sub_WavUnp11(UInt32 buf_out,UInt32 size_out,UInt32 work_buff,UInt32 base)
{
	UInt32	var4=0,var8=0,varc=0,var10=0,var14=0;
	UInt32	flag=0,tmp0=0,tmp1=0,tmp2=0,tmp3=0,tmp4=0,tmp5=0;


	if(size_out==0)
		return 0;
	tmp0=work_buff;
	if(*((UInt32 *)(tmp0+8))<=8) {
		tmp3=*((UInt32 *)tmp0);
		*((UInt32 *)(tmp0+4))|=((UInt32)*((UInt16 *)tmp3)&0xFFFF)<<(*((UInt32 *)(tmp0+8))&0xFF);		
		*((UInt32 *)(tmp0+8))+=0x10;
		*((UInt32 *)tmp0)+=2;				                    
	}
	*((UInt32 *)(tmp0+8))+=0xFFFFFFF8;
	varc=*((UInt32 *)(tmp0+4))&0xFF;
	*((UInt32 *)(tmp0+4))=*((UInt32 *)(tmp0+4))>>8;
	Sub_WavUnp1(varc,base);
	if((varc&0xFF)==0)
		*((UInt32 *)base)=1;
	else
		*((UInt32 *)base)=0;
	varc=buf_out;

	while(1) {
		if(*((UInt32 *)(tmp0+8))<=7) {
			tmp3=*((UInt32 *)tmp0);
			*((UInt32 *)(tmp0+4))|=((UInt32)*((UInt16 *)tmp3)&0xFFFF)<<(*((UInt32 *)(tmp0+8))&0xFF);		
			*((UInt32 *)(tmp0+8))+=0x10;
			*((UInt32 *)tmp0)+=2;
		}
		tmp3=*((UInt32 *)(tmp0+4));
		tmp4=tmp3&0x7F;					
		tmp2=base+(tmp4+0x45F)*12;
		tmp1=*((UInt32 *)tmp2);
		var8=0;
		flag=1;
		if(tmp1==*((UInt32 *)(base+4))) {
			var8=1;
			tmp5=*((UInt32 *)(tmp2+4));
			if(tmp5>7) {
				*((UInt32 *)(tmp0+4))=*((UInt32 *)(tmp0+4))>>7;
				*((UInt32 *)(tmp0+8))+=0xFFFFFFF9;
				var14=*((UInt32 *)(tmp2+8));
			} else {
				*((UInt32 *)(tmp0+4))=*((UInt32 *)(tmp0+4))>>(*((UInt32 *)(tmp2+4))&0xFF);
				*((UInt32 *)(tmp0+8))-=*((UInt32 *)(tmp2+4));
				tmp5=*((UInt32 *)(tmp2+8));
				flag=0;
			}
		} else {
			tmp5=*((UInt32 *)(base+0x3060));
			tmp3=*((UInt32 *)tmp5);
			var14=*((UInt32 *)(tmp3+4));
			tmp3=var14;
			if((SInt32)var14<=0)
				var14=0;
		}
		if(flag) {
			var10=0;
			do {
				var14=*((UInt32 *)(var14+0x14));
				if(Sub_WavUnp8(tmp0)) {
					tmp3=*((UInt32 *)(var14+4));
					var14=tmp3;
				} else
					tmp3=var14;
				var10++;
				if(var10==7)
					var4=tmp3;
			} while(*((UInt32 *)(tmp3+0x14)));
			if(var8==0) {
				if(var10>7)	{
					*((UInt32 *)tmp2)=*((UInt32 *)(base+4));
					*((UInt32 *)(tmp2+4))=var10;
					*((UInt32 *)(tmp2+8))=var4;
				} else {
					tmp3=(((var10|0xFFFFFFFF)>>((0x20-var10)&0xFF)))&tmp4;
					tmp1=1<<(var10&0xFF);
					tmp5=base+0x347C+tmp3*12;
					tmp2=(tmp1*3)<<2;
					do {
						tmp3+=tmp1;
						*((UInt32 *)(tmp5-8))=*((UInt32 *)(base+4));
						*((UInt32 *)(tmp5-4))=var10;
						*((UInt32 *)tmp5)=*((UInt32 *)(var14+8));
						tmp5+=tmp2;
					} while(tmp3<=0x7F);
				}
			}
			var10=*((UInt32 *)(var14+8));	
			tmp5=var10;
		}
		if(tmp5==0x101) {
			tmp5=*((UInt32 *)(tmp0+8));
			if(tmp5<=8)	{
				tmp3=*((UInt32 *)tmp0);
				*((UInt32 *)(tmp0+4))|=((UInt32)*((UInt16 *)tmp3)&0xFFFF)<<(*((UInt32 *)(tmp0+8))&0xFF);
				*((UInt32 *)(tmp0+8))+=0x10;
				*((UInt32 *)tmp0)+=2;
			}
			tmp2=*((UInt32 *)(tmp0+4))&0xFF;
			*((UInt32 *)(tmp0+4))=*((UInt32 *)(tmp0+4))>>8;
			*((UInt32 *)(tmp0+8))+=0xFFFFFFF8;			
			var10=tmp2;
			Sub_WavUnp9(tmp2,base);
			if(*((UInt32 *)base)==0) {
				tmp2=*((UInt32 *)(base+tmp2*4+0x306C));
				if(tmp2) {									
					while(tmp2) {
						tmp3=*((UInt32 *)(tmp2+0xC));
						tmp3++;
						*((UInt32 *)(tmp2+0xC))=tmp3;	
						tmp0=tmp2;
						tmp5=tmp3;
						while(1) {
							tmp4=*((UInt32 *)(tmp0+4));
							if((SInt32)tmp4<0)
								tmp4=0;
							if(tmp4) {
								if(*((UInt32 *)(tmp4+0xC))<tmp5)
									tmp0=tmp4;
								else
									break;
							} else
								break;
						}								
						if(tmp0!=tmp2) {
							Sub_WavUnp4(tmp0,2,tmp2,base+0x305C);
							Sub_WavUnp4(tmp2,2,tmp4,base+0x305C);
							tmp3=*((UInt32 *)(tmp0+0x10));
							tmp5=*((UInt32 *)(tmp3+0x14));
							tmp3=*((UInt32 *)(tmp2+0x10));
							if(*((UInt32 *)(tmp3+0x14))==tmp2)
								*((UInt32 *)(tmp3+0x14))=tmp0;
							if(tmp5==tmp0) {
								tmp5=*((UInt32 *)(tmp0+0x10));
								*((UInt32 *)(tmp5+0x14))=tmp2;
							}
							tmp3=*((UInt32 *)(tmp2+0x10));
							*((UInt32 *)(tmp2+0x10))=*((UInt32 *)(tmp0+0x10));
							*((UInt32 *)(tmp0+0x10))=tmp3;
							*((UInt32 *)(base+4))+=1;
						}
						tmp2=*((UInt32 *)(tmp2+0x10));
					}
					tmp5=var10;
					tmp0=work_buff;
				}
				else
					tmp5=var10;
			}
			else
				tmp5=var10;
		}
		tmp3=varc;
		if(tmp5!=0x100)						
		{
			size_out--;
			*((UInt8 *)varc)=(UInt8)tmp5&0xFF;
			tmp3++;
			varc=tmp3;
			if(size_out) {
				if(*((UInt32 *)base)) {
					tmp3=*((UInt32 *)(base+tmp5*4+0x306C));
					Sub_WavUnp10(tmp3,base);
				}
			} else
				return tmp3-buf_out;
		}
		else
			return tmp3-buf_out;
	}
}
	

// ==========================================================================
void Sub_WavUnp1(UInt32 arg1,UInt32 base)
{
	UInt32	var_4=0,var_8=0,var_c=0;
	UInt32	tmp0=0,tmp1=0,tmp2=0,tmp3=0,tmp4=0;
	SInt32	i=0;


	while((SInt32)*((UInt32 *)(base+0x3064))>0) {       
		tmp0=Sub_WavUnp2(0,*((UInt32 *)(base+0x3050)),*((UInt32 *)(base+0x3064)));
		if(*((UInt32 *) tmp0))
			Sub_WavUnp3(tmp0);
		tmp2=base+0x3054;
		*((UInt32 *)(tmp0))=tmp2;
		*((UInt32 *)(tmp0+4))=*((UInt32 *)(tmp2+4));
		tmp3=*((UInt32 *)(tmp2+4));
		if((SInt32)tmp3<=0)
			tmp3=~tmp3;
		tmp3=Sub_WavUnp2(0,*((UInt32 *)(base+0x3050)),tmp3);
		*((UInt32 *)tmp3)=(UInt32)tmp0;
		*((UInt32 *)(tmp2+4))=base;
	}
	tmp0=base+0x306c;
	for(i=0x102;i!=0;i--) {
		*((UInt32 *)tmp0)=0;
		tmp0+=4;
	}
	var_c=0;
	var_8=base+0x306C;
	tmp1=(UInt32)wav_table+(((arg1&0xff)<<7)+(arg1&0xff))*2;
	var_4=tmp1;
	tmp0=0;
	while(tmp0<0x100) {
		if(*((UInt8 *)(tmp0+tmp1))) {	
			tmp2=*((UInt32 *)(base+0x3058));
			if((SInt32)tmp2<=0) {
				tmp2=*((UInt32 *)(base+0x3068))*24+base+8;
				*((UInt32 *)(base+0x3068))=*((UInt32 *)(base+0x3068))+1;
			}
			Sub_WavUnp4(tmp2,2,0,base+0x305C);
			*((UInt32 *)(tmp2+0x10))=0;
			*((UInt32 *)(tmp2+0x14))=0;
			*((UInt32 *)var_8)=tmp2;
			*((UInt32 *)(tmp2+8))=tmp0;
			tmp3=*((UInt32 *)(tmp0+tmp1))&0xFF;
			*((UInt32 *)(tmp2+0xC))=tmp3;
			if(tmp3>=var_c)
				var_c=tmp3;
			else {
				tmp1=*((UInt32 *)(base+0x3064));
				if((SInt32)tmp1>0) {
					while(tmp1)	{
						if(*((UInt32 *)(tmp1+0xC))<tmp3)
							tmp1=*((UInt32 *)(tmp1+4));
						else
							break;
					}
				} else
					tmp1=0;
				arg1=Sub_WavUnp2(0,*((UInt32 *)(base+0x305C)),tmp2);
				if(*((UInt32 *)arg1))
					Sub_WavUnp3(arg1);
				if(tmp1)
					tmp3=Sub_WavUnp2(0,*((UInt32 *)(base+0x305C)),tmp1);
				else
					tmp3=base+0x3060;
				tmp4=*((UInt32 *)tmp3);
				*((UInt32 *)arg1)=tmp4;
				*((UInt32 *)(arg1+4))=*((UInt32 *)(tmp4+4));
				tmp1=var_4;
				*((UInt32 *)(tmp4+4))=tmp2;
				*((UInt32 *)tmp3)=arg1;
			}
		}
		var_8+=4;
		tmp0++;
	}
	if(tmp0<0x102) {
		tmp1=base+tmp0*4+0x306C;
		while(tmp0<0x102) {
			tmp2=*((UInt32 *)(base+0x3058));
			if((SInt32)tmp2<=0)	{
				tmp2=base+*((UInt32 *)(base+0x3068))*24+8;
				*((UInt32 *)(base+0x3068))=*((UInt32 *)(base+0x3068))+1;
				
			}
			Sub_WavUnp4(tmp2,1,0,base+0x305C);
			tmp1+=4;
			*((UInt32 *)(tmp2+0x10))=0;
			*((UInt32 *)(tmp2+0x14))=0;
			*((UInt32 *)(tmp1-4))=tmp2;
			*((UInt32 *)(tmp2+8))=tmp0;
			tmp0++;
			*((UInt32 *)(tmp2+0xC))=1;
		}
	}
	tmp0=*((UInt32 *)(base+0x3064));
	if((SInt32)tmp0<=0)
		tmp0=0;
	while(tmp0) {
		tmp1=*((UInt32 *)(tmp0+4));
		if((SInt32)tmp1<=0)
			tmp1=0;
		arg1=tmp1;
		if(tmp1) {
			tmp2=*((UInt32 *)(base+0x3058));
			if((SInt32)tmp2<=0)	{
				tmp2=base+*((UInt32 *)(base+0x3068))*24+8;
				*((UInt32 *)(base+0x3068))=*((UInt32 *)(base+0x3068))+1;
			}
			Sub_WavUnp4(tmp2,2,0,base+0x305C);
			*((UInt32 *)(tmp2+0x10))=0;
			*((UInt32 *)(tmp2+0x14))=0;
			*((UInt32 *)(tmp2+0x14))=tmp0;
			*((UInt32 *)(tmp2+0xC))=*((UInt32 *)(tmp1+0xC))+*((UInt32 *)(tmp0+0xC));
			*((UInt32 *)(tmp0+0x10))=tmp2;
			*((UInt32 *)(tmp1+0x10))=tmp2;
			tmp3=*((UInt32 *)(tmp2+0xC));
			if(tmp3>=var_c)
				var_c=tmp3;
			else {
				tmp1=*((UInt32 *)(tmp1+4));
				tmp0=tmp1;
				if((SInt32)tmp0<=0)
					tmp0=0;
				while(tmp0)	{
					if(*((UInt32 *)(tmp0+0xC))<tmp3) {
						tmp0=*((UInt32 *)(tmp0+4));
						if((SInt32)tmp0<=0)
							tmp0=0;
					}
					else
						break;
				}
				tmp1=Sub_WavUnp2(0,*((UInt32 *)(base+0x305C)),tmp2);
				if(*((UInt32 *)(tmp1))) {
					tmp3=Sub_WavUnp5(tmp1);
					*((UInt32 *)(tmp3))=*((UInt32 *)(tmp1));
					tmp4=*((UInt32 *)tmp1);
					*((UInt32 *)(tmp4+4))=*((UInt32 *)(tmp1+4));
					*((UInt32 *)tmp1)=0;
					*((UInt32 *)(tmp1+4))=0;
				}
				if(tmp0)
					tmp3=Sub_WavUnp2(0,*((UInt32 *)(base+0x305C)),tmp0);
				else
					tmp3=base+0x3060;
				tmp4=*((UInt32 *)(tmp3));
				*((UInt32 *)(tmp1))=tmp4;
				*((UInt32 *)(tmp1+4))=*((UInt32 *)(tmp4+4));
				*((UInt32 *)(tmp4+4))=tmp2;
				*((UInt32 *)(tmp3))=tmp1;
				tmp1=arg1;
			}
			tmp0=*((UInt32 *)(tmp1+4));
			if((SInt32)tmp0<=0)
				tmp0=0;
		} else
			break;
	}
	*((UInt32 *)(base+4))=1;
	return;		
}


// ==========================================================================
void Sub_WavUnp9(UInt32 arg1,UInt32 arg2)
{
	UInt32	var4=0,var8=0,tmp0=0,tmp1=0,tmp2=0,tmp3=0,tmp4=0;


	tmp0=*((UInt32 *)(arg2+0x3064));
	if((SInt32)tmp0<0)
		tmp0=0;
	tmp2=*((UInt32 *)(arg2+0x3058));
	if((SInt32)tmp2<0)
		tmp2=0;
	var4=arg2;
	if(tmp2==0) {
		tmp2=var4+*((UInt32 *)(var4+0x3068))*24+8;
		*((UInt32 *)(var4+0x3068))+=1;
	}
	var8=arg2+0x305C;
	tmp1=Sub_WavUnp2(0,*((UInt32 *)var8),tmp2);
	if(*((UInt32 *)tmp1)) {
		tmp3=Sub_WavUnp5(tmp1);
		*((UInt32 *)tmp3)=*((UInt32 *)tmp1);
		tmp3=*((UInt32 *)tmp1);
		*((UInt32 *)(tmp3+4))=*((UInt32 *)(tmp1+4));
		*((UInt32 *)tmp1)=0;
		*((UInt32 *)(tmp1+4))=0;
	}
	*((UInt32 *)tmp1)=var8+4;
	*((UInt32 *)(tmp1+4))=*((UInt32 *)(var8+8));
	tmp3=Sub_WavUnp2(0,*((UInt32 *)var8),Sub_WavUnp7(var8+4));
	*((UInt32 *)tmp3)=tmp1;
	*((UInt32 *)(var8+8))=tmp2;
	*((UInt32 *)(tmp2+0x10))=0;
	*((UInt32 *)(tmp2+0x14))=0;
	*((UInt32 *)(tmp2+8))=*((UInt32 *)(tmp0+8));
	*((UInt32 *)(tmp2+0xC))=*((UInt32 *)(tmp0+0xC));
	*((UInt32 *)(tmp2+0x10))=tmp0;
	*((UInt32 *)(var4+*((UInt32 *)(tmp0+8))*4+0x306C))=tmp2;
	tmp2=*((UInt32 *)(var4+0x3058));
	if((SInt32)tmp2<0)
		tmp2=0;
	if(tmp2==0)	{
		tmp2=var4+*((UInt32 *)(var4+0x3068))*24+8;
		*((UInt32 *)(var4+0x3068))+=1;
	}
	tmp1=Sub_WavUnp2(0,*((UInt32 *)var8),tmp2);
	if(*((UInt32 *)tmp1)) {
		tmp3=Sub_WavUnp5(tmp1);
		*((UInt32 *)tmp3)=*((UInt32 *)tmp1);
		tmp3=*((UInt32 *)tmp1);
		*((UInt32 *)(tmp3+4))=*((UInt32 *)(tmp1+4));
		*((UInt32 *)tmp1)=0;
		*((UInt32 *)(tmp1+4))=0;
	}
	*((UInt32 *)tmp1)=var8+4;
	*((UInt32 *)(tmp1+4))=*((UInt32 *)(var8+8));
	tmp3=Sub_WavUnp2(0,*((UInt32 *)var8),Sub_WavUnp7(var8+4));
	*((UInt32 *)tmp3)=tmp1;
	tmp3=arg1;
	*((UInt32 *)(var8+8))=tmp2;
	*((UInt32 *)(tmp2+0x14))=0;
	*((UInt32 *)(tmp2+8))=tmp3;
	*((UInt32 *)(tmp2+0xC))=0;
	*((UInt32 *)(tmp2+0x10))=tmp0;
	*((UInt32 *)(var4+tmp3*4+0x306C))=tmp2;
	*((UInt32 *)(tmp0+0x14))=tmp2;
	while(tmp2) {
		tmp4=tmp2;
		*((UInt32 *)(tmp2+0xC))+=1;
		tmp3=*((UInt32 *)(tmp2+0xC));
		while(1) {
			tmp1=*((UInt32 *)(tmp4+4));
			if((SInt32)tmp1<=0)	{
				tmp1=0;
				break;
			}
			if(*((UInt32 *)(tmp1+0xC))>=tmp3)
				break;
			tmp4=tmp1;
		}
		if(tmp4!=tmp2) {
			Sub_WavUnp4(tmp4,2,tmp2,var8);
			Sub_WavUnp4(tmp2,2,tmp1,var8);
			tmp3=*((UInt32 *)(tmp4+0x10));
			tmp1=*((UInt32 *)(tmp2+0x10));
			tmp0=*((UInt32 *)(tmp3+0x14));
			tmp3=*((UInt32 *)(tmp1+0x14));
			if(tmp3==tmp2)
				*((UInt32 *)(tmp1+0x14))=tmp4;
			if(tmp0==tmp4) {
				tmp1=*((UInt32 *)(tmp4+0x10));
				*((UInt32 *)(tmp1+0x14))=tmp2;
			}
			tmp3=*((UInt32 *)(tmp2+0x10));
			*((UInt32 *)(tmp2+0x10))=*((UInt32 *)(tmp4+0x10));
			*((UInt32 *)(tmp4+0x10))=tmp3;
			*((UInt32 *)(var4+4))+=1;
		}
		tmp2=*((UInt32 *)(tmp2+0x10));
	}
	return;
}


// ==========================================================================
void Sub_WavUnp6(UInt32 ptr_base)
{
	UInt32	ptr1=0;
	SInt32	i=0;


	ptr1=ptr_base+8;				
	for(i=0x203;i!=0;i--) {		
		*((UInt32 *)ptr1)=0;
		*((UInt32 *)(ptr1+4))=0;
		ptr1+=0x18;				
	}
	ptr1=ptr_base+0x3054;
	*((UInt32 *)ptr1)=0;
	*((UInt32 *)(ptr1+4))=0;
	*((UInt32 *)ptr1)=ptr1;
	*((UInt32 *)(ptr_base+0x3050))=0;
	*((UInt32 *)(ptr_base+0x3058))=~ptr1;
	ptr1=ptr_base+0x3060;
	*((UInt32 *)ptr1)=0;
	*((UInt32 *)(ptr1+4))=0;
	*((UInt32 *)ptr1)=(UInt32) ptr1;
	*((UInt32 *)(ptr_base+0x305C))=0;
	*((UInt32 *)(ptr_base+0x3064))=~ptr1;
	*((UInt32 *)(ptr_base+0x3068))=0;
	*((UInt32 *)(ptr_base+4))=1;
	return;
}


// ==========================================================================
UInt32 Sub_WavUnp2(UInt32 arg1,UInt32 arg2,UInt32 arg3)
{
	if(arg1)
		return arg3+arg2;
	return arg3;
}
	   

// ==========================================================================
void Sub_WavUnp3(UInt32 ptr)  
{
	UInt32 ptr0=0,ptr1=0;


	ptr0=*((UInt32 *)ptr);
	if(ptr0) {
		ptr1=*((UInt32 *)(ptr+4));
		if((SInt32)ptr1<0)
		 	ptr1=~ptr1;
		else
			ptr1=ptr1+ptr-*((UInt32 *)(ptr0+4));
		*((UInt32 *)ptr1)=ptr0;
		*((UInt32 *)(ptr0+4))=*((UInt32 *)(ptr+4));
		*((UInt32 *)ptr)=0;
		*((UInt32 *)(ptr+4))=0;
	}
	return;
}
		

// ==========================================================================
void Sub_WavUnp4(UInt32 arg1,UInt32 arg2,UInt32 arg3,UInt32 arg4)
{
	UInt32	tmp0=0,tmp1=0;


	tmp0=*((UInt32 *)arg1);
	if(tmp0) {
		tmp1=*((UInt32 *)(arg1+4));
		if((SInt32)tmp1<0)
			tmp1=~tmp1;
		else
			tmp1+=arg1-*((UInt32 *)(tmp0+4));
		*((UInt32 *)tmp1)=(UInt32)tmp0;
		*((UInt32 *)(tmp0+4))=*((UInt32 *)(arg1+4));
		*((UInt32 *)arg1)=0;
		*((UInt32 *)(arg1+4))=0;
	}
	if(arg3==0)
		arg3=arg4+4;
	if(arg2-1) {
		if((arg2-2)==0)	{
			tmp1=*((UInt32 *)arg3);
			*((UInt32 *)arg1)=tmp1;
			*((UInt32 *)(arg1+4))=*((UInt32 *)(tmp1+4));
			*((UInt32 *)(tmp1+4))=arg1;
			*((UInt32 *)arg3)=arg1;
		}
		return;
	}
	*((UInt32 *)arg1)=arg3;
	*((UInt32 *)(arg1+4))=*((UInt32 *)(arg3+4));
	tmp1=*((UInt32 *)(arg3+4));
	if((SInt32)tmp1<=0)
		tmp1=~tmp1;
	*((UInt32 *)tmp1)=arg1;
	*((UInt32 *)(arg3+4))=arg1;
	return;
}


// ==========================================================================
UInt32 Sub_WavUnp5(UInt32 arg1)
{
	UInt32 tmp0=0,tmp1=0;


	tmp0=*((UInt32 *)(arg1+4));
	if((SInt32)tmp0<0)
		return ~tmp0;
	tmp1=*((UInt32 *)arg1);
	tmp0+=arg1-*((UInt32 *)(tmp1+4));
	return tmp0;
}


// ==========================================================================
UInt32 Sub_WavUnp7(UInt32 arg1)
{
	UInt32 tmp=0;


	tmp=*((UInt32 *)(arg1+4));
	if((SInt32)tmp<=0)
		return ~tmp;
	return tmp;
}


// ==========================================================================
UInt32 Sub_WavUnp8(UInt32 arg1)
{
	UInt32	tmp0=0,tmp1=0;


	tmp0=*((UInt32 *)(arg1+4));
	*((UInt32 *)(arg1+4))=tmp0>>1;
	tmp0&=1;
	*((UInt32 *)(arg1+8))-=1;
	if(*((UInt32 *)(arg1+8))==0) {
		tmp1=*((UInt32 *)arg1);
		*((UInt32 *)arg1)+=4;
		*((UInt32 *)(arg1+4))=*((UInt32 *)tmp1);
		*((UInt32 *)(arg1+8))=0x20;
	}
	return tmp0;
}


// ==========================================================================
void Sub_WavUnp10(UInt32 arg1,UInt32 arg2)
{
	UInt32	tmp0=0,tmp1=0,tmp2=0,tmp3=0,arg2_old=0;


	arg2_old=arg2;
	while(arg1) {
		tmp2=arg1;
		*((UInt32 *)(arg1+0xC))+=1;
		tmp3=*((UInt32 *)(arg1+0xC));
		while(1) {
			tmp0=*((UInt32 *)(tmp2+4));
			if((SInt32)tmp0<=0) {
				tmp0=0;
				break;
			}
			if(*((UInt32 *)(tmp0+0xC))>=tmp3)
				break;
			tmp2=tmp0;
		}
		if(tmp2!=arg1) {
			if(*((UInt32 *)tmp2)) {
				tmp3=Sub_WavUnp5(tmp2);
				*((UInt32 *)tmp3)=*((UInt32 *)tmp2);
				tmp1=*((UInt32 *)tmp2);
				*((UInt32 *)(tmp1+4))=*((UInt32 *)(tmp2+4));
				*((UInt32 *)tmp2)=0;
				*((UInt32 *)(tmp2+4))=0;
			}
			tmp1=arg1;
			if(tmp1==0)
				tmp1=arg2+0x3060;
			tmp3=*((UInt32 *)tmp1);
			*((UInt32 *)tmp2)=tmp3;
			*((UInt32 *)(tmp2+4))=*((UInt32 *)(tmp3+4));
			*((UInt32 *)tmp1)=tmp2;
			arg2=Sub_WavUnp2(0,*((UInt32 *)(arg2+0x205C)),arg1);
			if(*((UInt32 *)arg2)) {
				tmp3=Sub_WavUnp5(arg2);
				*((UInt32 *)tmp3)=*((UInt32 *)arg2);
				tmp1=*((UInt32 *)arg2);
				*((UInt32 *)(tmp1+4))=*((UInt32 *)(arg2+4));
				*((UInt32 *)arg2)=0;
				*((UInt32 *)(arg2+4))=0;
			}
			if(tmp0)
				tmp3=Sub_WavUnp2(0,*((UInt32 *)(arg2_old+0x305C)),tmp0);
			else
				tmp3=arg2_old+0x3060;
			tmp1=*((UInt32 *)tmp3);
			*((UInt32 *)arg2)=tmp1;
			tmp0=*((UInt32 *)(tmp1+4));
			*((UInt32 *)(arg2+4))=tmp0;
			*((UInt32 *)(tmp1+4))=arg1;
			*((UInt32 *)tmp3)=arg2;
			tmp3=*((UInt32 *)(tmp2+0x10));
			tmp1=*((UInt32 *)(tmp3+0x14));
			tmp3=*((UInt32 *)(arg1+0x10));
			if(*((UInt32 *)(tmp3+0x14))==arg1)
				*((UInt32 *)(tmp3+0x14))=tmp2;
			if(tmp1==tmp2) {
				tmp1=*((UInt32 *)(tmp2+0x10));
				*((UInt32 *)(tmp1+0x14))=arg1;
			}
			tmp3=*((UInt32 *)(arg1+0x10));
			*((UInt32 *)(arg1+0x10))=*((UInt32 *)(tmp2+0x10));
			*((UInt32 *)(tmp2+0x10))=tmp3;
			arg2=arg2_old;
			*((UInt32 *)(arg2_old+4))+=1;
		}
		arg1=*((UInt32 *)(arg1+0x10));
	}
	return;
}

#ifdef WIN32
   #pragma warning (pop)
#endif
