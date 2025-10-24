#include "ps1_mcd.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

void ps1_mcd_flush_block(struct ps1_mcd_state* mcd, int addr) {
    fseek(mcd->file, addr, SEEK_SET);
    fwrite(&mcd->buf[addr], 1, 128, mcd->file);
}

/*
  Send Reply Comment
  81h  N/A   Memory card address
  52h  FLAG  Send Read Command (ASCII "R"), Receive FLAG Byte
  00h  5Ah   Receive Memory Card ID1
  00h  5Dh   Receive Memory Card ID2
  MSB  (00h) Send Address MSB  ;\sector number (0..3FFh)
  LSB  (pre) Send Address LSB  ;/
  00h  5Ch   Receive Command Acknowledge 1  ;<-- late /ACK after this byte-pair
  00h  5Dh   Receive Command Acknowledge 2
  00h  MSB   Receive Confirmed Address MSB
  00h  LSB   Receive Confirmed Address LSB
  00h  ...   Receive Data Sector (128 bytes)
  00h  CHK   Receive Checksum (MSB xor LSB xor Data bytes)
  00h  47h   Receive Memory End Byte (should be always 47h="G"=Good for Read)
*/
void ps1_mcd_cmd_read(struct ps2_sio2* sio2, struct ps1_mcd_state* mcd) {
    uint16_t msb = queue_at(sio2->in, 4);
    uint16_t lsb = queue_at(sio2->in, 5);

    uint16_t addr = ((msb << 8) | lsb) * 128;

    printf("ps1_mcd: ps1_mcd_cmd_read(%04x)\n", (msb << 8) | lsb);

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, mcd->flag);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x5d);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, msb); // (pre)
    queue_push(sio2->out, 0x5c);
    queue_push(sio2->out, 0x5d);
    queue_push(sio2->out, msb);
    queue_push(sio2->out, lsb);

    uint8_t checksum = msb ^ lsb;

    for (int i = 0; i < 128; i++) {
        checksum ^= mcd->buf[addr + i];

        queue_push(sio2->out, mcd->buf[addr + i]);
    }

    queue_push(sio2->out, checksum);
    queue_push(sio2->out, 'G'); // 0x47
}

/*
  Send Reply Comment
  81h  N/A   Memory card address
  57h  FLAG  Send Write Command (ASCII "W"), Receive FLAG Byte
  00h  5Ah   Receive Memory Card ID1
  00h  5Dh   Receive Memory Card ID2
  MSB  (00h) Send Address MSB  ;\sector number (0..3FFh)
  LSB  (pre) Send Address LSB  ;/
  ...  (pre) Send Data Sector (128 bytes)
  CHK  (pre) Send Checksum (MSB xor LSB xor Data bytes)
  00h  5Ch   Receive Command Acknowledge 1
  00h  5Dh   Receive Command Acknowledge 2
  00h  4xh   Receive Memory End Byte (47h=Good, 4Eh=BadChecksum, FFh=BadSector)
*/
void ps1_mcd_cmd_write(struct ps2_sio2* sio2, struct ps1_mcd_state* mcd) {
    uint16_t msb = queue_at(sio2->in, 4);
    uint16_t lsb = queue_at(sio2->in, 5);

    uint16_t addr = ((msb << 8) | lsb) * 128;

    printf("ps1_mcd: ps1_mcd_cmd_write(%04x)\n", (msb << 8) | lsb);

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, mcd->flag);
    queue_push(sio2->out, 0x5a);
    queue_push(sio2->out, 0x5d);
    queue_push(sio2->out, 0x00);
    queue_push(sio2->out, msb); // (pre)

    for (int i = 0; i < 128; i++) {
        mcd->buf[addr+i] = queue_at(sio2->in, 6+i);

        queue_push(sio2->out, queue_at(sio2->in, 5+i)); // (pre)
    }

    ps1_mcd_flush_block(mcd, addr);

    queue_push(sio2->out, queue_at(sio2->in, 133)); // (pre)
    queue_push(sio2->out, 0x5c);
    queue_push(sio2->out, 0x5d);
    queue_push(sio2->out, 'G');

    // Reset directory read flag
    mcd->flag &= ~0x08;
}
void ps1_mcd_cmd_get_id(struct ps2_sio2* sio2, struct ps1_mcd_state* mcd) {
    printf("ps1_mcd: ps1_mcd_cmd_get_id\n");

    exit(1);
}
void ps1_mcd_cmd_invalid(struct ps2_sio2* sio2, struct ps1_mcd_state* mcd) {
    printf("ps1_mcd: ps1_mcd_cmd_invalid(%02x)\n", queue_size(sio2->in));

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, mcd->flag);

    for (int i = 2; i < queue_size(sio2->in); i++)
        queue_push(sio2->out, 0xff);
}
void ps1_mcd_cmd_detect_pocketstation(struct ps2_sio2* sio2, struct ps1_mcd_state* mcd) {
    printf("ps1_mcd: ps1_mcd_cmd_detect_pocketstation\n");

    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, mcd->flag);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
    queue_push(sio2->out, 0xff);
}

void ps1_mcd_handle_command(struct ps2_sio2* sio2, void* udata, int cmd) {
    struct ps1_mcd_state* mcd = (struct ps1_mcd_state*)udata;

    switch (cmd) {
        case 0x52: ps1_mcd_cmd_read(sio2, mcd); return;
        case 0x53: ps1_mcd_cmd_get_id(sio2, mcd); return;
        case 0x57: ps1_mcd_cmd_write(sio2, mcd); return;
        // case 0x58: ps1_mcd_cmd_detect_pocketstation(sio2, mcd); return;
    }

    sio2->recv1 |= 0x2000;

    ps1_mcd_cmd_invalid(sio2, mcd);
}

struct ps1_mcd_state* ps1_mcd_attach(struct ps2_sio2* sio2, int port, const char* path) {
    FILE* file = fopen(path, "r+b");

    if (!file)
        return NULL;

    struct ps1_mcd_state* mcd = malloc(sizeof(struct ps1_mcd_state));
    struct sio2_device dev;

    memset(mcd, 0, sizeof(struct ps1_mcd_state));

    mcd->file = file;
    mcd->flag = 0x08;

    fread(mcd->buf, 1, PS1_MCD_SIZE, file);

    printf("ps1_mcd: Memory card at \'%s\' initialized.\n",
        path
    );

    dev.detach = ps1_mcd_detach;
    dev.handle_command = ps1_mcd_handle_command;
    dev.udata = mcd;

    ps2_sio2_attach_device(sio2, dev, port);

    return mcd;
}

void ps1_mcd_detach(void* udata) {
    struct ps1_mcd_state* mcd = (struct ps1_mcd_state*)udata;

    // Flush buffer back to file
    fseek(mcd->file, 0, SEEK_SET);
    fwrite(mcd->buf, 1, PS1_MCD_SIZE, mcd->file);

    fclose(mcd->file);
    free(mcd);
}