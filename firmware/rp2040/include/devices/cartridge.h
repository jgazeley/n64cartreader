#ifndef DEVICES_CARTRIDGE_H_
#define DEVICES_CARTRIDGE_H_

#define N64_TITLE_OFFSET 0x20
#define N64_TITLE_LENGTH 20
#define N64_HEADER_LENGTH 64

#ifdef __cplusplus
extern "C" {
#endif
bool n64_read_bytes(uint32_t base_addr, uint8_t *buf, size_t len);
bool n64_read_bytes_fast(uint32_t base_addr, uint8_t *buf, size_t len);
bool n64_get_header(uint8_t* buffer, size_t buffer_size);
bool n64_get_title(uint8_t* buffer, size_t buffer_size);
// bool n64_rom_dump     (uint32_t offset, void *dst, size_t len);
// bool n64_sram_read    (uint32_t offset, void *dst, size_t len);
// bool n64_sram_write   (uint32_t offset, const void *src, size_t len);
// bool n64_flash_read   (uint32_t offset, void *dst, size_t len);
// bool n64_flash_write  (uint32_t offset, const void *src, size_t len);
bool n64_eeprom_read  (uint16_t addr, uint8_t *dst, size_t len);
// bool n64_eeprom_write (uint16_t addr, const uint8_t *src, size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* DEVICES_CARTRIDGE_H_ */