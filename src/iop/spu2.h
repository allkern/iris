#ifndef SPU2_H
#define SPU2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SPU2_RAM_SIZE 0x200000

#define VP_VOLL(c, v)   (*((uint16_t*)(&spu2->r[0x400 * c + 0x000 + (v << 4)])))
#define VP_VOLR(c, v)   (*((uint16_t*)(&spu2->r[0x400 * c + 0x002 + (v << 4)])))
#define VP_PITCH(c, v)  (*((uint16_t*)(&spu2->r[0x400 * c + 0x004 + (v << 4)])))
#define VP_ADSR1(c, v)  (*((uint16_t*)(&spu2->r[0x400 * c + 0x006 + (v << 4)])))
#define VP_ADSR2(c, v)  (*((uint16_t*)(&spu2->r[0x400 * c + 0x008 + (v << 4)])))
#define VP_ENVX(c, v)   (*((uint16_t*)(&spu2->r[0x400 * c + 0x00A + (v << 4)])))
#define VP_VOLXL(c, v)  (*((uint16_t*)(&spu2->r[0x400 * c + 0x00C + (v << 4)])))
#define VP_VOLXR(c, v)  (*((uint16_t*)(&spu2->r[0x400 * c + 0x00E + (v << 4)])))
#define VA_SSA(c, v)    (*((uint16_t*)(&spu2->r[0x400 * c + 0x1C0 + (v * 12)])))
#define VA_LSAX(c, v)   (*((uint16_t*)(&spu2->r[0x400 * c + 0x1C4 + (v * 12)])))
#define VA_NAX(c, v)    (*((uint16_t*)(&spu2->r[0x400 * c + 0x1C8 + (v * 12)])))
#define S_PMON(c)       (*((uint32_t*)(&spu2->r[0x400 * c + 0x180])))
#define S_NON(c)        (*((uint32_t*)(&spu2->r[0x400 * c + 0x184])))
#define S_VMIXL(c)      (*((uint32_t*)(&spu2->r[0x400 * c + 0x188])))
#define S_VMIXEL(c)     (*((uint32_t*)(&spu2->r[0x400 * c + 0x18C])))
#define S_VMIXR(c)      (*((uint32_t*)(&spu2->r[0x400 * c + 0x190])))
#define S_VMIXER(c)     (*((uint32_t*)(&spu2->r[0x400 * c + 0x194])))
#define P_MMIX(c)       (*((uint32_t*)(&spu2->r[0x400 * c + 0x198])))
#define P_ATTR(c)       (*((uint32_t*)(&spu2->r[0x400 * c + 0x19A])))
#define A_IRQA(c)       (*((uint32_t*)(&spu2->r[0x400 * c + 0x19C])))
#define S_KON(c)        (*((uint32_t*)(&spu2->r[0x400 * c + 0x1A0])))
#define S_KOFF(c)       (*((uint32_t*)(&spu2->r[0x400 * c + 0x1A4])))
#define A_TSA(c)        (*((uint32_t*)(&spu2->r[0x400 * c + 0x1A8])))
#define P_DATA(c)       (*((uint16_t*)(&spu2->r[0x400 * c + 0x1AC])))
#define P_CTRL(c)       (*((uint16_t*)(&spu2->r[0x400 * c + 0x1AE])))
#define P_ADMAS(c)      (*((uint16_t*)(&spu2->r[0x400 * c + 0x1B0])))
#define A_ESA(c)        (*((uint32_t*)(&spu2->r[0x400 * c + 0x2E0])))
#define FB_SRC_A(c)     (*((uint32_t*)(&spu2->r[0x400 * c + 0x2E4])))
#define FB_SRC_B(c)     (*((uint32_t*)(&spu2->r[0x400 * c + 0x2E8])))
#define IIR_DEST_A0(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x2EC])))
#define IIR_DEST_A1(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x2F0])))
#define ACC_SRC_A0(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x2F4])))
#define ACC_SRC_A1(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x2F8])))
#define ACC_SRC_B0(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x2FC])))
#define ACC_SRC_B1(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x300])))
#define IIR_SRC_A0(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x304])))
#define IIR_SRC_A1(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x308])))
#define IIR_DEST_B0(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x30C])))
#define IIR_DEST_B1(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x310])))
#define ACC_SRC_C0(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x314])))
#define ACC_SRC_C1(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x318])))
#define ACC_SRC_D0(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x31C])))
#define ACC_SRC_D1(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x320])))
#define IIR_SRC_B1(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x324])))
#define IIR_SRC_B0(c)   (*((uint32_t*)(&spu2->r[0x400 * c + 0x328])))
#define MIX_DEST_A0(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x32C])))
#define MIX_DEST_A1(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x330])))
#define MIX_DEST_B0(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x334])))
#define MIX_DEST_B1(c)  (*((uint32_t*)(&spu2->r[0x400 * c + 0x338])))
#define A_EEA(c)        (*((uint32_t*)(&spu2->r[0x400 * c + 0x33C])))
#define P_ENDX(c)       (*((uint32_t*)(&spu2->r[0x400 * c + 0x340])))
#define P_STAT(c)       (*((uint16_t*)(&spu2->r[0x400 * c + 0x344])))
#define P_ENDS(c)       (*((uint16_t*)(&spu2->r[0x400 * c + 0x346])))
#define P_MVOLL(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x760])))
#define P_MVOLR(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x762])))
#define P_EVOLL(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x764])))
#define P_EVOLR(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x766])))
#define P_AVOLL(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x768])))
#define P_AVOLR(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x76A])))
#define P_BVOLL(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x76C])))
#define P_BVOLR(c)      (*((uint16_t*)(&spu2->r[0x28 * c + 0x76E])))
#define P_MVOLXL(c)     (*((uint16_t*)(&spu2->r[0x28 * c + 0x770])))
#define P_MVOLXR(c)     (*((uint16_t*)(&spu2->r[0x28 * c + 0x772])))
#define IIR_ALPHA(c)    (*((uint16_t*)(&spu2->r[0x28 * c + 0x774])))
#define ACC_COEF_A(c)   (*((uint16_t*)(&spu2->r[0x28 * c + 0x776])))
#define ACC_COEF_B(c)   (*((uint16_t*)(&spu2->r[0x28 * c + 0x778])))
#define ACC_COEF_C(c)   (*((uint16_t*)(&spu2->r[0x28 * c + 0x77A])))
#define ACC_COEF_D(c)   (*((uint16_t*)(&spu2->r[0x28 * c + 0x77C])))
#define IIR_COEF(c)     (*((uint16_t*)(&spu2->r[0x28 * c + 0x77E])))
#define FB_ALPHA(c)     (*((uint16_t*)(&spu2->r[0x28 * c + 0x780])))
#define FB_X(c)         (*((uint16_t*)(&spu2->r[0x28 * c + 0x782])))
#define IN_COEF_L(c)    (*((uint16_t*)(&spu2->r[0x28 * c + 0x784])))
#define IN_COEF_R(c)    (*((uint16_t*)(&spu2->r[0x28 * c + 0x786])))
#define SPDIF_OUT       (*((uint16_t*)(&spu2->r[0x7C0])))
#define SPDIF_MODE      (*((uint16_t*)(&spu2->r[0x7C6])))
#define SPDIF_MEDIA     (*((uint16_t*)(&spu2->r[0x7C8])))
#define SPDIF_COPY      (*((uint16_t*)(&spu2->r[0x7CA])))

/* Memory ranges:
    1f900000-1f90017f CORE0 Voice settings
    1f900180-1f9001b1 CORE0 Common settings
    1f9001c0-1f9002df CORE0 Voice address settings
    1f9002e0-1f900347 CORE0 Misc. settings
    1f900400-1f90057f CORE1 Voice settings
    1f900580-1f9005b1 CORE1 Common settings
    1f9005c0-1f9006df CORE1 Voice address settings
    1f9006e0-1f900747 CORE1 Reverb/Misc. settings
    1f900760-1f900787 CORE0 Volume/Reverb settings
    1f900788-1f9007af CORE1 Volume/Reverb settings
    1f9007c0-1f9007cf S/PDIF settings

   Breakdown:
    1f900000-1f90017f CORE0 Voice settings:
        1f900XX0 Voice X (0-23) VOLL                  (S) voice volume (left)
        1f900XX2 Voice X (0-23) VOLR                  (S) voice volume (right)
        1f900XX4 Voice X (0-23) PITCH                 (S) voice pitch
        1f900XX6 Voice X (0-23) ADSR1                 (S) voice envelope (AR, DR, SL)
        1f900XX8 Voice X (0-23) ADSR2                 (S) voice envelope (SR, RR)
        1f900XXa Voice X (0-23) ENVX                  (S) voice envelope (current value)
        1f900XXc Voice X (0-23) VOLXL                 (S) voice volume (current value left)
        1f900XXe Voice X (0-23) VOLXR                 (S) voice volume (current value right)
    1f900180-1f9001b1 CORE0 Common settings
        1f900180 PMON                                 (P) pitch modulation on
        1f900184 NON                                  (P) noise generator on
        1f900188 VMIXL                                (P) voice output mixing (dry left)
        1f90018C VMIXEL                               (P) voice output mixing (wet left)
        1f900190 VMIXR                                (P) voice output mixing (dry right)
        1f900194 VMIXER                               (P) voice output mixing (wet right)
        1f900198 MMIX                                 (P) output type after voice mixing
        1f90019A ATTR                                 (P) core attributes
        1f90019C IRQA                                 (P) IRQ address 
        1f9001A0 KON                                  (P) key on (start voice sound generation)
        1f9001A4 KOFF                                 (P) key off (end voice sound generation)
        1f9001A8 TSA                                  (P) DMA transfer start address
        1f9001AC DATA                                 (S) DMA data register
        1f9001AE CTRL                                 (S) DMA control register
        1f9001B0 ADMAS                                (S) AutoDMA (ADMA) status
    1f9001c0-1f9002df CORE0 Voice address settings
    1f9002e0-1f900347 CORE0 Misc. settings
    1f900400-1f90057f CORE1 Voice settings
    1f900580-1f9005b1 CORE1 Common settings
    1f9005c0-1f9006df CORE1 Voice address settings
    1f9006e0-1f900747 CORE1 Reverb/Misc. settings
    1f900760-1f900787 CORE0 Volume/Reverb settings
    1f900788-1f9007af CORE1 Volume/Reverb settings
    1f9007c0-1f9007cf S/PDIF settings
*/

struct spu2_voice {
    int playing;
    uint32_t* ssa;
    uint32_t* lsax;
    uint32_t* nax;
};

struct spu2_core {
    struct spu2_voice v[24];
};

struct ps2_spu2 {
    uint16_t ram[SPU2_RAM_SIZE >> 1];

    // Register area (1f900000-1f9007ff)
    uint8_t r[0x800];

    struct spu2_core c[2];
};

struct spu2_sample {
    union {
        uint32_t u32;
        uint16_t u16[2];
    };
};

struct ps2_spu2* ps2_spu2_create(void);
void ps2_spu2_init(struct ps2_spu2* spu2);
uint64_t ps2_spu2_read16(struct ps2_spu2* spu2, uint32_t addr);
void ps2_spu2_write16(struct ps2_spu2* spu2, uint32_t addr, uint64_t data);
void ps2_spu2_destroy(struct ps2_spu2* spu2);
struct spu2_sample ps2_spu2_get_sample(struct ps2_spu2* spu);

#ifdef __cplusplus
}
#endif

#endif