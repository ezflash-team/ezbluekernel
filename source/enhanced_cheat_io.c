#include <gba_base.h>
#include <string.h>
#include "ezkernel.h"
#include "showcht.h"
#include "Ezcard_OP.h"

extern u32 pROMPATCH_OFFSET[MAX_ROM_PATCH_BYTES];
extern u8 pROMPATCH_VALUE[MAX_ROM_PATCH_BYTES];
extern CHT_ROM_GROUP pROMGROUP[MAX_ROM_CHEAT_GROUPS];

/* Function = Read original ROM bytes without touching BLUE's shared file. */
static u32 ReadOriginalByte(u32 offset,u8 *value,void *context)
{
    FIL *file=(FIL*)context;
    UINT count=0;
    return f_lseek(file,offset)==FR_OK &&
        f_read(file,value,1,&count)==FR_OK && count==1;
}

/* Function = Resolve ROMIF groups before loading or modifying a game. */
u32 BlueCheatPrepareRom(const char *filename)
{
    FIL file;
    if (!gl_rom_group_count) return 1;
    if (f_open(&file,filename,FA_READ)!=FR_OK) return 0;
    EvaluateEnhancedCheatGroups(ReadOriginalByte,&file,(u32)f_size(&file));
    f_close(&file);
    return 1;
}

/* Function = Write a ROM byte using the cartridge's halfword bus and banking. */
static u32 WriteDDRByte(u32 offset,u8 value,void *context)
{
    u32 size=*(u32*)context;
    u32 within=offset & 0x7FFFFFu;
    vu16 *address;
    u16 word;
    if (offset>=size) return 0;
    SetPSRampage((offset>>23)*0x1000u);
    address=(vu16*)((u8*)PSRAMBase_S98+(within & ~1u));
    word=*address;
    if (offset & 1u) word=(word & 0x00FFu)|((u16)value<<8);
    else word=(word & 0xFF00u)|value;
    *address=word;
    SetPSRampage(0);
    return 1;
}

/* Function = Apply selected ROM writes after BLUE's compatibility patches. */
void BlueCheatApplyDDR(u32 rom_size)
{
    ApplyActiveRomPatches(WriteDDRByte,&rom_size,rom_size);
}

/* Function = Apply final ROM bytes to the NOR block before it is programmed. */
void BlueCheatApplyNOR(u32 block_offset,u32 valid_bytes)
{
    u32 group_index;
    for(group_index=0;group_index<gl_rom_group_count;++group_index) {
        CHT_ROM_GROUP *group=&pROMGROUP[group_index];
        u32 index;
        if(!group->active) continue;
        for(index=0;index<group->patch_count;++index) {
            u32 record=group->patch_start+index;
            u32 offset=pROMPATCH_OFFSET[record];
            if(offset>=block_offset && offset-block_offset<valid_bytes)
                pReadCache[offset-block_offset]=pROMPATCH_VALUE[record];
        }
    }
}
