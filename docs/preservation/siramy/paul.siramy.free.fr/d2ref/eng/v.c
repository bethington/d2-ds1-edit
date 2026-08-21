
/*
   Goal   : Intend to output D2S file (Diablo II savegames) in a more
            "comprehensive" and usable form. It allows easier hacking
   Use    : Usually, I redirect the output in a file,
            then I use GREP command on 1 or more of this files
   Statut : Work in progress
   
   Date   : 07-18-2001
   
   Author : Paul SIRAMY (siramy_paul@yahoo.com , paul.siramy@free.fr)
   Note   : You can compil it with :
               DJGPP
               Turbo C++ 3.0
            Shoul be compiled with any other compiler
   
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// global variables
void * buf;              // the copy of the d2s file into memory
int  buf_size = 0;       // size of the d2s file
int  savegame_version,   // version of the savegame
     savegame_expansion; // set to TRUE if it's only playable in expansion
#ifndef TRUE
   #define FALSE 0
   #define TRUE  -1
#endif
// enums
typedef enum {none, string, array, bitfield, value, quest_struct} INFO_TYPE;
typedef enum {VER_ERR, VER_100, VER_108, VER_109} SAVEGAME_VERSION;
// ID of items with 15 bytes size (Diablo 2 version >= 1.04)
// ID are bytes 10 to 13 of item's data (bytes ranging from 0 to 14)
struct
{
   char id[12];
   char name[30];
} item_15[] =
  {
     // potions
     {"A0 C1 C5 80", "minor healing potion"},
     {"A0 C1 C9 80", "light healing potion"},
     {"A0 C1 CD 80", "healing potion"},
     {"A0 C1 D1 80", "greater healing potion"},
     {"A0 C1 D5 80", "super healing potion"},
     {"B4 C1 C5 80", "minor mana potion"},
     {"B4 C1 C9 80", "light mana potion"},
     {"B4 C1 CD 80", "mana potion"},
     {"B4 C1 D1 80", "greater mana potion"},
     {"B4 C1 D5 80", "super mana potion"},
        
     {"C8 D9 CD 81", "rejuvenation potion"},
     {"C8 D9 B1 81", "full rejuvenation potion"},
        
     {"D8 C1 CD 81", "stamina potion"},
     {"E4 C1 CD 81", "antidote potion"},
     {"DC B5 CD 81", "thawing potion"},
     // scroll
     {"D0 CD 80 81", "scroll of town portal"},
     {"A4 CD 8D 81", "scroll of identify"},
     // miscelaneous
     {"B4 CD CD 81", "Mephisto's soulstone"},
     {"C4 95 E5 81", "Khalim's eye"},
     // gems
     {"9C 8D D9 81", "chipped amethyst"},
     {"9C 99 D9 81", "flawed amethyst"},
     {"9C CD D9 81", "amethyst"},
     {"9C E9 D9 81", "flawless amethyst"},
     {"9C 81 D9 81", "perfect amethyst"},
     
     {"9C 8D E5 81", "chipped topaz"},
     {"9C 99 E5 81", "flawed topaz"},
     {"9C CD E5 81", "topaz"},
     {"9C B1 E5 81", "flawless topaz"},
     {"9C C1 E5 81", "perfect topaz"},
     
     {"9C 8D 89 81", "chipped saphirre"},
     {"9C 99 89 81", "flawed saphirre"},
     {"9C CD 89 81", "saphirre"},
     {"9C B1 89 81", "flawless saphirre"},
     {"9C C1 89 81", "perfect saphirre"},
     {"9C 8D 9D 81", "chipped emerald"},
     {"9C 99 9D 81", "flawed emerald"},
     {"9C CD 9D 81", "emerald"},
     {"9C B1 9D 81", "flawless emerald"},
     {"9C C1 9D 81", "perfect emerald"},
     {"9C 8D C9 81", "chipped ruby"},
     {"9C 99 C9 81", "flawed ruby"},
     {"9C CD C9 81", "ruby"},
     {"9C B1 C9 81", "flawless ruby"},
     {"9C C1 C9 81", "perfect ruby"},
     {"9C 8D DD 81", "chipped diamond"},
     {"9C 99 DD 81", "flawed diamond"},
     {"9C CD DD 81", "diamond"},
     {"9C B1 DD 81", "flawless diamond"},
     {"9C C1 DD 81", "perfect diamond"},
     
     {"CC AD 8D 81", "chipped skull"},
     {"CC AD 99 81", "flawed skull"},
     {"CC AD D5 81", "skull"},
     {"CC AD B1 81", "flawless skull"},
     {"CC AD E9 81", "perfect skull"},
     
     {"", ""}   // DO NOT REMOVE !
     };
// item of 27 bytes (v <= 1.03)
struct ITEM_27
{
   unsigned char  jm[2];
   unsigned short bitfield1;
   unsigned short bitfield2;
   unsigned char  bitfield3;
   unsigned char  modlevel;
   unsigned short itemcode;
   unsigned short quantity;
   unsigned short quantityex;
   unsigned short durability;
   unsigned char  coordinates;
   unsigned char  specialitemcode;
   unsigned long  properties1;
   unsigned long  properties2;
   unsigned short container;
} item_27;
// Datas of the Diablo II file
struct
{
   unsigned short offset;
   char           name[20];
   int            size;
   unsigned char  num_el;   // number of element
   unsigned char  size_el;  // size of 1 element
   INFO_TYPE      type;
} info[] =
  {
     //                          total
     //offs  name                 size  #  siz type
     {0x000, "savegame header",     8,  8,  1, array},
     {0x008, "player name",        16,  1, 16, string},
     {0x018, "hardcore_flags",      1,  1,  1, bitfield},
     {0x019, "acts passed",         1,  1,  1, value},
     {0x01A, "player config I/II",  2,  2,  1, array},
     {0x01C, "unknown value",       2,  2,  1, array},
     {0x01E, "fixed_01",            4,  4,  1, array},
     {0x022, "player class",        2,  1,  2, value},
     {0x024, "player level copy",   2,  1,  2, value},
     {0x026, "look head",           1,  1,  1, array},
     {0x027, "look torso",          1,  1,  1, array},
     {0x028, "look legs",           1,  1,  1, array},
     {0x029, "look R arm",          1,  1,  1, array},
     {0x02A, "look L arm",          1,  1,  1, array},
     {0x02B, "look R hand",         1,  1,  1, array},
     {0x02C, "look L hand",         1,  1,  1, array},
     {0x02D, "look shield",         1,  1,  1, array},
     {0x02E, "look R shoulder",     1,  1,  1, array},
     {0x02F, "look L shoulder",     1,  1,  1, array},
     {0x030, "look reserved",       6,  6,  1, array},
     {0x036, "look col head",       1,  1,  1, array},
     {0x037, "look col torso",      1,  1,  1, array},
     {0x038, "look col legs",       1,  1,  1, array},
     {0x039, "look col R arm",      1,  1,  1, array},
     {0x03A, "look col L arm",      1,  1,  1, array},
     {0x03B, "look col R hand",     1,  1,  1, array},
     {0x03C, "look col L hand",     1,  1,  1, array},
     {0x03D, "look col shield",     1,  1,  1, array},
     {0x03E, "look col R shoulder", 1,  1,  1, array},
     {0x03F, "look col L shoulder", 1,  1,  1, array},
     {0x040, "look col reserved",   6,  6,  1, array},
     
     {0x046, "shortcut F1 to F8",  16,  8,  2, array},
     {0x056, "skill left hand",     1,  1,  1, array},
     {0x057, "skill right hand",    1,  1,  1, array},
     {0x058, "save_location",       2,  1,  2, array},
     
     {0x05A, "reserved_01",        36, 36,  1, array},
     {0x07E, "seed (maps layout)",  4,  4,  1, array},
     {0x082, "fixed_02",            8,  8,  1, array},
     
     {0x08A, "normal quests",      96,  1, 96, quest_struct},
     {0x0EA, "nightmare quests",   96,  1, 96, quest_struct},
     {0x14A, "hell quests",        96,  1, 96, quest_struct},
     
     {0x1AA, "fixed_03",           10, 10,  1, array},
     {0x1B4, "waypoint header",     2,  2,  1, array},
     {0x1B6, "nrml waypoint",       5,  1,  5, bitfield},
     {0x1BB, "reserved waypoint",  17, 17,  1, array},
     
     {0x1CC, "waypoint header",     2,  2,  1, array},
     {0x1CE, "ngtm waypoint",       5,  1,  5, bitfield},
     {0x1D3, "reserved waypoint",  17, 17,  1, array},
     
     {0x1E4, "waypoint header",     2,  2,  1, array},
     {0x1E6, "hell waypoint",       5,  1,  5, bitfield},
     {0x1EB, "reserved waypoint",  17, 17,  1, array},
     
     {0x1FC, "npc statut header",   4,  4,  1, array},
     
     {0x200, "npc statut1 nrml a1", 1,  1,  1, bitfield},
     {0x201, "npc statut1 nrml a2", 1,  1,  1, bitfield},
     {0x202, "npc statut1 nrml a3", 1,  1,  1, bitfield},
     {0x203, "npc statut1 nrml a4", 1,  1,  1, bitfield},
     {0x204, "npc statut1 nrml a5", 1,  1,  1, bitfield},
     {0x205, "npc statut1 resrved", 3,  3,  1, bitfield},
     {0x208, "npc statut1 ngtm a1", 1,  1,  1, bitfield},
     {0x209, "npc statut1 ngtm a2", 1,  1,  1, bitfield},
     {0x20A, "npc statut1 ngtm a3", 1,  1,  1, bitfield},
     {0x20B, "npc statut1 ngtm a4", 1,  1,  1, bitfield},
     {0x20C, "npc statut1 ngtm a5", 1,  1,  1, bitfield},
     {0x20D, "npc statut1 resrved", 3,  3,  1, bitfield},
     {0x210, "npc statut1 hell a1", 1,  1,  1, bitfield},
     {0x211, "npc statut1 hell a2", 1,  1,  1, bitfield},
     {0x212, "npc statut1 hell a3", 1,  1,  1, bitfield},
     {0x213, "npc statut1 hell a4", 1,  1,  1, bitfield},
     {0x214, "npc statut1 hell a5", 1,  1,  1, bitfield},
     {0x215, "npc statut1 resrved", 3,  3,  1, bitfield},
     {0x218, "npc statut2 nrml a1", 1,  1,  1, bitfield},
     {0x219, "npc statut2 nrml a2", 1,  1,  1, bitfield},
     {0x21A, "npc statut2 nrml a3", 1,  1,  1, bitfield},
     {0x21B, "npc statut2 nrml a4", 1,  1,  1, bitfield},
     {0x21C, "npc statut2 nrml a5", 1,  1,  1, bitfield},
     {0x21D, "npc statut2 resrved", 3,  3,  1, bitfield},
     {0x220, "npc statut2 ngtm a1", 1,  1,  1, bitfield},
     {0x221, "npc statut2 ngtm a2", 1,  1,  1, bitfield},
     {0x222, "npc statut2 ngtm a3", 1,  1,  1, bitfield},
     {0x223, "npc statut2 ngtm a4", 1,  1,  1, bitfield},
     {0x224, "npc statut2 ngtm a5", 1,  1,  1, bitfield},
     {0x225, "npc statut2 resrved", 3,  3,  1, bitfield},
     {0x228, "npc statut2 hell a1", 1,  1,  1, bitfield},
     {0x229, "npc statut2 hell a2", 1,  1,  1, bitfield},
     {0x22A, "npc statut2 hell a3", 1,  1,  1, bitfield},
     {0x22B, "npc statut2 hell a4", 1,  1,  1, bitfield},
     {0x22C, "npc statut2 hell a5", 1,  1,  1, bitfield},
     {0x22D, "npc statut2 resrved", 3,  3,  1, bitfield},
     {0x230, "status header",       2,  2,  1, array},
     {0x232, "stat/skill    flag",  1,  1,  1, bitfield},
     {0x233, "xp/gold/stash flag",  1,  1,  1, bitfield},
     {0x234, "reserved      flag",  1,  1,  1, bitfield},
     {0xFFF, "",                    0,  0,  0, none},
     /* Now, there are volatile datas with non fixed offset
        Ex : strength, dexterity, experience, gold ...
        After, we found the skill header. The offsets will be updated
        at run-time.
     */
     {0x000, "skill header",        2,  2,  1, array},
     {0x000, "skill values",       30, 30,  1, array},
     {0x000, "inventory header",    2,  2,  1, array},
     {0x000, "number of items",     2,  2,  1, value},
     {0xFFF, "",                    0,  0,  0, none},
     
     /* Now, Items. Since version 1.04, items are no longer 27 bytes.
        They could be of 15 or 31 bytes, and since version 1.08 they
        could be of any size
        After, corpse and mercenary structure (up to version 1.06).
        In a savegame of version 1.08, we won't use this folowing lines
     */
     
     {0x000, "inventory footer",    2,  2,  1, array},
     {0x000, "corpse presence",     2,  2,  1, array},
     {0x000, "mercenary header",    2,  2,  1, array},
     {0x000, "mercenary data",      6,  6,  1, array},
     {0xFFF, "",                    0,  0,  0, none}    // DO NOT REMOVE !
   };
// output a string
void print_string(const unsigned char * ptr, int size)
{
   while(size)
   {
      if(* ptr == 0)
         return;
      printf("%c", * ptr >= ' ' ? * ptr : '#');
      ptr++;
      size--;
   }
}
// output an array of bytes
void print_array(const unsigned char * ptr, int size)
{
   printf("hexa [%i] : ", size);
   while (size)
   {
      printf("%02X ", * ptr);
      ptr++;
      size--;
   };
}
// output bitfields
void print_bitfield(unsigned char * ptr_start, int size)
{
   unsigned long b;
   unsigned char val, * ptr = ptr_start;
   int i, nb_char;
   for (nb_char=0; nb_char<size; nb_char++)
   {
      val = (unsigned char) * ptr;
      b = 128;
      for (i=0; i<8; i++)
      {
         printf("%u", val & b ? 1 : 0);
         b >>= 1;
      }
      printf(" ");
      ptr++;
   }
   ptr = ptr_start;
   printf(" hexa = ");
   for (nb_char=0; nb_char<size; nb_char++)
   {
      val = (unsigned char) * ptr;
      printf("%02X ", val);
      ptr++;
   }
}
// output text info on an item, only item with 27 bytes accepted
// IN : pointer on item
void item_info_27(unsigned char b[27])
{
   struct
   {
      char          byte;
      unsigned char size;
      void          * data;
   } transfert [] =
     {
        //byte size  data
        {0x00, 2, &item_27.jm},
        {0x02, 2, &item_27.bitfield1},
        {0x04, 2, &item_27.bitfield2},
        {0x06, 1, &item_27.bitfield3},
        {0x07, 1, &item_27.modlevel},
        {0x08, 2, &item_27.itemcode},
        {0x0A, 2, &item_27.quantity},
        {0x0C, 1, &item_27.quantityex},
        {0x0D, 2, &item_27.durability},
        {0x0F, 1, &item_27.coordinates},
        {0x10, 1, &item_27.specialitemcode},
        {0x11, 4, &item_27.properties1},
        {0x15, 4, &item_27.properties2},
        {0x19, 2, &item_27.container},
        {-1, -1, NULL}   // DO NOT REMOVE
     };
   struct ITEM_27 * itm = & item_27;
   void * ptr_src, * ptr_dst;
   int  i=0, old_byte=0;
   int b1, b2, b3, b4; // temp variables
   unsigned long dw1, dw2;
   // copy the item into the item_27 structure, for better handling of datas
   while (transfert[i].byte != -1)
   {
      if (transfert[i].byte != old_byte)
      {
         printf("item_info_27() error : "
                "new %02X != old %02X\n", transfert[i].byte, old_byte);
         return;
      }
      ptr_dst = transfert[i].data;
      ptr_src = & b[transfert[i].byte];
      memcpy(ptr_dst, ptr_src, transfert[i].size);
      old_byte += transfert[i].size;
      i++;
   }
   // signature (01 02)
   b1 = itm->jm[0];
   b2 = itm->jm[1];
   if ( b1 != 0x4A || b2 != 0x4D)
   {
      printf("? signature != 4A 4D (%02X %02X)\n", b1, b2);
//      return;
   }
   // bitfield 1 (03 04)
   b1 = itm->bitfield1;
   if (b1 & ~ 0x2910)
      printf("? unknown bits in bitfield 1 (%04X)\n", b1);
   // identified
   b2 = b1 & 0x0010;
   printf("identified       = %s\n", b2 ? "yes" : "no");
   // check conflict : durability statut / current durability
   printf("durabili. statut = ");
   b2  = (b1 & 0x0100) ? 1 : 0;              // durability statut
   b3  = (itm->durability & 0x01FE) ? 1 : 0; // current durability
   if (b2)
   {
      printf("UNusable");
      if (b3)
      {
         // current durability > 0
         printf(", NOT normal, current durability is > 0\n");
      }
      else
      {
         // current durability == 0
         printf(", ok because current durability = 0\n");
      }
   }
   else
   {
      printf("usable");
      if (b3)
      {
         // current durability > 0
         printf(", ok because current durability is > 0\n");
      }
      else
      {
         // current durability == 0
         printf(", NOT normal if not potion, etc... (durability = 0)\n");
      }
   }
   // imbued
   b2 = b1 & 0x2000;
   printf("imbued item      = %s\n", b2 ? "yes" : "no");
   // socketed
   b2 = b1 & 0x0800;
   printf("socketed         = %s\n", b2 ? "yes" : "no");
   // bitfield 2 (05 06)
   b1 = itm->bitfield2;
   if (b1 & ~ 0x0003)
      printf("? unknown bits in bitfield 2 (%04X)\n", b1);
   // ear flag
   b2 = b1 & 0x0001;
   printf("ear flag         = %s\n", b2 ? "yes" : "no");
   // startup item
   b2 = b1 & 0x0002;
   printf("startup item     = %s\n", b2 ? "yes" : "no");
   // bitfield 3 (07)
   b1 = itm->bitfield3;
   if (b1 & ~ 0xEF)
      printf("? unknown bits in bitfield 3 (%02X)\n", b1);
   // equiped location (on body)
   b2 = b1 & 0x0F;
   printf("equiped location = %01Xh (", b2);
   switch(b2)
   {
      case 0x0 : printf("not on body");  break;
      case 0X1 : printf("head");         break;
      case 0X2 : printf("amulet");       break;
      case 0X3 : printf("chest");        break;
      case 0X4 : printf("left hand");    break;
      case 0X5 : printf("right hand");   break;
      case 0X6 : printf("left finger");  break;
      case 0X7 : printf("right finger"); break;
      case 0X8 : printf("belt");         break;
      case 0X9 : printf("boots");        break;
      case 0XA : printf("gloves");       break;
      defaut : printf("? body unknown");
   }
   printf(")\n");
   // gems in socketed items
   b2 = (b1 & 0xE0) >> 5;
   printf("# gems sock. itm = %3i\n", b2);
   // item modifier level (08)
   b1 = itm->modlevel;
   printf("modifier level   = %3i\n", b1);
   // item code (09 10)
   b1 = itm->itemcode;
   if (b1 & ~ 0xFFF0)
      printf("? unknown bits in item code (%04X)\n", b1);
   // item code
   b2 = (b1 & 0x3FF0) >> 4;
   printf("item code        = %04X\n", b2);
   // on body
   b2 = b1 & 0x4000;
   printf("on body          = %s\n", b2 ? "yes" : "no");
   // in belt / in item
   b2 = b1 & 0x8000;
   printf("in belt/socketed = %s\n", b2 ? "yes" : "no");
   // quantity and type (11 12)
   b1 = itm->quantity;
   if (b1 & ~ 0xFFEF)
      printf("? unknown bits in quantity and type (%04X)\n", b1);
   // item type
   b2 = b1 & 0x000F;
   if (b2 & 1)
      printf("item type (sock) = %3i (%02X) : ", b2, b2);
   else
      printf("item type        = %3i (%02X) : ", b2, b2);
   b2 >>= 1;
   switch(b2)
   {
      case 0x1 : printf("low-quality (cracked) item\n");  break;
      case 0x2 : printf("white normal item\n"); break;
      case 0x3 : printf("superior item\n");     break;
      case 0x4 : printf("blue magical (beryl) item\n"); break;
      case 0x5 : printf("green set item\n");    break;
      case 0x6 : printf("yellow rare item\n");  break;
      case 0x7 : printf("gray unique item\n");  break;
      default : printf("?\n");
   }
   // quantity
   b2 = (b1 & 0x1FE0) >> 5;
   printf("quantity         = %3i\n", b2);
   // extended quantity field (13)
   b2 = (b1 & 0xE000) >> 13;
   printf("ext quantity 1   = %3i\n", b2);
   // unknown, extended quantity field ?
   b1 = itm->quantityex;
   printf("ext quantity 2   = %3i\n", b1);
   // durability (14 15 and bit 0 of 16)
   b1 = itm->durability;
   if (b1 & ~ 0xFFFE)
      printf("? unknown bits in durability (%04X)\n", b1);
   b2 = itm->coordinates;
   b3 = (b1 & 0x01FE) >> 1;  // current durability
   b4 = (b1 & 0xFE00) >> 9;  // max durability
   b4 |= (b2 & 0x0001) << 7;
   // durability current & max
   printf("durability now   = %3i\n", b3);
   printf("durability max   = %3i\n", b4);
   // location (16 26 27)
   b1 = (itm->container & 0x07F8) >> 3;
   printf("location         = ");
   b3 = FALSE; // use b3 for "in belt ?"
   b4 = FALSE; // use b4 for "on body ?"
   switch (b1)
   {
      case 0x00 : printf("inventory\n");     break;
      case 0x03 : printf("horadric cube\n"); break;
      case 0x04 : printf("stash\n");         break;
      case 0xFF :
         // on body or on belt
         b2 = (itm->itemcode & 0xC000) >> 14;
         switch (b2)
         {
            case 1 : printf("on body\n");
                     b4 = TRUE;
                     break;
            case 2 : printf("in belt\n");
                     b3 = TRUE;
                     break;
            default : printf("? unknown location (itemcode = %i)\n", b2);
         }
         break;
      default : printf("? unknown location (container = %02X)\n", b1);
   }
   // coordinates (only if not on body)
   if (b4 == FALSE)
   {
      b4 = itm->coordinates;
      switch(b3)
      {
         case TRUE  : b1 = (b4 & 0x06) >> 1;
                      b2 = 3 - ((b4 & 0x18) >> 3);
                      break;
         case FALSE : b1 = (b4 & 0x3E) >> 1;
                      b2 = (b4 & 0xC0) >> 6;
                      break;
         default : printf("? BUG\n");
      }
      printf("coordinates      = %2i, %i\n", b1, b2);
   }
   else
      printf("no coordinates on body\n");
   // special item code (17)  (byte 11 is for index)
   b1 = itm->specialitemcode;
   printf("ID unique / set  = %02X", (unsigned char) b1);
   b2 = itm->quantity & 0x000F;
   if (b2 == 0xA)
   {
      printf(" (collection = ");
      switch(b1)
      {
         case 0x00 : printf("Civerb's Vestments");     break;
         case 0x02 : printf("Hsarus' Trim");           break;
         case 0x04 : printf("Cleglaw's Brace");        break;
         case 0x06 : printf("Iratha's Finery");        break;
         case 0x08 : printf("Isenhart's Armory");      break;
         case 0x0A : printf("Vidala's Rig");           break;
         case 0x0C : printf("Milabrega's Regalia");    break;
         case 0x0E : printf("Cathan's Traps");         break;
         case 0x10 : printf("Tancred's Battlegear");   break;
         case 0x12 : printf("Sigon's Complete Steel"); break;
         case 0x14 : printf("Infernal Tools");         break;
         case 0x16 : printf("Berserker's Garb");       break;
         case 0x18 : printf("Death's Disguise");       break;
         case 0x1A : printf("Angelical Raiment");      break;
         case 0x1C : printf("Arctic Gear");            break;
         case 0x1E : printf("Arcanna's Tricks");       break;
         default : printf("? unknown");
      }
      printf(")");
   }
   else if (b2 == 0xE)
   {
      printf(" (unique = ");
      switch(b1)
      {
         case 0x00 : printf("The Gnasher");               break;
         case 0x02 : printf("Deathspade");                break;
         case 0x04 : printf("Bladebone");                 break;
         case 0x06 : printf("Mindrend");                  break;
         case 0x08 : printf("Rakescar");                  break;
         case 0x0A : printf("Fechmars Axe");              break;
         case 0x0C : printf("Goreshovel");                break;
         case 0x0E : printf("The Chieftan");              break;
         case 0x10 : printf("Brainhew");                  break;
         case 0x12 : printf("The Humongous");             break;
         case 0x14 : printf("Iros Torch");                break;
         case 0x16 : printf("Maelstromwrath");            break;
         case 0x18 : printf("Gravenspine");               break;
         case 0x1A : printf("Umes Lament");               break;
         case 0x1C : printf("Felloak");                   break;
         case 0x1E : printf("Knell Striker");             break;
         case 0x20 : printf("Rusthandle");                break;
         case 0x22 : printf("Stormeye");                  break;
         case 0x24 : printf("Stoutnail");                 break;
         case 0x26 : printf("Crushflange");               break;
         case 0x28 : printf("Bloodrise");                 break;
         case 0x2A : printf("The Generals Tan Do Li Ga"); break;
         case 0x2C : printf("Ironstone");                 break;
         case 0x2E : printf("Bonesob");                   break;
         case 0x30 : printf("Steeldriver");               break;
         case 0x32 : printf("Rixots Keen");               break;
         case 0x34 : printf("Blood Crescent");            break;
         case 0x36 : printf("Krintizs Skewer");           break;
         case 0x38 : printf("Gleamscythe");               break;
         case 0x3A : printf("Azurewrath");                break;
         case 0x3C : printf("Griswolds Edge");            break;
         case 0x3E : printf("Hellplague");                break;
         case 0x40 : printf("Culwens Point");             break;
         case 0x42 : printf("Shadowfang");                break;
         case 0x44 : printf("Soulflay");                  break;
         case 0x46 : printf("Kinemils Awl");              break;
         case 0x48 : printf("Blacktongue");               break;
         case 0x4A : printf("Ripsaw");                    break;
         case 0x4C : printf("The Patriarch");             break;
         case 0x4E : printf("Gull");                      break;
         case 0x50 : printf("The Diggler");               break;
         case 0x52 : printf("The Jade Tan Do");           break;
         case 0x54 : printf("Irices Shard");              break;
         case 0x56 : printf("The Dragon Chang");          break;
         case 0x58 : printf("Razortine");                 break;
         case 0x5A : printf("Bloodthief");                break;
         case 0x5C : printf("Lance of Yaggai");           break;
         case 0x5E : printf("The Tannr Gorerod");         break;
         case 0x60 : printf("Dimoaks Hew");               break;
         case 0x62 : printf("Steelgoad");                 break;
         case 0x64 : printf("Soul Harvest");              break;
         case 0x66 : printf("The Battlebranch");          break;
         case 0x68 : printf("Woestave");                  break;
         case 0x6A : printf("The Grim Reaper");           break;
         case 0x6C : printf("Bane Ash");                  break;
         case 0x6E : printf("Serpent Lord");              break;
         case 0x70 : printf("Lazarus Spire");             break;
         case 0x72 : printf("The Salamander");            break;
         case 0x74 : printf("The Iron Jang Bong");        break;
         case 0x76 : printf("Pluckeye");                  break;
         case 0x78 : printf("Witherstring");              break;
         case 0x7A : printf("Rimeraven");                 break;
         case 0x7C : printf("Piercerib");                 break;
         case 0x7E : printf("Pullspite");                 break;
         case 0x80 : printf("Wizendraw");                 break;
         case 0x82 : printf("Hellclap");                  break;
         case 0x84 : printf("Blastbark");                 break;
         case 0x86 : printf("Leadcrow");                  break;
         case 0x88 : printf("Ichorsting");                break;
         case 0x8A : printf("Hellcast");                  break;
         case 0x8C : printf("Doomspittle");               break;
         case 0x8E : printf("War Bonnet");                break;
         case 0x90 : printf("Tarnhelm");                  break;
         case 0x92 : printf("Coif of Glory");             break;
         case 0x94 : printf("Duskdeep");                  break;
         case 0x96 : printf("Wormskull");                 break;
         case 0x98 : printf("Howltusk");                  break;
         case 0x9A : printf("Undead Crown");              break;
         case 0x9C : printf("The Face of Horror");        break;
         case 0x9E : printf("Greyform");                  break;
         case 0xA0 : printf("Blinkbats Form");            break;
         case 0xA2 : printf("The Centurion");             break;
         case 0xA4 : printf("Twitchthroe");               break;
         case 0xA6 : printf("Darkglow");                  break;
         case 0xA8 : printf("Hawkmail");                  break;
         case 0xAA : printf("Sparking Mail");             break;
         case 0xAC : printf("Venomsward");                break;
         case 0xAE : printf("Iceblink");                  break;
         case 0xB0 : printf("Boneflesh");                 break;
         case 0xB2 : printf("Rockfleece");                break;
         case 0xB4 : printf("Rattlecage");                break;
         case 0xB6 : printf("Goldskin");                  break;
         case 0xB8 : printf("Victors Silk");              break;
         case 0xBA : printf("Heavenly Garb");             break;
         case 0xBC : printf("Pelta Lunata");              break;
         case 0xBE : printf("Umbral Disk");               break;
         case 0xC0 : printf("Stormguild");                break;
         case 0xC2 : printf("Wall of the Eyeless");       break;
         case 0xC4 : printf("Swordback Hold");            break;
         case 0xC6 : printf("Steelclash");                break;
         case 0xC8 : printf("Bverrit Keep");              break;
         case 0xCA : printf("The Ward");                  break;
         case 0xCC : printf("The Hand of Broc");          break;
         case 0xCE : printf("Bloodfist");                 break;
         case 0xD0 : printf("Chance Guards");             break;
         case 0xD2 : printf("Magefist");                  break;
         case 0xD4 : printf("Frostburn");                 break;
         case 0xD6 : printf("Hotspur");                   break;
         case 0xD8 : printf("Gorefoot");                  break;
         case 0xDA : printf("Treads of Cthon");           break;
         case 0xDC : printf("Goblin Toe");                break;
         case 0xDE : printf("Tearhaunch");                break;
         case 0xE0 : printf("Lenyms Cord");               break;
         case 0xE2 : printf("Snakecord");                 break;
         case 0xE4 : printf("Nightsmoke");                break;
         case 0xE6 : printf("Goldwrap");                  break;
         case 0xE8 : printf("Bladebuckle");               break;
         case 0xEA : printf("Nokozan Relic");             break;
         case 0xEC : printf("The Eye of Etlich");         break;
         case 0xEE : printf("The Mahim-Oak Curio");       break;
         case 0xF0 : printf("Nagelring");                 break;
         case 0xF2 : printf("Manald Heal");               break;
         case 0xF4 : printf("The Stone of Jordan");       break;
         case 0xF6 : printf("Amulet of the Viper");       break;
         case 0xF8 : printf("Staff of Kings");            break;
         case 0xFA : printf("Horadric Staff");            break;
         case 0xFC : printf("Hell Forge Hammer");         break;
         case 0xFE : printf("KhalimFlail");               break;
         case 0xFF : printf("SuperKhalimFlail");          break;
         default : printf("? unknown");
      }
      printf(")");
   }
   printf("\n");
   // properties1 (18 19 20 21)
/*
   dw1 = (unsigned long) itm->properties1;
   printf("properties1      = %08lX\n", dw1);
*/
   dw1 = (itm->properties1 & 0xFFFFFFF8lu) >> 3;
   dw2 = ((unsigned long) (itm->properties2 & 0x0007)) << 29;
   dw1 |= dw2;
   printf("properties 1 DWA = %08lX\n", dw1);
   // properties2 (22 23 24 25)
   dw1 = (itm->properties2 & 0xFFFFFFF8lu) >> 3;
   dw2 = ((unsigned long) (itm->container & 0x0007)) << 29;
   dw1 |= dw2;
   printf("magic attrb. DWB = %08lX", dw1);
   printf("\n");
}
// output text info on an item, only item with 15 bytes accepted
// IN : pointer on item
void item_info_15(unsigned char b[15])
{
   char tmp_str[15];
   int  i=0, x, y;
   // check item's structure integrity
   if (b[0] != 0x4A || b[1] != 0x4D || b[2] != 0x10)
      printf("? no 4A 4D 10 signature (%02X %02X %02X)\n", b[0], b[1], b[2]);
   if (b[3] != 0)
      printf("? byte 3 != 0 (%02X)\n", b[3]);
   if (b[4] != 0x38 || b[5] != 0 || b[6] != 0 || b[7] != 0)
   {
      printf("? no 38 00 00 00 signature"
             "(%02X %02X %02X %02X)\n", b[4], b[5], b[6], b[7]);
//      return;
   }
   if (b[14] != 0)
      printf("? byte 14 != 0 (%02X)\n", b[14]);
   // check location
   printf("location : %02X %02X\n", b[8], b[9]);
   x = 1 + ((b[8] >> 2) & 0x0F);
   y = 1 + ((b[9] & 0x01) << 1) + (b[8] & 0xC0 ? 1 : 0);
   if (b[9] == 0x00 || b[9] == 0x01)
   {
      // backpack
      if (b[8] & 0x03)
         printf("?1 invalid backpack (%02X & 0x03)\n", b[8]);
      else if ((b[8] > 0x24 && b[8] < 0x80) || (b[8] > 0xA4))
         printf("?2 invalid backpack\n");
      else
         printf("backpack (%2i, %i)\n", x, y);
   }
   else if (b[9] == 0x10 || b[9] == 0x11)
   {
      // stash
      if (b[8] & 0x03)
         printf("?1 invalid stash (%02X & 0x03)\n", b[8]);
      else if ((b[8] > 0x14 && b[8] < 0x80) || (b[8] > 0x94))
         printf("?2 invalid stash\n");
      else
         printf("stash (%2i, %i)\n", x, y);
   }
   else if ((b[9] & 0xF0) == 0xF0)
   {
      // belt
      if (b[9] == 0xFC || b[9] == 0xFD || b[9] == 0xFF)
         printf("somewhere in belt\n");
      else
         printf("? maybe in belt (%02X %02X)\n", b[8], b[9]);
      // belt ==> item +3
      if ((b[10] & 0x03) != 0x03)
         printf("? byte 10 of item does not have +3 despite in belt (%02X)\n", b[10]);
      b[10] &= 0xFC;
   }
   else if (b[9] == 0x0C || b[9] == 0x0D)
   {
      // horadric cube
      if (b[8] & 0x03)
         printf("?1 invalid horadric cube (%02X & 0x03)\n", b[8]);
      if (b[8] != 0x00 && b[8] != 0x04 && b[8] != 0x08 &&
          b[8] != 0x80 && b[8] != 0x84 && b[8] != 0x88)
      {
         printf("?2 invalid horadric cube\n");
      }
      else
         printf("horadric cube (%2i, %i)\n", x, y);
   }
   else
      printf("? unknown location\n");
   // item type
   while (strlen(item_15[i].id) != 0)
   {
      sprintf(tmp_str, "%02X %02X %02X %02X", b[10], b[11], b[12], b[13]);
      if (0 == strcmp(item_15[i].id, tmp_str))
      {
         printf("%s : %s\n", item_15[i].id, item_15[i].name);
         return;
      }
      i++;
   }
   printf("? unknown item (%02X %02X %02X %02X)\n", b[10], b[11], b[12], b[13]);
}
/* dump all the items
   return
       -1 : error
        0 : no items to output
    other : depending of savegame_version
               VER_100 : starting offset of the mercenary data
               VER_108 : starting offset of the last item
*/
int item_output(int offset_start)
{
   unsigned char * ubptr, *b0, *b1, *b2, *b3, *b4, *b6, *b11, *b12, *b13;
   unsigned short int * uwptr;
   int item_now = 0, item_max, offset1 = offset_start + 2, offset2;
   int is_last_item = FALSE, found;
   int socketed = FALSE, sock_now, sock_max;
   int size;
   ubptr = (unsigned char *) buf + offset_start;
   uwptr = (unsigned short int *) ubptr;
   item_max = * uwptr;
   if (item_max == 0)
      return 0; // return "no items to output"
   while (1)
   {
      // is it an item ?
      b0  = (unsigned char *) buf + offset1;
      b1  = b0 +  1;
      b2  = b0 +  2;
      b3  = b0 +  3;
      b4  = b0 +  4;
      b6  = b0 +  6;
      b11 = b0 + 11;
      b12 = b0 + 12;
      b13 = b0 + 13;
      if ( (*b0 == 0x4A) && (*b1 == 0x4D))
      {
         // it's an item. is it socketed ?
         if (*b3 & 0x08)
         {
            // it's socketed. can it really be ?
            if (socketed == TRUE)
            {
               // impossible
               printf("error : socketed items can't be socketed\n");
               return -1; // return "error"
            }
            else
            {
               // ok
               socketed = TRUE;
               sock_now = -1; // next item will be the first socketed gem
               
               // how many items socketed into it ?
               // byte 4 is for the item version
               switch ((*b4 & 0xF8) >> 3)
               {
                  // version 1.00 (items of 27 bytes)
                  case 0x00 :
                     sock_max = (*b6 & 0xE0) >> 5;
                     break;
                  // version 1.04 (items of 15 and 31 bytes)
                  case 0x03 :
                  case 0x07 :
                     sock_max  = (*b11 & 0x80) >> 7;
                     sock_max += (*b12 & 0x03) << 1;
                     break;
                  // version 1.08 (items of variable size)
                  case 0x10 :
                  case 0x14 :
                     sock_max = (*b13 & 0x70) >> 4;
                     break;
                  default : sock_max = 0; break;
               }
            }
         }
      }
      else
      {
         printf("error : not an item (%02X %02X)\n", *b0, *b1);
         return -1;
      }
      // is it the real last item ? gem socketed problem
      if ( (item_now + 1) == item_max)
      {
         if (socketed == FALSE)
            is_last_item = TRUE;
         else if ( (sock_now + 1) == sock_max)
            is_last_item = TRUE;
      }
      
      // just for readibility of the output
      if ( ! socketed)
      {
         if ((savegame_version == VER_100) && (size == 15 || size == 27))
            printf("\n");
      }
         
      // output the right header of the item
      if (socketed && (sock_now != -1))
      {
         // socketed item header
         printf("     %i/%i : %03X = ", sock_now + 1, sock_max, offset1);
      }
      else
      {
         // normal item header
         printf("item %3i : %03X = ", item_now + 1, offset1);
      }
      // don't dump the bytes if last item of version 1.08
      if ( (savegame_version == VER_108) && is_last_item)
         return offset1; // return the starting offset of the last item
      // search the starting offset of next item
      offset2 = offset1 + 3;
      found = FALSE;
      while ( ( ! found) && (offset2 < (buf_size - 3)) )
      {
         b0  = (unsigned char *) buf + offset2;
         b1  = b0 +  1;
         b2  = b0 +  2;
         if ( (*b0 == 0x4A) && (*b1 == 0x4D))
            found = TRUE;
         else
            offset2++;
      }
      if ( ! found)
      {
         printf("error : starting offset of next item not found\n");
         return -1; // return "error"
      }
      size = offset2 - offset1;
      // dump the bytes of the item
      print_array(buf + offset1, size);
      printf("\n");
      // additional infos that can be output
      if ( ! socketed && savegame_version == VER_100)
      {
         switch(size)
         {
            // version 1.00
            case 15 : item_info_15(buf + offset1); break;
            // version 1.04
            case 27 : item_info_27(buf + offset1); break;
            case 31 : break; // not done yet
         }
      }
      else if (savegame_version == VER_108)
      {
         // not done yet
      }
      
      // update the item count
      if(socketed)
      {
         if ( (sock_now + 1) < sock_max)
            sock_now++;
         else
         {
            socketed = FALSE;
            item_now++;
         }
      }
      else
         item_now++;
      // prepare next search
      offset1 = offset2;
      if (offset1 >= buf_size - 3)
      {
         printf("error : end of file");
         return -1; // return "error"
      }
      
      // breaking the loop ? only for version 1.00, because in version 1.08
      // we may have break it earlier
      if (savegame_version == VER_100 && is_last_item)
      {
         printf("\n");
         return offset1; // return the starting offset of mercenary data
      }
   }
}
// output a detail of the quests datas for a difficulty's level
void quest_output(int offset)
{
   // datas from act 1 to 4
   char data[][22] =
   {
        "act 1 enable a",
        "act 1 enable b",
        "act 1 quest 1",
        "act 1 quest 2",
        "act 1 quest 5",
        "act 1 quest 3",
        "act 1 quest 4",
        "act 1 quest 6",
        "act 2 enable a",
        "act 2 enable b",
        "act 2 quest 1",
        "act 2 quest 2",
        "act 2 quest 3",
        "act 2 quest 4",
        "act 2 quest 5",
        "act 2 quest 6",
        "act 3 enable a",
        "act 3 enable b",
        "act 3 quest 1",
        "act 3 quest 2",
        "act 3 quest 3",
        "act 3 quest 4",
        "act 3 quest 5",
        "act 3 quest 6",
        "act 4 enable a",
        "act 4 enable b",
        "act 4 quest 1",
        "act 4 quest 3",
        "act 4 quest 2",
        ""
   };
   
   // datas from act 5
   char data_a5[][22] =
   {
        "act 5 enable a", // 2
        "act 5 reserved", // 6
        "act 5 enable b", // 2
        "act 5 reserved", // 4
        "act 5 quest 1",  // 2
        "act 5 quest 2",  // 2
        "act 5 quest 3",  // 2
        "act 5 quest 4",  // 2
        "act 5 quest 5",  // 2
        "act 5 quest 6",  // 2
        "unknown data",   // 12
        ""
   };
   char size_a5[] = {2,6,2,4,2,2,2,2,2,2,12};
   
   unsigned char * uchar_ptr;
   int i;
   char level, old_act = '0', cur_act;
   // for futur use
   switch (offset)
   {
      case 0x08A : level = 1; break;
      case 0x0EA : level = 2; break;
      case 0x14A : level = 3; break;
      default    : level = 0; break;
   }
   // for each sub-datas
   for (i=0; strlen(data[i]) != 0; i++)
   {
      uchar_ptr = (unsigned char *) buf + offset;
      cur_act = data[i][4];
      if (cur_act != old_act)
      {
         printf("\n");
         old_act = cur_act;
      }
      printf("   %03X : ", offset);
      switch(level)
      {
         case 0 : printf("????"); break;
         case 1 : printf("nrml"); break;
         case 2 : printf("ngtm"); break;
         case 3 : printf("hell"); break;
      }
      printf(" %-*s = ", sizeof(data[i]), data[i]);
      print_bitfield(uchar_ptr, 2);
      printf("\n");
      offset += 2;
   }
   // act 5
   for (i=0; strlen(data_a5[i]) != 0; i++)
   {
      uchar_ptr = (unsigned char *) buf + offset;
      cur_act = data_a5[i][4];
      if (cur_act != old_act)
      {
         printf("\n");
         old_act = cur_act;
      }
      printf("   %03X : ", offset);
      switch(level)
      {
         case 0 : printf("????"); break;
         case 1 : printf("nrml"); break;
         case 2 : printf("ngtm"); break;
         case 3 : printf("hell"); break;
      }
      printf(" %-*s = ", sizeof(data_a5[i]), data_a5[i]);
      if (size_a5[i] == 2)
         print_bitfield(uchar_ptr, size_a5[i]);
      else
         print_array(uchar_ptr, size_a5[i]);
      printf("\n");
      offset += size_a5[i];
   }
}
// output the statut datas
// return -1 if error, otherwise next offset
int not_fixed_data(void)
{
   struct
   {
      char name[20];
      char size;
      char flag;
      char bit;
   } data[] =
     {  // name             size flag bit
        {"strength",          4,  0,  0},
        {"energy",            4,  0,  1},
        {"dexterity",         4,  0,  2},
        {"vitality",          4,  0,  3},
        {"stat points",       4,  0,  4},
        {"skill points",      4,  0,  5},
        {"tricky byte",       1, -1, -1}, // trick ! to be skip
        {"life current",      2,  0,  6},
        {"? life current",    2,  0,  6},
        {"life max",          2,  0,  7},
        {"? life max",        2,  0,  7},
        {"mana current",      2,  1,  0},
        {"? mana current",    2,  1,  0},
        {"mana max",          2,  1,  1},
        {"? mana max",        2,  1,  1},
        {"stamina current",   2,  1,  2},
        {"? stamina current", 2,  1,  2},
        {"stamina max",       2,  1,  3},
        {"? stamina max",     1,  1,  3}, // trick ! size not 2, only 1
        {"true player level", 4,  1,  4},
        {"experience",        4,  1,  5},
        {"gold",              4,  1,  6},
        {"gold in stash",     4,  1,  7},
        {"", -1, -1, -1}  // DO NOT REMOVE !
     };
   unsigned char  * uchar_ptr;  // ptr on 1 byte
   unsigned short * ushort_ptr; // ptr on 2 byte
   unsigned long  * ulong_ptr;  // ptr on 4 byte (also used for 3 bytes)
   unsigned char  * flag_base = (unsigned char *) buf + 0x232;
   int            offset = 0x235, index = 0, print_bool, current_flag, bit;
   unsigned char  flag[3];
   //inits
   flag[0] = * (flag_base);
   flag[1] = * (flag_base + 1);
   flag[2] = * (flag_base + 2);
   //main loop
   while(data[index].size != -1)
   {
      uchar_ptr  = ((unsigned char *) buf) + offset;
      ushort_ptr = (unsigned short *) uchar_ptr;
      ulong_ptr  = (unsigned long  *) uchar_ptr;
      print_bool = FALSE;
      if (data[index].flag == -1)
         print_bool = TRUE;
      else
      {
         current_flag = flag[data[index].flag];
         bit = 1 << data[index].bit;
         if (current_flag & bit)
            print_bool = TRUE;
      }
      if (print_bool)
      {
         printf("%03X = %-20s : ", offset, data[index].name);
         switch (data[index].size)
         {
            case 1 :  printf("%u",  * uchar_ptr);                break;
            case 2 :  printf("%u",  * ushort_ptr);               break;
            case 3 :  printf("%lu", (* ulong_ptr) & 0xFFFFFFLU); break;
            case 4 :  printf("%lu", * ulong_ptr);                break;
            default : printf("?");
         }
         printf("\n");
         offset += data[index].size;
      }
      index++;
   };
   // version problem ?
   if (flag[2])
   {
      printf("some unknown datas, due to reserved flag\n");
      printf("warning : at this point, you may have garbage\n");
//      return -1;
   }
   return offset;
}
// update info structure offset datas. Use : starting from skill header
void update_offset(int index_start, int offset_start)
{
   unsigned short offset = offset_start;
   int i = index_start;
   while (info[i].offset == 0x000)
   {
      info[i].offset = offset;
      offset += info[i].size;
      i++;
   }
}
/* check the integrity of the info structure
   look for gap, or overlaped datas
   return 0 if all is ok,
          1 if errors found
*/
int check_offsets(void)
{
   int i = 0, old_off = -1, old_size=-1, new_off, size, num_el, size_el;
   char * name, * old_name = NULL;
   while (info[i].offset != 0xFFF)
   {
      new_off = info[i].offset;
      name    = info[i].name;
      size    = info[i].size;
      num_el  = info[i].num_el;
      size_el = info[i].size_el;
      if(new_off <= old_off)
      {
         printf("Overlap of %i bytes\n", old_off - new_off + 1);
         printf("Size error for \"%s\" : %i\n", old_name, old_size);
         printf("OR offset error for \"%s\" : %03X\n", name, new_off);
         return 1;
      }
      if(size != num_el * size_el)
      {
         printf("size error for \"%s\" : %i != %i * %i\n", name, size, num_el, size_el);
         return 1;
      }
      if (old_off + 1 != new_off)
      {
         printf("Gap of %i bytes\n", new_off - old_off - 1);
         printf("size error for \"%s\" : %i\n", old_name, old_size);
         printf("OR offset error for \"%s\" : %03X\n", name, new_off);
         return 1;
      }
      old_off  = new_off + size - 1;
      old_size = size;
      old_name = name;
      i++;
   }
   return 0;
}
// output 1 data of the info structure
int view_info(int i)
{
   void * ptr;
   int  size;
   
   printf("%03X = %-*s : ", info[i].offset, sizeof(info[i].name), info[i].name);
   ptr  = (char *) buf + info[i].offset;
   size = info[i].size;
   switch(info[i].type)
   {
      case none      : break;
      case string    : print_string(ptr, size);   break;
      case array     : print_array(ptr, size);    break;
      case bitfield  : print_bitfield(ptr, size); break;
      case value     :
         switch(info[i].size)
         {
            case 1 : printf("%u",  * (unsigned char  *) ptr); break;
            case 2 : printf("%u",  * (unsigned short *) ptr); break;
            case 3 : printf("%lu", (* (unsigned long  *) ptr) & 0xFFFFFFLU); break;
            case 4 : printf("%lu", * (unsigned long  *) ptr); break;
            default :
               printf("error, size not between 1 and 4 : %i\n", info[i].size);
               return 1;
         }
         break;
      case quest_struct : printf("\n"); quest_output(info[i].offset); break;
      default : break;
   }
   printf("\n");
   return 0;
}
/* output many datas of the info structure
   return -1 if error, else index where found "0xFFF"
*/
int view_all_info(int index_start)
{
   int i=index_start;
   while (info[i].offset != 0xFFF)
   {
      if (view_info(i))
         return -1;
      // jump 1 more line, to read more easily
      if (0 == strcmp("fixed_03", info[i].name))
         printf("\n");
      else if (0 == strcmp("reserved waypoint", info[i].name))
         printf("\n");
      else if (0 == strcmp("npc statut header", info[i].name))
         printf("\n");
      else if (0 == strcmp("npc statut1 resrved", info[i].name))
         printf("\n");
      else if (0 == strcmp("npc statut2 resrved", info[i].name))
         printf("\n");
         
      i++;
   }
   return i;
}
// inits : copy the d2s file into memory
int my_init(char * argv[])
{
   FILE * in;
   long size;
   unsigned int number, size_int;
   unsigned long * ptr_tmp;
   in = fopen(argv[1], "rb");
   if (in == NULL)
   {
      printf("not open : %s\n", argv[1]);
      return 1;
   }
   fseek(in, 0, SEEK_END);
   size = ftell(in);
   size_int = (int) size;
   buf_size = size_int;
   if (size == -1)
   {
      printf("seek error\n");
      fclose(in);
      return 1;
   }
   fseek(in, 0, SEEK_SET);
   buf = (void *) malloc(size_int);
   if (buf == NULL)
   {
      printf("error malloc %i\n", size_int);
      fclose(in);
      return 1;
   }
   number = fread(buf, size_int, 1, in);
   fclose(in);
   if (number != 1)
   {
      printf("error reading %i bytes\n", size_int);
      return 1;
   }
   // check the version signature
   savegame_version = VER_ERR;
   savegame_expansion = FALSE;
   ptr_tmp = buf;
   if ( * ptr_tmp == 0xAA55AA55LU)
   {
      // it's a diablo 2 savegame
      ptr_tmp++;
      switch( * ptr_tmp)
      {
         case 0x00000047LU : savegame_version = VER_100; break;
         case 0x00000059LU : savegame_version = VER_108; break;
         case 0x0000005CLU : savegame_version = VER_109; break;
      }
      // check a bit in byte 0x18 (hardcore_flag) to know if it's expansion
      if ( (* ((unsigned char *) buf + 0x18)) & 0x20 )
         savegame_expansion = TRUE;
   }
   
   return 0;
}
// find and output the bytes of the last item, version 1.08
int ver_108_last_item(int offset)
{
   unsigned char * ptr_start = ((unsigned char *) buf) + offset,
                 * ptr_end = (unsigned char *) buf + buf_size - 57,
                 * ptr = ptr_start,
                 * ptr2;
   int b0, b1, b2, b3, find = FALSE, size;
   while ( (find == FALSE) && (ptr < ptr_end) )
   {
      b0 = * ptr;
      b1 = * (ptr + 1);
      b2 = * (ptr + 2);
      b3 = * (ptr + 3);
      if (b0 == 0x4A && b1 == 0x4D && b2 == 0x00 && b3 == 0x00)
         find = TRUE;
      else
         ptr++;
   }
   size = ptr - ptr_start;
   print_array(ptr_start, size);
   printf("\n");
   return offset + size;
}
// find and output the bytes of the last item for mercenary, version 1.08
int ver_108_last_item_merc(int offset)
{
   unsigned char * ptr_start = ((unsigned char *) buf) + offset,
                 * ptr_end = (unsigned char *) buf + buf_size - 50,
                 * ptr = ptr_start,
                 * ptr2;
   int b0, b1, b2, b3, find = FALSE, size;
   while ( (find == FALSE) && (ptr < ptr_end) )
   {
      b0 = * ptr;
      b1 = * (ptr + 1);
      b2 = * (ptr + 6);
      b3 = * (ptr + 7);
      if (b0 == 0xFF && b1 == 0x00 && b2 == 0xFF && b3 == 0x00)
         find = TRUE;
      else
         ptr++;
   }
   if (ptr == ptr_end)
      printf("\nFF 00 xx xx xx FF 00 not find, datas are now garbage\n");
   size = ptr - ptr_start;
   print_array(ptr_start, size);
   printf("\n");
   return offset + size;
}
// mercenary of version 1.08
int ver_108_mercenary(int offset)
{
   unsigned char * ptr = ((unsigned char *) buf) + offset;
   int b0, b1, b2, b3;
   unsigned long * ulptr;
   b0 = * ptr;
   b1 = * (ptr + 1);
   b2 = * (ptr + 2);
   b3 = * (ptr + 3);
   if (b0 == 0x4A && b1 == 0x4D && b2 == 0x00 && b3 == 0x00)
   {
      printf("%03X = hexa[4] : 4A 4D 00 00", offset);
      offset += 4;
      ptr += 4;
      b0 = * ptr;
      b1 = * (ptr + 1);
      b2 = * (ptr + 2);
      b3 = * (ptr + 3);
      if (b0 == 0x4A && b1 == 0x4D && b2 == 0x00 && b3 == 0x00)
      {
         printf("  (player is alive...)\n");
         printf("%03X = hexa[4] : 4A 4D 00 00  (...and he bought a mercenary before...)\n", offset);
         offset += 4;
         ptr += 4;
         b0 = * ptr;
         b1 = * (ptr + 1);
         if (b0 == 0x01 && b1 == 0x00)
            printf("%03X = hexa[2] : 01 00        (...who is dead)\n", offset);
         else if (b0 == 0x00 && b1 == 0x00)
            printf("%03X = hexa[2] : 00 00        (...who is alive)\n", offset);
         else
            printf("%03X = hexa[2] : %02X %02X    (error, unknown values)\n", offset, b0, b1);
         offset += 2;
         ptr += 2;
      }
      else
      {
         if (b0 == 0x00 && b1 == 0xFF && b2 == 0x00)
         {
            printf("  (player is alive...)\n");
            printf("%03X = hexa[1] : 00           (...but he never bought a mercenary)\n", offset);
            offset += 1;
            return offset;
         }
         else if (b0 == 0x01 && b1 == 0x00)
         {
            printf(" (player is dead...)\n");
            printf("%03X = hexa[2] : 01 00        (...and his mercenary is dead as well)\n", offset);
            offset += 2;
            ptr += 2;
         }
         else if (b0 == 0x00 && b1 == 0x00)
         {
            printf(" (player is dead...)\n");
            printf("%03X = hexa[2] : 00 00        (...but his mercenary is alive)\n", offset);
            offset += 2;
            ptr += 2;
         }
         else
         {
            printf("%03X = hexa[1] : %02X         (error, unknown value)\n", offset, b0, b1, b2);
            offset += 1;
            return offset;
         }
      }
      // description
      printf("%03X = description          : ", offset);
      print_array((unsigned char *) buf + offset, 8);
      offset += 8;
      printf("\n");
      // experience
      printf("%03X = experience           : ", offset);
      ulptr = (unsigned long) ((unsigned char *) buf + offset);
      printf("%lu\n", * ulptr & 0xFFFFFFFFLU);
      offset += 4;
      // item header
      printf("%03X = item's header        : ", offset);
      print_array((unsigned char *) buf + offset, 2);
      printf("\n");
      offset += 2;
      // nb of items
      printf("%03X = number of items      : ", offset);
      ulptr = (unsigned long) ((unsigned char *) buf + offset);
      printf("%lu\n", * ulptr & 0x0000FFFFLU);
      offset += 2;
      
      // items
      if (* ulptr & 0x0000FFFFLU)
      {
         offset = item_output(offset - 2);
         if (offset <= 0)
         {
            free(buf);
            exit(1);
         }
         
         offset = ver_108_last_item_merc(offset);
      }
      return offset;
   }
   else
   {
      if (b0 == 0x00)
         printf("%03X = hexa[1] : 00 (player is dead and he never bought a mercenary)\n", offset);
      else
         printf("%03X = hexa[1] : %02X (error, unknown value)\n", offset, b0);
      offset += 1;
      return offset;
   }
}
// last unknown datas
void ver_108_unknown(int offset)
{
   unsigned char * ptr = ((unsigned char *) buf) + offset;
   int i;
   for (i=0; i<8; i++)
   {
      printf("%03X = unknown %i : ", offset, i + 1);
      print_array(ptr, 6);
      printf("\n");
      offset += 6;
      ptr += 6;
   }
   printf("%03X = last data : ", offset);
   i = buf_size - offset;
   print_array(ptr, i);
   printf("\n");
}
// modify offset table. Needed if version 1.09
void modify_offset_table()
{
   int i=1;
   while (info[i].offset != 0xFFF)
   {
      info[i].offset += 12;
      i++;
   }
}
// start of the program
int main(int argc, char * argv[])
{
   int last_index, next_offset;
   // integrity check
   if (check_offsets())
      return 1;
   // syntax check
   if (argc == 1)
   {
      printf("\nusage : v <name>.d2s\n   ex : v pablo.d2s\n");
      return 0;
   }
   // output header
   printf("================\n"
          "  %s\n"
          "================\n" , argv[1]);
   if (my_init(argv))
      return 1;
   printf("savegame version find : ");
   switch(savegame_version)
   {
      case VER_100 :
         printf("from 1.00 to 1.06");
         if (savegame_expansion)
            printf(", in expansion (error)\n");
         else
            printf("\n");
         break;
         
      case VER_108 :
         printf("1.08, ");
         if (savegame_expansion)
            printf("in expansion\n");
         else
            printf("not converted to expansion\n");
         break;
      case VER_109 :
         printf("1.09\n");
         break;
         
      default : printf("error\n");
   }
   // break the program if unknown version
   if (savegame_version == VER_109 || savegame_version == VER_ERR)
   {
//      modify_offset_table();
      printf("this version can't be analysed yet\n");
      free(buf);
      exit(0);
   }
      
   // first part : fixed data (up to statut flags)
   last_index = view_all_info(0);
   if (last_index == -1)
   {
      printf("error\n");
      free(buf);
      return 1;
   }
   // second part : volatile datas (from strength, to gold in stash)
   next_offset = not_fixed_data();
   if (next_offset == -1)
   {
      printf("error\n");
      free(buf);
      return 1;
   }
   // third part : skills and start of items
   last_index++;
   update_offset(last_index, next_offset);
   last_index = view_all_info(last_index);
   if (last_index == -1)
   {
      printf("error\n");
      free(buf);
      return 1;
   }
   // forth part : items
   next_offset = item_output(info[last_index-1].offset);
   if (next_offset <= 0)
   {
      free(buf);
      exit(1);
   }
   // fifth part : corpse & mercenary
   switch(savegame_version)
   {
      case VER_100 :
         // no special problem, we have output the bytes of the last item,
         // and we're ready to output the mercenary data
         last_index++;
         update_offset(last_index, next_offset);
         last_index = view_all_info(last_index);
         if (last_index == -1)
         {
            printf("error\n");
            free(buf);
            return 1;
         }
         break;
         
      case VER_108 :
         // we didn't output the bytes of the last item, we still need to
         // find its size. Not only that we need to do some special process
         // to find it, but the mercenary may be equiped with items as well
         next_offset = ver_108_last_item(next_offset);
         next_offset = ver_108_mercenary(next_offset);
         // unknown datas at end of file
         ver_108_unknown(next_offset);
         
         break;
   }
   // end of program
   printf("\n");
   free(buf);
   return 0;
}
