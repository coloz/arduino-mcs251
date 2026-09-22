//! An ELF32 load image for recipe runners with a fixed .elf link target.
//! It contains the actual MCS251 flash bytes, not a sentinel or renamed HEX.
use anyhow::{Context, Result, bail, ensure};
use std::collections::BTreeMap;

fn u16_at(b: &[u8], i: usize) -> Result<u16> {
    Ok(u16::from_le_bytes(
        b.get(i..i + 2).context("truncated ELF")?.try_into()?,
    ))
}
fn u32_at(b: &[u8], i: usize) -> Result<u32> {
    Ok(u32::from_le_bytes(
        b.get(i..i + 4).context("truncated ELF")?.try_into()?,
    ))
}
fn put16(b: &mut [u8], i: usize, n: u16) {
    b[i..i + 2].copy_from_slice(&n.to_le_bytes());
}
fn put32(b: &mut [u8], i: usize, n: u32) {
    b[i..i + 4].copy_from_slice(&n.to_le_bytes());
}

pub fn from_hex(hex: &[u8]) -> Result<Vec<u8>> {
    let mut image = BTreeMap::new();
    let mut base = 0u32;
    let mut eof = false;
    let mut entry = None;
    for line in std::str::from_utf8(hex)?
        .lines()
        .filter(|l| !l.trim().is_empty())
    {
        let line = line.trim();
        ensure!(
            !eof && line.starts_with(':') && line.len() % 2 == 1,
            "invalid HEX record"
        );
        let r = (1..line.len())
            .step_by(2)
            .map(|i| u8::from_str_radix(&line[i..i + 2], 16))
            .collect::<std::result::Result<Vec<_>, _>>()?;
        ensure!(
            r.len() >= 5 && r.len() == r[0] as usize + 5,
            "invalid HEX length"
        );
        ensure!(
            r.iter().fold(0u8, |s, n| s.wrapping_add(*n)) == 0,
            "HEX checksum mismatch"
        );
        let addr = u16::from_be_bytes([r[1], r[2]]) as u32;
        let data = &r[4..r.len() - 1];
        match r[3] {
            0 => {
                for (i, byte) in data.iter().enumerate() {
                    let a = base
                        .checked_add(addr)
                        .and_then(|a| a.checked_add(i as u32))
                        .context("HEX address overflow")?;
                    ensure!(a <= 0xffffff, "flash address exceeds MCS251 address space");
                    if let Some(previous) = image.insert(a, *byte) {
                        ensure!(previous == *byte, "conflicting HEX data");
                    }
                }
            }
            1 => {
                ensure!(data.is_empty() && addr == 0, "invalid HEX EOF");
                eof = true;
            }
            2 | 4 => {
                ensure!(data.len() == 2 && addr == 0, "invalid HEX base address");
                base =
                    (u16::from_be_bytes(data.try_into()?) as u32) << if r[3] == 2 { 4 } else { 16 };
            }
            3 | 5 => {
                ensure!(data.len() == 4 && addr == 0, "invalid HEX entry point");
                entry = Some(if r[3] == 5 {
                    u32::from_be_bytes(data.try_into()?)
                } else {
                    ((u16::from_be_bytes(data[..2].try_into()?) as u32) << 4)
                        + u16::from_be_bytes(data[2..].try_into()?) as u32
                });
            }
            _ => bail!("unsupported HEX record"),
        }
    }
    ensure!(eof && !image.is_empty(), "missing HEX data or EOF");
    let mut segments: Vec<(u32, Vec<u8>)> = Vec::new();
    for (address, byte) in image {
        if let Some((start, bytes)) = segments.last_mut()
            && *start + bytes.len() as u32 == address
        {
            bytes.push(byte);
        } else {
            segments.push((address, vec![byte]));
        }
    }
    ensure!(segments.len() < 0xffff, "too many ELF load segments");
    let mut result = vec![0u8; 52 + segments.len() * 32];
    result[..7].copy_from_slice(b"\x7fELF\x01\x01\x01");
    put16(&mut result, 16, 2); // ET_EXEC
    put16(&mut result, 18, 165); // EM_8051: Intel 8051 and variants
    put32(&mut result, 20, 1);
    put32(&mut result, 24, entry.unwrap_or(segments[0].0));
    put32(&mut result, 28, 52);
    put16(&mut result, 40, 52);
    put16(&mut result, 42, 32);
    put16(&mut result, 44, segments.len() as u16);
    for (i, (address, bytes)) in segments.into_iter().enumerate() {
        let p = 52 + i * 32;
        let offset = result.len() as u32;
        put32(&mut result, p, 1); // PT_LOAD
        put32(&mut result, p + 4, offset);
        put32(&mut result, p + 8, address);
        put32(&mut result, p + 12, address);
        put32(&mut result, p + 16, bytes.len() as u32);
        put32(&mut result, p + 20, bytes.len() as u32);
        put32(&mut result, p + 24, 5); // read, execute
        put32(&mut result, p + 28, 1);
        result.extend(bytes);
    }
    Ok(result)
}

fn record(out: &mut String, kind: u8, address: u16, data: &[u8]) {
    use std::fmt::Write;
    let mut bytes = vec![data.len() as u8, (address >> 8) as u8, address as u8, kind];
    bytes.extend_from_slice(data);
    bytes.push(0u8.wrapping_sub(bytes.iter().fold(0u8, |s, n| s.wrapping_add(*n))));
    out.push(':');
    for byte in bytes {
        write!(out, "{byte:02X}").unwrap();
    }
    out.push('\n');
}

pub fn to_hex(elf: &[u8]) -> Result<Vec<u8>> {
    ensure!(
        elf.starts_with(b"\x7fELF\x01\x01\x01")
            && u16_at(elf, 16)? == 2
            && u16_at(elf, 18)? == 165
            && u16_at(elf, 42)? == 32,
        "expected STC ELF32 load image"
    );
    let mut image = BTreeMap::new();
    let phoff = u32_at(elf, 28)? as usize;
    for i in 0..u16_at(elf, 44)? as usize {
        let p = phoff.checked_add(i * 32).context("ELF offset overflow")?;
        ensure!(u32_at(elf, p)? == 1, "unexpected ELF segment type");
        let start = u32_at(elf, p + 4)? as usize;
        let addr = u32_at(elf, p + 12)?;
        let size = u32_at(elf, p + 16)? as usize;
        ensure!(
            size <= u32_at(elf, p + 20)? as usize
                && size <= 0x1000000
                && (addr as u64 + size as u64) <= 0x1000000,
            "invalid ELF flash segment"
        );
        let bytes = elf
            .get(start..start.checked_add(size).context("ELF offset overflow")?)
            .context("truncated ELF segment")?;
        for (j, byte) in bytes.iter().enumerate() {
            ensure!(
                image.insert(addr + j as u32, *byte).is_none(),
                "overlapping ELF segments"
            );
        }
    }
    ensure!(!image.is_empty(), "empty ELF image");
    let mut out = String::new();
    let mut high = None;
    let mut iter = image.into_iter().peekable();
    while let Some((addr, byte)) = iter.next() {
        if high != Some(addr >> 16) {
            high = Some(addr >> 16);
            record(&mut out, 4, 0, &((addr >> 16) as u16).to_be_bytes());
        }
        let mut data = vec![byte];
        while data.len() < 16
            && iter
                .peek()
                .is_some_and(|(a, _)| *a == addr + data.len() as u32 && *a >> 16 == addr >> 16)
        {
            data.push(iter.next().unwrap().1);
        }
        record(&mut out, 0, addr as u16, &data);
    }
    record(&mut out, 1, 0, &[]);
    Ok(out.into_bytes())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn load_image_roundtrip_preserves_sparse_24_bit_flash() {
        let mut hex = String::new();
        record(&mut hex, 0, 0, &[2, 0, 32]);
        record(&mut hex, 4, 0, &[0, 8]);
        record(&mut hex, 0, 0xfffe, &[0x55, 0xaa]);
        record(&mut hex, 4, 0, &[0, 9]);
        record(&mut hex, 0, 0, &[1, 2, 3]);
        record(&mut hex, 1, 0, &[]);
        let elf = from_hex(hex.as_bytes()).unwrap();
        assert_eq!(from_hex(&to_hex(&elf).unwrap()).unwrap(), elf);
        assert!(to_hex(&elf[..elf.len() - 1]).is_err());
        assert!(from_hex(b":0300000002002000\n:00000001FF\n").is_err());
        assert!(from_hex(b":00000001FF\n").is_err());
    }
}
