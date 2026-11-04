import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { eepromConfigurations } from "./eeprom.mjs";

const devices = JSON.parse(readFileSync(new URL("./devices.json", import.meta.url))).devices;

test("all ten devices have disjoint program and EEPROM ranges", () => {
  assert.equal(devices.length, 10);
  for (const device of devices) {
    for (const option of eepromConfigurations(device)) {
      assert.ok(option.codeStart <= 0xff0000 && option.codeEnd > 0xff0000);
      assert.ok(option.flashStart + option.size <= option.codeStart ||
        option.flashStart >= option.codeEnd);
      assert.equal(option.codeEnd - option.codeStart, option.maximumCodeBytes);
    }
  }
});

test("2 KiB reservations retain HOME and use the correct side of Flash", () => {
  for (const [id, expected] of [
    ["stc32g12k128", [0xfe0800, 0x1000000, 129024, 0xfe0000]],
    ["stc32g144k246", [0xfc3000, 0x1000000, 249856, 0xfc2800]],
    ["stc32g8k64", [0xff0000, 0xfff800, 63488, 0xfff800]],
    ["stc32cl8k64", [0xff0000, 0xfff800, 63488, 0xfff800]],
    ["ai8051u_34k64", [0xff0000, 0xfff800, 63488, 0xfff800]],
  ]) {
    const option = eepromConfigurations(devices.find(d => d.id === id)).find(o => o.id === "k2");
    assert.deepEqual([option.codeStart, option.codeEnd, option.maximumCodeBytes, option.flashStart], expected);
  }
});

test("invalid reservations fail before generating unsafe board properties", () => {
  const prefix = devices.find(d => d.id === "stc32g12k128");
  const separate = devices.find(d => d.id === "stc32g8k48");
  for (const layout of [
    { ...prefix.eeprom_layout, reserve_options_bytes: [0, 513] },
    { ...prefix.eeprom_layout, reserve_options_bytes: [0, 2048, 1024] },
    { ...prefix.eeprom_layout, reserve_options_bytes: [0, 65536] },
    { ...prefix.eeprom_layout, sector_bytes: 0 },
  ]) {
    assert.throws(() => eepromConfigurations({ ...prefix, eeprom_layout: layout }));
  }
  assert.throws(() => eepromConfigurations({ ...separate,
    eeprom_layout: { ...separate.eeprom_layout, flash_start: 0xffb000 } }));
});
