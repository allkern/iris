#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "ipu.hpp"
#include "ee/dmac.hpp"
#include "ee/intc.hpp"

namespace iris::ipu {

/**
  * The majority of this code is based upon Play!'s implementation of the IPU.
  * All the relevant files are located in the following links:
  *
  * https://github.com/jpd002/Play-/tree/master/Source/ee (IPU base, some tables)
  * https://github.com/jpd002/Play--Framework/tree/master/include/mpeg2 (Table includes)
  * https://github.com/jpd002/Play--Framework/tree/master/src/mpeg2 (Tables)
  * https://github.com/jpd002/Play--Framework/blob/master/src/idct/IEEE1180.cpp (IDCT transformation)
  */

uint32_t Ipu::inverse_scan_zigzag[0x40] =
{
    0,  1,  5,  6, 14, 15, 27, 28,
    2,  4,  7,  13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43,
    9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

uint32_t Ipu::inverse_scan_alternate[0x40] =
{
    0, 4, 6, 20, 22, 36, 38, 52,
    1, 5, 7, 21, 23, 37, 39, 53,
    2, 8, 19, 24, 34, 40, 50, 54,
    3, 9, 18, 25, 35, 41, 51, 55,
    10, 17, 26, 30, 42, 46, 56, 60,
    11, 16, 27, 31, 43, 47, 57, 61,
    12, 15, 28, 32, 44, 48, 58, 62,
    13, 14, 29, 33, 45, 49, 59, 63
};

uint32_t Ipu::quantizer_linear[0x20] =
{
    0,  2,  4,  6,  8,  10,  12,  14,
    16,  18,  20,  22,  24,  26,  28,  30,
    32,  34,  36,  38,  40,  42,  44,  46,
    48,  50,  52,  54,  56,  58,  60,  62
};

uint32_t Ipu::quantizer_nonlinear[0x20] =
{
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  10,  12,  14,  16,  18,  20,  22,
    24,  28,  32,  36,  40,  44,  48,  52,
    56,  64,  72,  80,  88,  96,  104, 112,
};

static const uint8_t default_intra_IQ[0x40] =
{
    8,  16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83,
};

static const uint8_t default_nonintra_IQ[0x40] =
{
    16, 17, 18, 19, 20, 21, 22, 23,
    17, 18, 19, 20, 21, 22, 23, 24,
    18, 19, 20, 21, 22, 23, 24, 25,
    19, 20, 21, 22, 23, 24, 26, 27,
    20, 21, 22, 23, 25, 26, 27, 28,
    21, 22, 23, 24, 26, 27, 28, 30,
    22, 23, 24, 26, 27, 28, 30, 31,
    23, 24, 25, 27, 28, 30, 31, 33,
};

Ipu::Ipu(ee::intc::Intc* intc, ee::dmac::Dmac* dmac) : intc(intc), dmac(dmac)
{
    //Generate CrCb->RGB conversion map
    for (unsigned int i = 0; i < 0x40; i += 0x8)
    {
        for (unsigned int j = 0; j < 0x10; j += 2)
        {
            int index = j + (i * 4);
            crcb_map[index + 0x00] = (j / 2) + i;
            crcb_map[index + 0x01] = (j / 2) + i;

            crcb_map[index + 0x10] = (j / 2) + i;
            crcb_map[index + 0x11] = (j / 2) + i;
        }
    }

    for (int i = 0; i < 256; i++) {
        const float c = (float)i - 128.0f;

        csc_r_cr[i] =  1.402f   * c;
        csc_g_cb[i] = -0.34414f * c;
        csc_g_cr[i] = -0.71414f * c;
        csc_b_cb[i] =  1.772f   * c;
    }

    //I'm assuming the dithering process rounds down so I've rounded the matrix values down
    dither_mtx[0][0] = -4;
    dither_mtx[0][1] = 0;
    dither_mtx[0][2] = -3;
    dither_mtx[0][3] = 1;
    dither_mtx[1][0] = 2;
    dither_mtx[1][1] = -2;
    dither_mtx[1][2] = 3;
    dither_mtx[1][3] = -1;
    dither_mtx[2][0] = -3;
    dither_mtx[2][1] = 1;
    dither_mtx[2][2] = -4;
    dither_mtx[2][3] = 0;
    dither_mtx[3][0] = 3;
    dither_mtx[3][1] = -1;
    dither_mtx[3][2] = 2;
    dither_mtx[3][3] = -2;
}

void Ipu::reset()
{
    dct_coeff = nullptr;
    VDEC_table = nullptr;
    in_FIFO.reset();
    out_FIFO.reset();
    prepare_IDCT();
    memcpy(intra_IQ, default_intra_IQ, sizeof(intra_IQ));
    memcpy(nonintra_IQ, default_nonintra_IQ, sizeof(nonintra_IQ));

    ctrl.error_code = false;
    ctrl.start_code = false;
    ctrl.intra_DC_precision = 0;
    ctrl.MPEG1 = false;
    ctrl.picture_type = 0;
    ctrl.busy = false;
    ctrl.coded_block_pattern = 0;
    command = 0;
    command_option = 0;
    bytes_left = 0;
    command_decoding = false;
}

void Ipu::run()
{
    if (ctrl.busy)
    {
        // try
        // {
            switch (command)
            {
                case 0x01:
                    if (in_FIFO.f.size())
                    {
                        if (process_IDEC())
                            finish_command();
                    }
                    break;
                case 0x02:
                    if (in_FIFO.f.size())
                    {
                        if (process_BDEC())
                            finish_command();
                    }
                    break;
                case 0x03:
                    if (in_FIFO.f.size())
                        process_VDEC();
                    break;
                case 0x04:
                    if (in_FIFO.f.size())
                        process_FDEC();
                    break;
                case 0x05:
                    if (setiq_state == SETIQ_STATE::ADVANCE)
                    {
                        if (!in_FIFO.advance_stream(command_option & 0x3F))
                            break;

                        setiq_state = SETIQ_STATE::POPULATE_TABLE;
                    }
                    while (bytes_left && in_FIFO.f.size())
                    {
                        uint32_t value;
                        if (!in_FIFO.get_bits(value, 8))
                            break;
                        in_FIFO.advance_stream(8);
                        int index = 64 - bytes_left;
                        if (command_option & (1 << 27))
                            nonintra_IQ[index] = value & 0xFF;
                        else
                            intra_IQ[index] = value & 0xFF;
                        bytes_left--;
                    }
                    if (bytes_left <= 0)
                        ctrl.busy = false;
                    break;
                case 0x06:
                    while (bytes_left && in_FIFO.f.size())
                    {
                        uint128_t quad = in_FIFO.f.front();
                        in_FIFO.f.pop_front();
                        for (int i = 0; i < 8; i++)
                        {
                            int index = (32 - bytes_left) >> 1;
                            VQCLUT[index] = quad.u16[i];
                            bytes_left -= 2;
                        }
                    }
                    if (bytes_left <= 0)
                        ctrl.busy = false;
                    break;
                case 0x07:
                    if (in_FIFO.f.size())
                    {
                        if (process_CSC())
                            finish_command();
                    }
                    break;
                case 0x08:
                    if (in_FIFO.f.size())
                    {
                        if (process_PACK())
                            finish_command();
                    }
                    break;
            }
        // }
        // catch (VLC_Error& e)
        // {
        //     std::iris_debug(this, "VLC error: {}", e.what());

        //     ctrl.error_code = true;
        //     finish_command();
        // }
    }
    if (can_write_FIFO()) {
        dmac->channels[ee::dmac::IPU_TO].dreq = 1;
        // iris_debug(this, "set ipu_to dreq");
        ee::dmac::handle_ipu_to_transfer(dmac);
    }
    if (can_read_FIFO()) {
        dmac->channels[ee::dmac::IPU_FROM].dreq = 1;
        iris_debug(this, "Output FIFO ready");
        // iris_debug(this, "set ipu_from dreq out={:x}", out_FIFO.f.size());
        ee::dmac::handle_ipu_from_transfer(dmac);
    }
}

#define IPU_BATCH_STEPS 64

void Ipu::run_until_stalled()
{
    for (int i = 0; i < IPU_BATCH_STEPS; i++)
    {
        if (!ctrl.busy)
            break;

        run();

        if (!in_FIFO.f.size())
            break;
    }
}

void Ipu::finish_command()
{
    ctrl.busy = false;
    command_decoding = false;

    ee::intc::irq(intc, ee::intc::IPU);
}

bool Ipu::process_IDEC()
{
    while (true)
    {
        switch (idec.state)
        {
            case IDEC_STATE::DELAY:
                //Play delays IDEC execution before consuming FIFO data.
                idec.state = IDEC_STATE::ADVANCE;
                return false;
            case IDEC_STATE::ADVANCE:
                iris_debug(this, "Advance stream");
                if (!in_FIFO.advance_stream(command_option & 0x3F))
                    return false;
                idec.state = IDEC_STATE::MACRO_I_TYPE;
                break;
            case IDEC_STATE::MACRO_I_TYPE:
            {
                iris_debug(this, "Decode macroblock I type");

                bool invalid = false;

                if (!macroblock_I_pic.get_symbol(in_FIFO, idec.macro_type, &invalid))
                {
                    if (!invalid)
                        return false;

                    iris_warning(this, "IDEC: no macroblock type matches the stream, aborting the command");

                    ctrl.error_code = true;

                    return true;
                }

                idec.state = IDEC_STATE::DCT_TYPE;
            }
                break;
            case IDEC_STATE::DCT_TYPE:
                iris_debug(this, "Decode DCT");
                if (idec.decodes_dct)
                {
                    uint32_t value;
                    if (!in_FIFO.get_bits(value, 1))
                        return false;
                    in_FIFO.advance_stream(1);

                    //Play expects this to be zero for IDEC intra path.
                    // if (value != 0)
                    //     throw VLC_Error("IDEC unsupported DCT type");
                }
                idec.state = IDEC_STATE::QSC;
                break;
            case IDEC_STATE::QSC:
                iris_debug(this, "Decode QSC");
                if (idec.macro_type & 0x10)
                {
                    if (!in_FIFO.get_bits(idec.qsc, 5))
                        return false;
                    in_FIFO.advance_stream(5);
                }
                idec.state = IDEC_STATE::INIT_BDEC;
                break;
            case IDEC_STATE::INIT_BDEC:
                //We don't need to advance, and the macroblock is always intra so no need to check for a CBP.
                iris_debug(this, "Init BDEC");
                bdec.state = BDEC_STATE::RESET_DC;
                bdec.intra = true;
                bdec.quantizer_step = idec.qsc;
                bdec.out_fifo = &idec.temp_fifo;
                ctrl.coded_block_pattern = 0x3F;
                bdec.block_index = 0;
                bdec.cur_channel = 0;
                bdec.reset_dc = idec.blocks_decoded == 0;
                bdec.check_start_code = false;
                idec.state = IDEC_STATE::READ_BLOCK;
                break;
            case IDEC_STATE::READ_BLOCK:
                iris_debug(this, "Read macroblock");
                if (!process_BDEC())
                    return false;
                idec.blocks_decoded++;
                idec.state = IDEC_STATE::INIT_CSC;
                break;
            case IDEC_STATE::INIT_CSC:
                //BDEC outputs in RAW16. CSC works in RAW8, so we need to convert appropriately.
                iris_debug(this, "Init CSC");
                for (int i = 0; i < RAW_BLOCK_SIZE / 8; i++)
                {
                    uint128_t quad = idec.temp_fifo.f.front();
                    idec.temp_fifo.f.pop_front();

                    int offset = i * 8;

                    for (int j = 0; j < 8; j++)
                    {
                        int16_t data = (int16_t)quad.u16[j];
                        if (data < 0)
                            data = 0;
                        if (data > 255)
                            data = 255;
                        csc.block[offset + j] = (uint8_t)data;
                    }
                }
                csc.state = CSC_STATE::CONVERT;
                csc.block_index = 0;
                csc.macroblocks = 1;

                idec.state = IDEC_STATE::EXEC_CSC;
                break;
            case IDEC_STATE::EXEC_CSC:
                iris_debug(this, "Exec CSC");
                if (!process_CSC())
                    return false;
                idec.state = IDEC_STATE::CHECK_START_CODE;
                break;
            case IDEC_STATE::CHECK_START_CODE:
            {
                iris_debug(this, "Check start code");
                uint32_t code;
                if (!in_FIFO.get_bits(code, 8))
                    return false;
                if (!code)
                {
                    idec.state = IDEC_STATE::VALID_START_CODE;
                    in_FIFO.byte_align();
                }
                else
                    idec.state = IDEC_STATE::MACRO_INC;
            }
                break;
            case IDEC_STATE::VALID_START_CODE:
            {
                iris_debug(this, "Validate start code");
                uint32_t code;
                if (!in_FIFO.get_bits(code, 24))
                {
                    //If we already detected 8 zero bits in CHECK_START_CODE but don't have
                    //enough bits for a full 24-bit validation code, treat this as a valid
                    //start-code boundary and finish the command.
                    idec.state = IDEC_STATE::DONE;
                    break;
                }

                if (code == 0)
                {
                    //Consume one byte of zero padding and keep searching for 0x000001.
                    if (!in_FIFO.advance_stream(8))
                        return false;
                }
                else if (code == 1)
                {
                    idec.state = IDEC_STATE::DONE;
                }
                else
                {
                    // throw VLC_Error("IDEC start code invalid");
                }
            }
                break;
            case IDEC_STATE::MACRO_INC:
            {
                iris_debug(this, "Macroblock increment");
                uint32_t inc;
                if (!macroblock_increment.get_symbol(in_FIFO, inc))
                    return false;

                // if ((inc & 0xFFFF) != 1)
                //     throw VLC_Error("IDEC invalid macroblock increment");

                idec.state = IDEC_STATE::MACRO_I_TYPE;
            }
                break;
            case IDEC_STATE::DONE:
                iris_debug(this, "IDEC done!");
                return true;
        }
    }
}

bool Ipu::process_BDEC()
{
    while (true)
    {
        switch (bdec.state)
        {
            case BDEC_STATE::ADVANCE:
                if (!in_FIFO.advance_stream(command_option & 0x3F))
                    return false;
                bdec.state = BDEC_STATE::GET_CBP;
                break;
            case BDEC_STATE::GET_CBP:
                iris_debug(this, "Get CBP!");
                if (!bdec.intra)
                {
                    uint32_t pattern;
                    if (!cbp.get_symbol(in_FIFO, pattern))
                        return false;
                    ctrl.coded_block_pattern = pattern;
                    iris_debug(this, "CBP: {}", ctrl.coded_block_pattern);
                }
                else
                    ctrl.coded_block_pattern = 0x3F;
                bdec.state = BDEC_STATE::RESET_DC;
                break;
            case BDEC_STATE::RESET_DC:
                if (bdec.reset_dc)
                {
                    iris_debug(this, "Reset DC!");

                    int16_t value;
                    switch (ctrl.intra_DC_precision)
                    {
                        case 0:
                            value = 128;
                            break;
                        case 1:
                            value = 256;
                            break;
                        case 2:
                            value = 512;
                            break;
                    }

                    for (int i = 0; i < 3; i++)
                        bdec.dc_predictor[i] = value;
                }
                bdec.state = BDEC_STATE::BEGIN_DECODING;
                break;
            case BDEC_STATE::BEGIN_DECODING:
                iris_debug(this, "Begin decoding block {}!", bdec.block_index);

                bdec.cur_block = bdec.blocks[bdec.block_index];
                memset(bdec.cur_block, 0, sizeof(int16_t) * 64);

                if (ctrl.coded_block_pattern & (1 << (5 - bdec.block_index)))
                {
                    //If block index is Cb/Cr, set the channel to the relevant chromance one
                    if (bdec.block_index > 3)
                        bdec.cur_channel = bdec.block_index - 3;
                    else
                        bdec.cur_channel = 0;

                    if (bdec.intra && ctrl.intra_VLC_table)
                    {
                        iris_debug(this, "Use DCT coefficient table 1");
                        dct_coeff = &dct_coeff1;
                    }
                    else
                    {
                        iris_debug(this, "Use DCT coefficient table 0");
                        dct_coeff = &dct_coeff0;
                    }

                    bdec.read_coeff_state = BDEC_Command::READ_COEFF::INIT;
                    bdec.state = BDEC_STATE::READ_COEFFS;
                }
                else
                    bdec.state = BDEC_STATE::LOAD_NEXT_BLOCK;
                break;
            case BDEC_STATE::READ_COEFFS:
            {
                iris_debug(this, "Read coeffs!");
                if (!BDEC_read_coeffs())
                    return false;
                iris_debug(this, "Dequantize!");
                dequantize(bdec.cur_block);
                iris_debug(this, "Inverse scan!");
                inverse_scan(bdec.cur_block);
                iris_debug(this, "IDCT!");

                int16_t temp[0x40];
                memcpy(temp, bdec.cur_block, 0x40 * sizeof(int16_t));
                perform_IDCT(temp, bdec.cur_block);
                bdec.state = BDEC_STATE::LOAD_NEXT_BLOCK;
            }
                break;
            case BDEC_STATE::LOAD_NEXT_BLOCK:
                iris_debug(this, "Load next block!");
                bdec.block_index++;
                if (bdec.block_index == 6)
                    bdec.state = BDEC_STATE::DONE;
                else
                    bdec.state = BDEC_STATE::BEGIN_DECODING;
                break;
            case BDEC_STATE::DONE:
            {
                iris_debug(this, "BDEC done!");
                uint128_t quad;
                for (int i = 0; i < 8; i++)
                {
                    memcpy(quad.u8, bdec.blocks[0] + (i * 8), sizeof(int16_t) * 8);
                    bdec.out_fifo->f.push_back(quad);
                    memcpy(quad.u8, bdec.blocks[1] + (i * 8), sizeof(int16_t) * 8);
                    bdec.out_fifo->f.push_back(quad);
                }

                for (int i = 0; i < 8; i++)
                {
                    memcpy(quad.u8, bdec.blocks[2] + (i * 8), sizeof(int16_t) * 8);
                    bdec.out_fifo->f.push_back(quad);
                    memcpy(quad.u8, bdec.blocks[3] + (i * 8), sizeof(int16_t) * 8);
                    bdec.out_fifo->f.push_back(quad);
                }

                for (int i = 0; i < 8; i++)
                {
                    memcpy(quad.u8, bdec.blocks[4] + (i * 8), sizeof(int16_t) * 8);
                    bdec.out_fifo->f.push_back(quad);
                }

                for (int i = 0; i < 8; i++)
                {
                    memcpy(quad.u8, bdec.blocks[5] + (i * 8), sizeof(int16_t) * 8);
                    bdec.out_fifo->f.push_back(quad);
                }

                if (bdec.check_start_code)
                    bdec.state = BDEC_STATE::CHECK_START_CODE;
                else
                    return true;
            }
                break;
            case BDEC_STATE::CHECK_START_CODE:
            {
                uint32_t bits;
                if (in_FIFO.get_bits(bits, 8))
                {
                    if (!bits)
                    {
                        ctrl.start_code = true;
                        iris_debug(this, "Start code detected!");
                    }
                    return true;
                }
                else
                    return false;
            }
        }
    }
}

void Ipu::inverse_scan(int16_t *block)
{
    int16_t temp[0x40];
    memcpy(temp, block, 0x40 * sizeof(int16_t));

    int id;
    for (int i = 0; i < 0x40; i++)
    {
        if (ctrl.alternate_scan)
            id = inverse_scan_alternate[i];
        else
            id = inverse_scan_zigzag[i];
        block[i] = temp[id];
    }
}

void Ipu::dequantize(int16_t *block)
{
    int q_scale;
    if (ctrl.nonlinear_Q_step)
        q_scale = quantizer_nonlinear[bdec.quantizer_step];
    else
        q_scale = quantizer_linear[bdec.quantizer_step];
    if (bdec.intra)
    {
        switch (ctrl.intra_DC_precision)
        {
            case 0:
                block[0] *= 8;
                break;
            case 1:
                block[0] *= 4;
                break;
            case 2:
                block[0] *= 2;
                break;
            default:
                iris_debug(this, "Dequantize: Intra DC precision == 3!");
                block[0] = 0;
                break;
        }

        for (int i = 1; i < 0x40; i++)
        {
            int16_t sign;
            if (!block[i])
                sign = 0;
            else
            {
                if (block[i] > 0)
                    sign = 1;
                else
                    sign = (int16_t)0xFFFF;
            }

            int32_t scaled = (int32_t)block[i] * (int32_t)intra_IQ[i] * q_scale * 2;
            block[i] = (int16_t)(scaled / 32);

            if (sign)
            {
                if (!(block[i] & 0x1))
                {
                    block[i] -= sign;
                    block[i] |= 1;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < 64; i++)
        {
            int16_t sign;
            if (!block[i])
                sign = 0;
            else
            {
                if (block[i] > 0)
                    sign = 1;
                else
                    sign = 0xFFFF;
            }

            int32_t scaled = (((int32_t)block[i] * 2) + sign) * (int32_t)nonintra_IQ[i] * q_scale;
            block[i] = (int16_t)(scaled / 32);

            if (sign)
            {
                if (!(block[i] & 0x1))
                {
                    block[i] -= sign;
                    block[i] |= 1;
                }
            }
        }
    }

    //Saturation step
    for (int i = 0; i < 64; i++)
    {
        if (block[i] > 2047)
            block[i] = 2047;
        if (block[i] < -2048)
            block[i] = -2048;
    }
}

//IDCT code here taken from mpeg2decode
//Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved.

#ifndef PI
# ifdef M_PI
#  define PI M_PI
# else
#  define PI 3.14159265358979323846
# endif
#endif
void Ipu::prepare_IDCT()
{
    int freq, time;
    double scale;

    for (freq=0; freq < 8; freq++)
    {
        scale = (freq == 0) ? sqrt(0.125) : 0.5;
        for (time=0; time<8; time++)
        {
            IDCT_table[freq][time] = scale*cos((PI/8.0)*freq*(time + 0.5));
        }
    }
}

void Ipu::perform_IDCT(const int16_t* pUV, int16_t* pXY)
{
    int i, j, k, v;
    double partial_product;
    double tmp[64];

    for (i=0; i<8; i++)
    {
        for (j=0; j<8; j++)
        {
            partial_product = 0.0;

            for (k=0; k<8; k++)
            {
                partial_product+= IDCT_table[k][j]*pUV[8*i+k];
            }

            tmp[8*i+j] = partial_product;
        }
    }

  /* Transpose operation is integrated into address mapping by switching
     loop order of i and j */

    for (j=0; j<8; j++)
    {
        for (i=0; i<8; i++)
        {
            partial_product = 0.0;

            for (k=0; k<8; k++)
            {
                partial_product+= IDCT_table[k][i]*tmp[8*k+j];
            }

            v = (int) floor(partial_product+0.5);
            pXY[8*i+j] = v;
        }
    }
}

//End IDCT code

bool Ipu::BDEC_read_coeffs()
{
    while (true)
    {
        switch (bdec.read_coeff_state)
        {
            case BDEC_Command::READ_COEFF::INIT:
                iris_debug(this, "READ_COEFF Init!");
                bdec.read_diff_state = BDEC_Command::READ_DIFF::SIZE;
                bdec.subblock_index = 0;
                if (bdec.intra)
                {
                    bdec.subblock_index = 1;
                    bdec.read_coeff_state = BDEC_Command::READ_COEFF::READ_DC_DIFF;
                }
                else
                    bdec.read_coeff_state = BDEC_Command::READ_COEFF::CHECK_END;
                break;
            case BDEC_Command::READ_COEFF::READ_DC_DIFF:
                iris_debug(this, "READ_COEFF Read DC diffs!");
                if (!BDEC_read_diff())
                    return false;
                bdec.cur_block[0] = (int16_t)(bdec.dc_predictor[bdec.cur_channel] + bdec.dc_diff);
                bdec.dc_predictor[bdec.cur_channel] = bdec.cur_block[0];
                bdec.read_coeff_state = BDEC_Command::READ_COEFF::CHECK_END;
                break;
            case BDEC_Command::READ_COEFF::CHECK_END:
                iris_debug(this, "READ_COEFF Check end of block!");
            {
                uint32_t end = 0;
                if (!dct_coeff->get_end_of_block(in_FIFO, end))
                    return false;
                if (bdec.subblock_index && end)
                    bdec.read_coeff_state = BDEC_Command::READ_COEFF::SKIP_END;
                else
                    bdec.read_coeff_state = BDEC_Command::READ_COEFF::COEFF;
            }
                break;
            case BDEC_Command::READ_COEFF::COEFF:
                iris_debug(this, "READ_COEFF Read coeffs!");
            {
                RunLevelPair pair;
                if (!bdec.subblock_index)
                {
                    if (!dct_coeff->get_runlevel_pair_dc(in_FIFO, pair, ctrl.MPEG1))
                        return false;
                }
                else
                {
                    if (!dct_coeff->get_runlevel_pair(in_FIFO, pair, ctrl.MPEG1))
                        return false;
                }
                iris_debug(this, "Run: {} Level: {}", pair.run, pair.level);
                bdec.subblock_index += pair.run;

                if (bdec.subblock_index < 0x40)
                {
                    bdec.cur_block[bdec.subblock_index] = (int16_t)pair.level;
                }
                else
                {
                    // throw VLC_Error("BDEC coefficient index overflow");
                }
                bdec.subblock_index++;
                bdec.read_coeff_state = BDEC_Command::READ_COEFF::CHECK_END;
            }
                break;
            case BDEC_Command::READ_COEFF::SKIP_END:
                iris_debug(this, "READ_COEFF Skip end!");
                if (!dct_coeff->get_skip_block(in_FIFO))
                    return false;
                return true;
        }
    }
}

bool Ipu::BDEC_read_diff()
{
    while (true)
    {
        switch (bdec.read_diff_state)
        {
            case BDEC_Command::READ_DIFF::SIZE:
                iris_debug(this, "READ_DIFF SIZE!");
                if (bdec.cur_channel == 0)
                {
                    if (!lum_table.get_symbol(in_FIFO, bdec.dc_size))
                        return false;
                }
                else
                {
                    if (!chrom_table.get_symbol(in_FIFO, bdec.dc_size))
                        return false;
                }
                bdec.read_diff_state = BDEC_Command::READ_DIFF::DIFF;
                break;
            case BDEC_Command::READ_DIFF::DIFF:
                iris_debug(this, "READ_DIFF DIFF!");
                if (!bdec.dc_size)
                    bdec.dc_diff = 0;
                else
                {
                    uint32_t result = 0;
                    if (!in_FIFO.get_bits(result, bdec.dc_size))
                        return false;

                    if (!in_FIFO.advance_stream(bdec.dc_size))
                        return false;

                    int16_t half_range = 1 << (bdec.dc_size - 1);
                    bdec.dc_diff = (int16_t)result;
                    if (bdec.dc_diff < half_range)
                        bdec.dc_diff += 1 - (2 * half_range);
                }
                return true;
        }
    }
}

void Ipu::convert_RGB32_to_RGB16(const uint8_t* rgb32, uint16_t* rgb16, bool dithering)
{
    for (int i = 0; i < 16; ++i)
    {
        for (int j = 0; j < 16; ++j)
        {
            //It's worth noting that bit 30 is the alpha bit for RGB16, not bit 31.
            const int index = j + (i * 16);
            const int dither = dithering ? dither_mtx[i & 3][j & 3] : 0;
            const int r = std::max(0, std::min(rgb32[4 * index] + dither, 255)) >> 3;
            const int g = std::max(0, std::min(rgb32[4 * index + 1] + dither, 255)) >> 3;
            const int b = std::max(0, std::min(rgb32[4 * index + 2] + dither, 255)) >> 3;
            const int a = rgb32[4 * index + 3] == 0x40;
            rgb16[index] = r | g  << 5 | b << 10 | a << 15;
        }
    }
}

void Ipu::process_VDEC()
{
    int table = command_option >> 26;
    switch (table)
    {
        case 0:
            iris_debug(this, "MBAI");
            VDEC_table = &macroblock_increment;
            break;
        case 1:
            iris_debug(this, "MBT");
            switch (ctrl.picture_type)
            {
                case 0x1:
                    iris_debug(this, "I pic");
                    VDEC_table = &macroblock_I_pic;
                    break;
                case 0x2:
                    iris_debug(this, "P pic");
                    VDEC_table = &macroblock_P_pic;
                    break;
                case 0x3:
                    iris_debug(this, "B pic");
                    VDEC_table = &macroblock_B_pic;
                    break;
            }
            break;
        case 2:
            iris_debug(this, "MC");
            VDEC_table = &motioncode;
            break;
    }

    while (true)
    {
        switch (vdec_state)
        {
            case VDEC_STATE::ADVANCE:
                if (!in_FIFO.advance_stream(command_option & 0x3F))
                    return;
                vdec_state = VDEC_STATE::DECODE;
                break;
            case VDEC_STATE::DECODE:
                if (!VDEC_table->get_symbol(in_FIFO, command_output))
                    return;
                else
                    vdec_state = VDEC_STATE::DONE;
                break;
            case VDEC_STATE::DONE:
                iris_debug(this, "VDEC done! Output: ${:08x} infifo={}", command_output, in_FIFO.f.size());
                finish_command();
                return;
        }
    }
}

void Ipu::process_FDEC()
{
    while (true)
    {
        switch (fdec_state)
        {
            case VDEC_STATE::ADVANCE:
                if (!in_FIFO.advance_stream(command_option & 0x3F))
                    return;
                fdec_state = VDEC_STATE::DECODE;
                break;
            case VDEC_STATE::DECODE:
                if (!in_FIFO.get_bits(command_output, 32))
                    return;
                fdec_state = VDEC_STATE::DONE;
                break;
            case VDEC_STATE::DONE:
                finish_command();
                iris_debug(this, "FDEC result: ${:08x}", command_output);
                return;
        }
    }
}

bool Ipu::process_CSC()
{
    while (true)
    {
        switch (csc.state)
        {
            case CSC_STATE::BEGIN:
                if (csc.macroblocks)
                {
                    csc.state = CSC_STATE::READ;
                    csc.block_index = 0;
                }
                else
                    csc.state = CSC_STATE::DONE;
                break;
            case CSC_STATE::READ:
                if (csc.block_index == RAW_BLOCK_SIZE)
                    csc.state = CSC_STATE::CONVERT;
                else
                {
                    do {
                        uint32_t value;
                        if (!in_FIFO.get_bits(value, 8))
                            return false;
                        in_FIFO.advance_stream(8);
                        csc.block[csc.block_index] = value & 0xFF;
                        csc.block_index++;
                    } while (csc.block_index < RAW_BLOCK_SIZE);
                }
                break;
            case CSC_STATE::CONVERT:
            {
                uint8_t rgb32[4 * RGB_BLOCK_SIZE];

                uint8_t* lum_block = csc.block;
                uint8_t* cb_block = csc.block + 0x100;
                uint8_t* cr_block = csc.block + 0x140;

                uint16_t alphaTh0 = (TH0 & 0x1FF);
                uint16_t alphaTh1 = (TH1 & 0x1FF);

                for (int index = 0; index < 256; index++)
                {
                    const unsigned int c = crcb_map[index];
                    const uint8_t cb = cb_block[c];
                    const uint8_t cr = cr_block[c];

                    float lum = lum_block[index];

                    float r = lum + csc_r_cr[cr];
                    float g = lum + csc_g_cb[cb] + csc_g_cr[cr];
                    float b = lum + csc_b_cb[cb];

                    if (r < 0)
                        r = 0;
                    if (r > 255)
                        r = 255;
                    if (g < 0)
                        g = 0;
                    if (g > 255)
                        g = 255;
                    if (b < 0)
                        b = 0;
                    if (b > 255)
                        b = 255;

                    uint8_t alpha;
                    if (r < alphaTh0 && g < alphaTh0 && b < alphaTh0)
                        alpha = 0;
                    else if (r < alphaTh1 && g < alphaTh1 && b < alphaTh1)
                        alpha = 0x40;
                    else
                        alpha = 0x80;

                    rgb32[4 * index] = (uint8_t)r;
                    rgb32[4 * index + 1] = (uint8_t)g;
                    rgb32[4 * index + 2] = (uint8_t)b;
                    rgb32[4 * index + 3] = alpha;
                }

                uint128_t quad;
                if (csc.use_RGB16)
                {
                    uint16_t rgb16[RGB_BLOCK_SIZE];

                    convert_RGB32_to_RGB16(rgb32, rgb16, csc.use_dithering);

                    for (int i = 0; i < RGB_BLOCK_SIZE / 8; i++)
                    {
                        for (int j = 0; j < 8; j++)
                        {
                            quad.u16[j] = rgb16[j + (i * 8)];
                        }
                        out_FIFO.f.push_back(quad);
                    }
                }
                else
                {
                    for (int i = 0; i < RGB_BLOCK_SIZE / 4; i++)
                    {
                        for (int j = 0; j < 4; j++)
                        {
                            int index = 4 * (j + (i * 4));
                            uint8_t r = rgb32[index];
                            uint8_t g = rgb32[index + 1];
                            uint8_t b = rgb32[index + 2];
                            uint8_t a = rgb32[index + 3];
                            uint32_t color = r | g << 8 | b << 16 | a << 24;
                            quad.u32[j] = color;
                        }
                        out_FIFO.f.push_back(quad);
                    }
                }
                csc.macroblocks--;
                csc.state = CSC_STATE::BEGIN;
                dmac->channels[ee::dmac::IPU_FROM].dreq = 1;
                // iris_debug(this, "set ipu_from dreq out={:x}", out_FIFO.f.size());
                ee::dmac::handle_ipu_from_transfer(dmac);
            }
                break;
            case CSC_STATE::DONE:
                iris_debug(this, "CSC done!");
                return true;
        }
    }
}

bool Ipu::process_PACK()
{
    while (true)
    {
        switch (pack.state)
        {
            case PACK_STATE::BEGIN:
                if (pack.macroblocks)
                {
                    pack.state = PACK_STATE::READ;
                    pack.block_index = 0;
                }
                else
                    pack.state = PACK_STATE::DONE;
                break;
            case PACK_STATE::READ:
                if (pack.block_index == 4 * RGB_BLOCK_SIZE)
                    pack.state = PACK_STATE::CONVERT;
                else
                {
                    uint32_t value;
                    if (!in_FIFO.get_bits(value, 8))
                        return false;
                    in_FIFO.advance_stream(8);
                    pack.block[pack.block_index] = value & 0xFF;
                    pack.block_index++;
                }
                break;
            case PACK_STATE::CONVERT:
            {
                uint16_t rgb16[RGB_BLOCK_SIZE];

                convert_RGB32_to_RGB16(pack.block, rgb16, pack.use_dithering);

                uint128_t quad;
                if (pack.use_RGB16)
                {
                    for (int i = 0; i < RGB_BLOCK_SIZE / 8; ++i)
                    {
                        for (int j = 0; j < 8; ++j)
                        {
                            quad.u16[j] = rgb16[j + (i * 8)];
                        }
                        out_FIFO.f.push_back(quad);
                    }
                }
                else
                {
                    int clut_r[16];
                    int clut_g[16];
                    int clut_b[16];
                    for (int i = 0; i < 16; ++i)
                    {
                        clut_r[i] = VQCLUT[i] & 0x1F;
                        clut_g[i] = (VQCLUT[i] >> 5) & 0x1F;
                        clut_b[i] = (VQCLUT[i] >> 10) & 0x1F;
                    }

                    auto closest_index = [&](uint16_t color) {
                        const int r = color & 0x1F;
                        const int g = (color >> 5) & 0x1F;
                        const int b = (color >> 10) & 0x1F;

                        uint8_t index = 0;
                        int min_distance = std::numeric_limits<int>::max();
                        for (uint8_t i = 0; i < 16; ++i)
                        {
                            const int dr = r - clut_r[i];
                            const int dg = g - clut_g[i];
                            const int db = b - clut_b[i];
                            const int distance = dr * dr + dg * dg + db * db;

                            // TODO: If two distances are the same which index is used?
                            if (min_distance > distance)
                            {
                                index = i;
                                min_distance = distance;
                            }
                        }

                        return index;
                    };

                    for (int i = 0; i < RGB_BLOCK_SIZE / 32; ++i)
                    {
                        for (int j = 0; j < 16; ++j)
                        {
                            int index = 2 * j + (i * 32);
                            const uint16_t color16_low = rgb16[index];
                            const uint16_t color16_high = rgb16[index + 1];
                            quad.u8[j] = closest_index(color16_high) << 4 | closest_index(color16_low);
                        }
                        out_FIFO.f.push_back(quad);
                    }
                }
                pack.macroblocks--;
                pack.state = PACK_STATE::BEGIN;
                dmac->channels[ee::dmac::IPU_FROM].dreq = 1;
                iris_debug(this, "set ipu_from dreq out={:x}", out_FIFO.f.size());
                ee::dmac::handle_ipu_from_transfer(dmac);
            }
                break;
            case PACK_STATE::DONE:
                iris_debug(this, "PACK done!");
                return true;
        }
    }
}

uint64_t Ipu::read_command()
{
    uint64_t reg = 0;
    reg |= command_output;
    reg |= (uint64_t)command_decoding << 63UL;

    iris_debug(this, "Read command: ${:08x}", command_output);

    return reg;
}

uint32_t Ipu::read_control()
{
    uint32_t reg = 0;
    reg |= in_FIFO.f.size();
    reg |= (ctrl.coded_block_pattern & 0x3F) << 8;
    reg |= ctrl.error_code << 14;
    reg |= ctrl.start_code << 15;
    reg |= ctrl.intra_DC_precision << 16;
    reg |= ctrl.alternate_scan << 20;
    reg |= ctrl.intra_VLC_table << 21;
    reg |= ctrl.nonlinear_Q_step << 22;
    reg |= ctrl.MPEG1 << 23;
    reg |= ctrl.picture_type << 24;
    reg |= ctrl.busy << 31;
    return reg;
}

uint32_t Ipu::read_BP()
{
    uint32_t reg = 0;
    uint8_t fifo_size = in_FIFO.f.size();

    //Check for FP bit
    if (in_FIFO.bit_pointer && fifo_size)
    {
        reg |= 1 << 16;
        fifo_size--;
    }
    reg |= in_FIFO.bit_pointer;
    reg |= fifo_size << 8;
    iris_debug(this, "Read BP: ${:08x}", reg);
    return reg;
}

uint64_t Ipu::read_top()
{
    uint64_t reg = 0;
    int max_bits = (in_FIFO.f.size() * 128) - in_FIFO.bit_pointer;
    if (max_bits > 32)
        max_bits = 32;
    uint32_t next_data;
    if (max_bits > 0)
        in_FIFO.get_bits(next_data, max_bits);
    else
        next_data = 0;
    reg |= next_data << (32 - max_bits);

    /** Note on max_bits
      * This seems to be undocumented behavior. FMV libraries use this bit to determine how much data is left
      * in the input FIFO, for the purposes of flushing their bitstream cache.
      * If this bit is set, this means that there are less than 32 bits left in the FIFO, and BP is then used.
      * If this is not set, at least 32 bits are available.
      *
      * This is needed for rare cases where games peek in the FIFO when less than 32 bits are available.
      */
    reg |= ((uint64_t)command_decoding | (uint64_t)(max_bits < 32)) << 63UL;
    return reg;
}

void Ipu::write_command(uint32_t value)
{
    iris_debug(this, "Write command: ${:08x}", value);
    if (!ctrl.busy)
    {
        ctrl.busy = true;
        command = (value >> 28);
        command_option = value & ~0xF0000000;
        ctrl.error_code = false;
        ctrl.start_code = false;
        switch (command)
        {
            case 0x00:
                iris_debug(this, "BCLR");
                in_FIFO.reset();
                in_FIFO.bit_pointer = command_option & 0x7F;
                finish_command();
                break;
            case 0x01:
                iris_debug(this, "IDEC");
                idec.state = IDEC_STATE::DELAY;
                idec.macro_type = 0;
                idec.qsc = (command_option >> 16) & 0x1F;
                idec.decodes_dct = command_option & (1 << 24);
                idec.blocks_decoded = 0;
                csc.use_RGB16 = command_option & (1 << 27);
                break;
            case 0x02:
                iris_debug(this, "BDEC");
                bdec.state = BDEC_STATE::ADVANCE;
                bdec.out_fifo = &out_FIFO;
                ctrl.coded_block_pattern = 0x3F;
                bdec.block_index = 0;
                bdec.cur_channel = 0;
                bdec.quantizer_step = (command_option >> 16) & 0x1F;
                bdec.reset_dc = command_option & (1 << 26);
                bdec.intra = command_option & (1 << 27);
                bdec.check_start_code = true;
                break;
            case 0x03:
                iris_debug(this, "VDEC");
                command_decoding = true;
                vdec_state = VDEC_STATE::ADVANCE;
                process_VDEC();
                break;
            case 0x04:
                iris_debug(this, "FDEC");
                command_decoding = true;
                fdec_state = VDEC_STATE::ADVANCE;
                process_FDEC();
                break;
            case 0x05:
                iris_debug(this, "SETIQ");
                bytes_left = 64;
                setiq_state = SETIQ_STATE::ADVANCE;
                break;
            case 0x06:
                iris_debug(this, "SETVQ");
                bytes_left = 32;
                break;
            case 0x07:
                iris_debug(this, "CSC");
                csc.state = CSC_STATE::BEGIN;
                csc.macroblocks = command_option & 0x7FF;
                csc.use_RGB16 = command_option & (1 << 27);
                csc.use_dithering = command_option & (1 << 26);
                break;
            case 0x08:
                iris_debug(this, "PACK");
                pack.state = PACK_STATE::BEGIN;
                pack.macroblocks = command_option & 0x7FF;
                pack.use_RGB16 = command_option & (1 << 27);
                pack.use_dithering = command_option & (1 << 26);
                break;
            case 0x09:
                iris_debug(this, "SETTH");
                TH0 = command_option & 0x1FF;
                TH1 = (command_option >> 16) & 0x1FF;
                finish_command();
                break;
        }
    }
}

void Ipu::write_control(uint32_t value)
{
    iris_debug(this, "Write control: ${:08x}", value);
    ctrl.intra_DC_precision = (value >> 16) & 0x3;
    ctrl.alternate_scan = value & (1 << 20);
    ctrl.intra_VLC_table = value & (1 << 21);
    ctrl.nonlinear_Q_step = value & (1 << 22);
    ctrl.MPEG1 = value & (1 << 23);
    ctrl.picture_type = (value >> 24) & 0x7;
    if (value & (1 << 30))
    {
        command = 0;
        in_FIFO.reset();
        out_FIFO.reset();
        // Note: A control reset does a forced command end, meaning it will
        //       force the procedure of a command stopping even if there is
        //       no command currently active, causing an interrupt to the core.
        //       Fightbox relies on this behaviour to boot and play its first
        //       two videos.
        finish_command();
    }
}

bool Ipu::can_read_FIFO()
{
    return out_FIFO.f.size() > 0;
}

bool Ipu::can_write_FIFO()
{
    return in_FIFO.f.size() < 8;
}

uint128_t Ipu::read_FIFO()
{
    if (!out_FIFO.f.size()) {
        dmac->channels[ee::dmac::IPU_FROM].dreq = 0;
        return { 0 };
    }
    uint128_t quad = out_FIFO.f.front();
    out_FIFO.f.pop_front();
    if (!out_FIFO.f.size()) {
        iris_debug(this, "clear ipu_from dreq");
        dmac->channels[ee::dmac::IPU_FROM].dreq = 0;
    }
    return quad;
}

uint128_t Ipu::peek_in_FIFO()
{
    if (in_FIFO.f.empty())
        return { 0 };

    return in_FIFO.f.front();
}

void Ipu::write_FIFO(uint128_t quad)
{
    iris_debug(this, "Write FIFO: ${:08x}_{:08x}_{:08x}_{:08x}", quad.u32[3], quad.u32[2], quad.u32[1], quad.u32[0]);

    //Certain games (Theme Park, Neo Contra, etc) read command output without sending a command.
    //They expect to read the first word of a newly started IPU_TO transfer.
    if (in_FIFO.f.size() == 0 && !ctrl.busy)
    {
        command_output = quad.u32[0];
        command_output = (command_output >> 24) | (((command_output >> 16) & 0xFF) << 8) |
                         (((command_output >> 8) & 0xFF) << 16) | (command_output << 24);
    }
    if (in_FIFO.f.size() == 7)
    {
        dmac->channels[ee::dmac::IPU_TO].dreq = 0;
    }
    if (in_FIFO.f.size() >= 8)
    {
    }
    in_FIFO.f.push_back(quad);
    in_FIFO.bit_cache_dirty = true;
}

Ipu* create(logger::Logger* logger, ee::dmac::Dmac* dmac, ee::intc::Intc* intc) {
    Ipu* ipu = new Ipu(intc, dmac);

    ipu->logger = logger;
    ipu->logger_id = logger::register_source(logger, "ipu");

    return ipu;
}

void reset(Ipu* ipu) {
    ipu->reset();
}

void destroy(Ipu* ipu) {
    delete ipu;
}

bool is_busy(Ipu* ipu) {
    return ipu->ctrl.busy;
}

void run(Ipu* ipu) {
    // The IPU drives its own register writes, so a command that kicks the
    // pipeline can re-enter here before the previous run has unwound.
    static bool running = false;

    if (running) return;

    running = true;

    ipu->run_until_stalled();

    running = false;
}

uint64_t read64(Ipu* ipu, uint32_t addr) {
    switch (addr) {
        case 0x10002000: return ipu->read_command();
        case 0x10002010: return ipu->read_control();
        case 0x10002020: return ipu->read_BP();
        case 0x10002030: return ipu->read_top();
    }

    iris_error(ipu, "Unhandled 64-bit read from address {:08x}", addr);

    return 0;
}

void write64(Ipu* ipu, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x10002000: ipu->write_command(data); run(ipu); return;
        case 0x10002010: ipu->write_control(data); run(ipu); return;
        case 0x10002020: return; // (W) write_BP
        case 0x10002030: return; // (W) write_top
    }

    iris_fatal_error(ipu, "Unhandled 64-bit write to address {:08x}", addr);
}

uint128_t read128(Ipu* ipu, uint32_t addr) {
    run(ipu);

    switch (addr) {
        case 0x10007000: return ipu->read_FIFO();
        case 0x10007010: return ipu->peek_in_FIFO();
    }

    iris_error(ipu, "Unhandled 128-bit read from address {:08x}", addr);

    return { 0 };
}

void write128(Ipu* ipu, uint32_t addr, uint128_t data) {
    switch (addr) {
        case 0x10007000: break; // (R) read_FIFO
        case 0x10007010: ipu->write_FIFO(data); run(ipu); return;
    }

    iris_fatal_error(ipu, "Unhandled 128-bit write to address {:08x}", addr);
}

uint128_t fifo_read(Ipu* ipu) {
    return ipu->read_FIFO();
}

void fifo_write(Ipu* ipu, uint128_t data) {
    ipu->write_FIFO(data);
}

}
