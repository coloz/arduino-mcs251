//! Arduino's dynamic-memory summary covers the external data pool only.
use anyhow::{Context, Result, ensure};
use std::{collections::BTreeMap, fs, path::Path};

fn sketch_size(report: &str) -> Result<(u64, u64)> {
    let mut regions = BTreeMap::new();
    for line in report.lines() {
        let line = line.trim();
        for name in ["ROM/EPROM/FLASH", "EXTERNAL RAM", "PAGED EXT. RAM"] {
            let Some(row) = line.strip_prefix(name) else {
                continue;
            };
            // Empty regions omit the start/end addresses in SDCC's .mem file.
            let fields: Vec<_> = row.split_whitespace().collect();
            ensure!(
                fields.len() == 2
                    || (fields.len() == 4
                        && fields[0].starts_with("0x")
                        && fields[1].starts_with("0x")),
                "invalid {name} memory report row"
            );
            let used: u64 = fields[fields.len() - 2].parse()?;
            let maximum: u64 = fields[fields.len() - 1].parse()?;
            ensure!(
                used <= maximum,
                "{name} exceeds available memory: {used} / {maximum} bytes"
            );
            ensure!(
                regions.insert(name, used).is_none(),
                "duplicate {name} memory report row"
            );
        }
    }
    let program = *regions
        .get("ROM/EPROM/FLASH")
        .context("missing Flash usage")?;
    let xdata = *regions.get("EXTERNAL RAM").context("missing XDATA usage")?;
    let pdata = regions.get("PAGED EXT. RAM").copied().unwrap_or(0);
    // Heap storage is already part of XDATA. DATA/IDATA and the EDATA stack
    // have separate limits and must not be charged against the XDATA budget.
    let ram = xdata.checked_add(pdata).context("RAM usage overflow")?;
    Ok((program, ram))
}

pub fn size(path: &Path) -> Result<()> {
    let (program, ram) = sketch_size(&fs::read_to_string(path)?)?;
    println!("STC_PROGRAM_BYTES {program}\nSTC_RAM_BYTES {ram}");
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    const REPORT: &str = "Internal RAM layout:
0x00:|0|0|0|0|0|0|0|0|a|a|a|a|a|a| | |
0x20:|T| | | | | | | | | | | | | | | |
Stack starts at: 0x0100 (spx set to 0x00ff) with 16128 bytes available.
Other memory:
   Name             Start    End      Size     Max
   PAGED EXT. RAM                         0      256
   EXTERNAL RAM     0x20000  0x2805f   32864    65536
   ROM/EPROM/FLASH  0xfc2800 0xffffff  13479   251904
";

    #[test]
    fn summary_counts_heap_once_and_keeps_internal_ram_separate() {
        assert_eq!(sketch_size(REPORT).unwrap(), (13479, 32864));
        assert_eq!(
            sketch_size(&REPORT.replace("0      256", "0x0 0xf 16 256")).unwrap(),
            (13479, 32880)
        );
    }

    #[test]
    fn empty_regions_without_addresses_are_recognized() {
        assert_eq!(
            sketch_size("EXTERNAL RAM 0 65536\nROM/EPROM/FLASH 0 251904\n").unwrap(),
            (0, 0)
        );
    }

    #[test]
    fn invalid_or_oversized_reports_fail_instead_of_showing_free_memory() {
        for report in [
            String::new(),
            REPORT.replace("EXTERNAL RAM", "UNKNOWN RAM"),
            REPORT.replace("13479   251904", "invalid 251904"),
            REPORT.replace("13479   251904", "251905 251904"),
            REPORT.replace("32864    65536", "65537 65536"),
            REPORT.replace("0      256", "257 256"),
            format!("{REPORT}EXTERNAL RAM 0 65536\n"),
        ] {
            assert!(sketch_size(&report).is_err(), "accepted report: {report}");
        }
    }
}
