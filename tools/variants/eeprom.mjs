// Flash reservations are physical addresses. IAP register addresses are a
// separate, chip-specific interface and must not be inferred from these macros.
export function eepromConfigurations(device) {
  const layout = device.eeprom_layout;
  const floor = device.linker.flash_loc ?? (0x1000000 - device.flash_bytes);
  const end = floor + device.flash_bytes;
  if (!layout || !["prefix", "suffix", "separate"].includes(layout.allocation) ||
      layout.sector_bytes !== 512) {
    throw new Error(`invalid EEPROM layout for ${device.model}`);
  }
  if (layout.allocation === "separate") {
    if (!Number.isInteger(layout.flash_start) || !Number.isInteger(layout.fixed_bytes) ||
        layout.fixed_bytes <= 0 || layout.fixed_bytes % layout.sector_bytes ||
        layout.flash_start % layout.sector_bytes || layout.flash_start < 0 ||
        layout.flash_start + layout.fixed_bytes > 0x1000000 ||
        (layout.flash_start < end && floor < layout.flash_start + layout.fixed_bytes) ||
        layout.reserve_options_bytes !== undefined) {
      throw new Error(`EEPROM overlaps program Flash for ${device.model}`);
    }
    return [configuration("fixed", layout.fixed_bytes, layout.flash_start, floor, end)];
  }
  const sizes = layout.reserve_options_bytes;
  if (!Array.isArray(sizes) || sizes[0] !== 0 || sizes.length < 2 ||
      sizes.some((size, i) => !Number.isInteger(size) || size < 0 ||
        size % layout.sector_bytes || (i > 0 && size <= sizes[i - 1])) ||
      layout.flash_start !== undefined || layout.fixed_bytes !== undefined) {
    throw new Error(`invalid EEPROM reservation sizes for ${device.model}`);
  }
  return sizes.map(size => {
    const start = layout.allocation === "prefix" ? floor + size : floor;
    const limit = layout.allocation === "suffix" ? end - size : end;
    if ((start >= device.linker.code_loc && layout.allocation === "prefix") ||
        limit <= device.linker.code_loc ||
        limit - start < 16384 ||
        (layout.allocation === "prefix" && device.linker.layout !== "segmented_home") ||
        (layout.allocation === "suffix" && device.linker.layout !== "post_home_contiguous")) {
      throw new Error(`EEPROM reservation covers reset/startup for ${device.model}`);
    }
    const id = size === 0 ? "none" : size % 1024 ? `b${size}` : `k${size / 1024}`;
    return configuration(id, size, layout.allocation === "prefix" ? floor : limit, start, limit);
  });

  function configuration(id, size, flashStart, codeStart, codeEnd) {
    return {
      id, size, flashStart, codeStart, codeEnd,
      maximumCodeBytes: device.maximum_code_bytes -
        (layout.allocation === "separate" ? 0 : size),
      flags: `-DSTC_EEPROM_SIZE=${size}UL -DSTC_EEPROM_FLASH_START=0x${flashStart.toString(16)}UL -DSTC_EEPROM_SECTOR_SIZE=${layout.sector_bytes}UL`,
    };
  }
}
