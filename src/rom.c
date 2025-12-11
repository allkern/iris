#include "rom.h"
#include "md5.h"

static const struct ps2_rom_info rom0_info_table[] = {
    { "32f2e4d5ff5ee11072a6bc45530f5765", "5.0 01/17/00 T", "N/A", "DTL-H10000", 4 },
    { "acf4730ceb38ac9d8c7d8e21f2614600", "5.0 01/17/00 T", "NTSC-J", "SCPH-10000", 1 },
    { "acf9968c8f596d2b15f42272082513d1", "5.0 02/17/00 T", "N/A", "DTL-H10000", 4 },
    { "b1459d7446c69e3e97e6ace3ae23dd1c", "5.0 02/17/00 T", "NTSC-J", "SCPH-10000/SCPH-15000", 1 },
    { "d3f1853a16c2ec18f3cd1ae655213308", "5.0 02/24/00 T", "N/A", "DTL-H10000", 4 },
    { "63e6fd9b3c72e0d7b920e80cf76645cd", "5.0 07/27/00 A", "N/A", "DTL-H30001", 4 },
    { "a20c97c02210f16678ca3010127caf36", "5.0 07/27/00 A", "NTSC-U/C", "SCPH-30001", 1 },
    { "a925e84ac0ed2711af36e28772793be2", "5.0 09/01/00 ?", "N/A", "COH-H31000 (Konami Python)", 6 },
    { "8db2fbbac7413bf3e7154c1e0715e565", "5.0 09/02/00 A", "NTSC-U/C", "SCPH-30001", 1 },
    { "91c87cb2f2eb6ce529a2360f80ce2457", "5.0 09/02/00 E", "N/A", "DTL-H30002", 4 },
    { "3016b3dd42148a67e2c048595ca4d7ce", "5.0 09/02/00 E", "N/A", "DTL-H30102", 4 },
    { "b7fa11e87d51752a98b38e3e691cbf17", "5.0 09/02/00 E", "PAL-E", "SCPH-30002/SCPH-30003/SCPH-30004", 1 },
    { "f63bc530bd7ad7c026fcd6f7bd0d9525", "5.0 10/27/00 J", "NTSC-J", "SCPH-18000 (GH-003)", 1 },
    { "cee06bd68c333fc5768244eae77e4495", "5.0 10/27/00 J", "NTSC-J", "SCPH-18000 (GH-008)", 1 },
    { "0bf988e9c7aaa4c051805b0fa6eb3387", "5.0 12/28/00 A", "N/A", "DTL-H30101", 4 },
    { "8accc3c49ac45f5ae2c5db0adc854633", "5.0 12/28/00 A", "NTSC-U/C", "SCPH-30001/SCPH-35001", 1 },
    { "6f9a6feb749f0533aaae2cc45090b0ed", "5.0 12/28/00 E", "N/A", "DTL-H30102", 4 },
    { "838544f12de9b0abc90811279ee223c8", "5.0 12/28/00 E", "PAL-E", "SCPH-30002/SCPH-30003/SCPH-30004/SCPH-35002/SCPH-35003/SCPH-35004", 1 },
    { "bb6bbc850458fff08af30e969ffd0175", "5.0 01/18/01 J", "N/A", "DTL-H30000", 4 },
    { "815ac991d8bc3b364696bead3457de7d", "5.0 01/18/01 J", "NTSC-J", "SCPH-30000/SCPH-35000", 1 },
    { "b107b5710042abe887c0f6175f6e94bb", "5.0 04/27/01 A", "NTSC-U/C", "SCPH-30001R", 1 },
    { "ab55cceea548303c22c72570cfd4dd71", "5.0 04/27/01 J", "NTSC-J", "SCPH-30000", 1 },
    { "18bcaadb9ff74ed3add26cdf709fff2e", "5.0 07/04/01 A", "NTSC-U/C", "SCPH-30001R", 1 },
    { "491209dd815ceee9de02dbbc408c06d6", "5.0 07/04/01 E", "PAL-E", "SCPH-30002R/SCPH-30003R/SCPH-30004R", 1 },
    { "7200a03d51cacc4c14fcdfdbc4898431", "5.0 10/04/01 A", "NTSC-U/C", "SCPH-30001R", 1 },
    { "8359638e857c8bc18c3c18ac17d9cc3c", "5.0 10/04/01 E", "PAL-E", "SCPH-30002R/SCPH-30003R/SCPH-30004R", 1 },
    { "28922c703cc7d2cf856f177f2985b3a9", "5.0 10/04/01 E", "PAL-E", "SCPH-30002R/SCPH-30003R/SCPH-30004R", 1 },
    { "352d2ff9b3f68be7e6fa7e6dd8389346", "5.0 07/30/01 J", "NTSC-J", "SCPH-30005R/SCPH-30006R/SCPH-30007R", 1 },
    { "d5ce2c7d119f563ce04bc04dbc3a323e", "5.0 02/07/02 A", "NTSC-U", "SCPH-39001", 1 },
    { "0d2228e6fd4fb639c9c39d077a9ec10c", "5.0 03/19/02 E", "PAL-E", "SCPH-39002/SCPH-39003/SCPH-39004", 1 },
    { "72da56fccb8fcd77bba16d1b6f479914", "5.0 04/26/02 J", "NTSC-J", "SCPH-37000/SCPH-39000", 1 },
    { "5b1f47fbeb277c6be2fccdd6344ff2fd", "5.0 04/26/02 E", "PAL-E", "SCPH-39008", 1 },
    { "315a4003535dfda689752cb25f24785c", "5.0 04/26/02 J", "NTSC-J", "SCPH-39005/SCPH-39006/SCPH-39007", 1 },
    { "52cca0058626569c7a9699838baab2d8", "5.0 11/19/02 ?", "N/A", "Namco System 246", 10 },
    { "312ad4816c232a9606e56f946bc0678a", "5.0 02/06/03 J", "NTSC-J", "SCPH-50000/SCPH-55000", 2 },
    { "666018ffec65c5c7e04796081295c6c7", "5.0 02/27/03 E", "N/A", "DTL-H50002", 4 },
    { "6e69920fa6eef8522a1d688a11e41bc6", "5.0 02/27/03 E", "PAL-E", "SCPH-50002/SCPH-50003/SCPH-50004", 2 },
    { "eb960de68f0c0f7f9fa083e9f79d0360", "5.0 03/25/03 A", "N/A", "DTL-H50001", 4 },
    { "8aa12ce243210128c5074552d3b86251", "5.0 03/25/03 A", "NTSC-U/C", "SCPH-50001", 2 },
    { "240d4c5ddd4b54069bdc4a3cd2faf99d", "5.0 02/24/03 J", "N/A", "DTL-H50009", 4 },
    { "1c6cd089e6c83da618fbf2a081eb4888", "5.0 10/28/03 J", "NTSC-J", "DESR-5000/DESR-5100/DESR-7000/DESR-7100 (PSX1)", 3 },
    { "1999f40e409756522f9e70fea308a020", "5.0 10/31/03 T", "N/A", "DTL-T10000", 5 },
    { "463d87789c555a4a7604e97d7db545d1", "5.0 06/23/03 J", "NTSC-J", "SCPH-55000", 2 },
    { "35461cecaa51712b300b2d6798825048", "5.0 06/23/03 A", "NTSC-U/C", "SCPH-50001/SCPH-50010", 2 },
    { "bd6415094e1ce9e05daabe85de807666", "5.0 06/23/03 E", "PAL-E", "SCPH-50002/SCPH-50003/SCPH-50004", 2 },
    { "2e70ad008d4ec8549aada8002fdf42fb", "5.0 06/23/03 J", "NTSC-J", "SCPH-50005/SCPH-50006/SCPH-50007", 2 },
    { "b53d51edc7fc086685e31b811dc32aad", "5.0 06/23/03 E", "PAL-E", "SCPH-50008", 2 },
    { "1b6e631b536247756287b916f9396872", "5.0 06/23/03 J", "NTSC-J", "SCPH-50009", 2 },
    { "00da1b177096cfd2532c8fa22b43e667", "5.0 08/22/03 J", "NTSC-J", "SCPH-50000/Konami Python 2", 2 },
    { "afde410bd026c16be605a1ae4bd651fd", "5.0 08/22/03 E", "PAL-E", "SCPH-50004", 2 },
    { "81f4336c1de607dd0865011c0447052e", "5.0 03/29/04 A", "NTSC-U/C", "SCPH-50011", 2 },
    { "a58676c6bd79229bda967d07b4ec2e16", "5.0 05/19/04 ?", "N/A", "Namco System 256", 11 },
    { "0eee5d1c779aa50e94edd168b4ebf42e", "5.0 06/14/04 J", "NTSC-J", "SCPH-70000", 2 },
    { "d333558cc14561c1fdc334c75d5f37b7", "5.0 06/14/04 A", "NTSC-U/C", "SCPH-70001/SCPH-70011/SCPH-70012", 2 },
    { "dc752f160044f2ed5fc1f4964db2a095", "5.0 06/14/04 E", "PAL-E", "SCPH-70002/SCPH-70003/SCPH-70004/SCPH-70008", 2 },
    { "63ead1d74893bf7f36880af81f68a82d", "5.0 06/14/04 E", "N/A", "DTL-H70002", 4 },
    { "3e3e030c0f600442fa05b94f87a1e238", "5.0 06/14/04 J", "NTSC-J", "SCPH-70005/SCPH-70006/SCPH-70007", 2 },
    { "1ad977bb539fc9448a08ab276a836bbc", "5.0 09/17/04 J", "NTSC-J", "DESR-5500/DESR-5700/DESR-7500/DESR-7700 (PSX2)", 3 },
    { "eb4f40fcf4911ede39c1bbfe91e7a89a", "5.0 06/20/05 J", "NTSC-J", "SCPH-75000", 2 },
    { "9959ad7a8685cad66206e7752ca23f8b", "5.0 06/20/05 A", "N/A", "DTL-H75000A", 4 },
    { "929a14baca1776b00869f983aa6e14d2", "5.0 06/20/05 A", "NTSC-U/C", "SCPH-75001/SCPH-75010", 2 },
    { "573f7d4a430c32b3cc0fd0c41e104bbd", "5.0 06/20/05 E", "PAL-E", "SCPH-75002/SCPH-75003/SCPH-75004/SCPH-75008", 2 },
    { "df63a604e8bff5b0599bd1a6c2721bd0", "5.0 06/20/05 J", "NTSC-J", "SCPH-75006", 2 },
    { "5b1ba4bb914406fae75ab8e38901684d", "5.0 02/10/06 J", "NTSC-J", "SCPH-77000", 2 },
    { "cb801b7920a7d536ba07b6534d2433ca", "5.0 02/10/06 A", "NTSC-U/C", "SCPH-77001/SCPH-77010", 2 },
    { "af60e6d1a939019d55e5b330d24b1c25", "5.0 02/10/06 E", "PAL-E", "SCPH-77002/SCPH-77003/SCPH-77004/SCPH-77008", 2 },
    { "549a66d0c698635ca9fa3ab012da7129", "5.0 02/10/06 J", "NTSC-J", "SCPH-77006/SCPH-77007", 2 },
    { "5de9d0d730ff1e7ad122806335332524", "5.0 09/05/06 J", "NTSC-J", "SCPH-79000/SCPH-90000", 2 },
    { "21fe4cad111f7dc0f9af29477057f88d", "5.0 09/05/06 A", "N/A", "DTL-H90000", 4 },
    { "40c11c063b3b9409aa5e4058e984e30c", "5.0 09/05/06 A", "NTSC-U/C", "SCPH-79001/SCPH-79010/SCPH-90001", 2 },
    { "80bbb237a6af9c611df43b16b930b683", "5.0 09/05/06 E", "PAL-E", "SCPH-79002/SCPH-79003/SCPH-79004/SCPH-79008/SCPH-90002/SCPH-90003/SCPH-90004", 2 },
    { "c37bce95d32b2be480f87dd32704e664", "5.0 09/05/06 J", "NTSC-J", "SCPH-79006/SCPH-79007/SCPH-90006/SCPH-90007", 2 },
    { "80ac46fa7e77b8ab4366e86948e54f83", "5.0 02/20/08 J", "NTSC-J", "SCPH-90000", 2 },
    { "21038400dc633070a78ad53090c53017", "5.0 02/20/08 A", "NTSC-U/C", "SCPH-90001/SCPH-90010", 2 },
    { "dc69f0643a3030aaa4797501b483d6c4", "5.0 02/20/08 E", "PAL-E", "SCPH-90002/SCPH-90003/SCPH-90004/SCPH-90008", 2 },
    { "30d56e79d89fbddf10938fa67fe3f34e", "5.0 02/20/08 J", "NTSC-J", "SCPH-90005/SCPH-90006/SCPH-90007", 2 },
    { "93ea3bcee4252627919175ff1b16a1d9", "5.0 04/15/10 E", "PAL-E", "KDL-22PX300 (Sony Bravia TV) (Europe)", 2 },
    { "d3e81e95db25f5a86a7b7474550a2155", "5.0 04/15/10 J", "NTSC-J", "KDL-22PX300 (Sony Bravia TV)", 2 },
    { "cc4b9cea0fdb3d2506173668a2a88305", "?.? \?\?/\?\?/?? ?", "N/A", "Namco System 147", 8 },
    { "860c13259a548c7ff07b67157928b076", "?.? \?\?/\?\?/?? ?", "N/A", "Namco System 148", 9 }
};

static const struct ps2_rom_info rom1_info_table[] = {
    { "31a671627b9bf2d88f5e3f6680941fa6", "1.10U" },
    { "22080eed26576f4e2282c905dc6e0a4b", "1.20E" },
    { "20a4e401b9e7885e25f1c31f6bfcbe0c", "1.20U" },
    { "a1a15b62cef142575faaea17fb23dbd1", "1.30E" },
    { "567fe068711a9e5914e836cb650600af", "1.30U" },
    { "8afc4544e572842a6bb301ad92dc0c02", "2.00J" },
    { "d5118e3979eb2a3814ebbfa32825e7b0", "2.10E" },
    { "f0b2e6b7f6d06561e230a5b85e5d1bf9", "2.10J" },
    { "d0f79251699fdeff073a7c2365d0c526", "2.10U" },
    { "32abbe7ab7c1b72d5ffc24d4963bd6c6", "2.12G" },
    { "4499f6303d05d4caeb289c2344ea3469", "2.12U" },
    { "6bdb45a952f697f367cd1646cdf09235", "2.13E" },
    { "d609f69d9e3ef236f6e2bf0a80762b6f", "2.15G" },
    { "f40466438d83a02c1eecba3efff20b6a", "3.00E" },
    { "e414c981647883e33f17642da827a739", "3.00U" },
    { "6bbd2f348c585fdd645900f6ec75f2c7", "3.02C" },
    { "503115717429b64e19fa6103e3fa5a35", "3.02E" },
    { "2ac40eec790adecf4bc5ee1090a27676", "3.02U" },
    { "a2b55c44a3c3eec0abe5647cfb6a6493", "3.10" },
    { "79b4880006769a1af9b0a6c7302cc18d", "3.11" }
};

static const struct ps2_rom_info unknown = {
    "00000000000000000000000000000000",
    "Unknown",
    "Unknown",
    "Unknown",
    2
};

struct ps2_rom_info ps2_rom0_search(uint8_t* rom, size_t size) {
    struct md5_context ctx;
    char buf[33];

    md5_init(&ctx);
    md5_update(&ctx, rom, size);
    md5_finalize(&ctx);

    for (int i = 0; i < 16; i++) {
        sprintf(&buf[i * 2], "%02x", ctx.digest[i]);
    }

    for (size_t i = 0; i < sizeof(rom0_info_table) / sizeof(rom0_info_table[0]); i++) {
        if (strncmp(buf, rom0_info_table[i].md5hash, 32) == 0) {
            return rom0_info_table[i];
        }
    }

    struct ps2_rom_info info;
    
    info = unknown;

    memcpy(info.md5hash, buf, 33);

    return info;
}

struct ps2_rom_info ps2_rom1_search(uint8_t* rom, size_t size) {
    struct md5_context ctx;
    char buf[33];

    md5_init(&ctx);
    md5_update(&ctx, rom, size);
    md5_finalize(&ctx);

    for (int i = 0; i < 16; i++) {
        sprintf(&buf[i * 2], "%02x", ctx.digest[i]);
    }

    for (size_t i = 0; i < sizeof(rom1_info_table) / sizeof(rom1_info_table[0]); i++) {
        if (strncmp(buf, rom1_info_table[i].md5hash, 32) == 0) {
            return rom1_info_table[i];
        }
    }

    struct ps2_rom_info info;
    
    info = unknown;

    memcpy(info.md5hash, buf, 33);

    return info;
}