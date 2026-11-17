//! Keep Windows linker inputs below the native tools' path-length limits.
//!
//! A workspace is stable for one final output path and remains in the user's
//! temporary directory: cached objects and paths recorded in diagnostics must
//! survive the linking command. The open lock serializes its use. An optional
//! STCXX_WORK_ROOT replaces the temporary root; both layouts append stcxx/<id>.
use anyhow::Result;
use std::{
    fs,
    path::{Path, PathBuf},
};

pub struct LinkWorkspace {
    pub directory: PathBuf,
    _lock: fs::File,
}

pub fn for_output(output: &Path, paths: &[PathBuf]) -> Result<Option<LinkWorkspace>> {
    #[cfg(windows)]
    {
        windows::for_output(output, paths)
    }
    #[cfg(not(windows))]
    {
        let _ = (output, paths);
        Ok(None)
    }
}

pub fn temporary(paths: &[PathBuf]) -> Result<tempfile::TempDir> {
    #[cfg(windows)]
    {
        windows::temporary(paths)
    }
    #[cfg(not(windows))]
    {
        let _ = paths;
        Ok(tempfile::tempdir()?)
    }
}

#[cfg(windows)]
mod windows {
    use super::*;
    use anyhow::{Context, ensure};
    use serde_json::json;
    use sha2::{Digest, Sha256};
    use std::{
        ffi::OsString,
        io::ErrorKind,
        os::windows::ffi::{OsStrExt, OsStringExt},
        path::{Component, Prefix},
    };

    fn fits(directory: &Path, paths: &[PathBuf]) -> Result<bool> {
        if !stcxx_driver::backend_paths::compatible(directory)? {
            return Ok(false);
        }
        for path in paths {
            ensure!(
                !path.is_absolute(),
                "workspace plan must contain relative paths"
            );
            if !stcxx_driver::backend_paths::compatible(&directory.join(path))? {
                return Ok(false);
            }
        }
        Ok(true)
    }

    // canonicalize() returns extended paths on Windows. Keep those paths at IO
    // boundaries; native SDCC tools receive ordinary drive or UNC paths.
    fn ordinary_path(path: &Path) -> Result<PathBuf> {
        let mut components = path.components();
        let Some(Component::Prefix(prefix)) = components.next() else {
            return Ok(path.to_path_buf());
        };
        let mut result = match prefix.kind() {
            Prefix::VerbatimDisk(drive) => PathBuf::from(format!("{}:\\", drive as char)),
            Prefix::VerbatimUNC(server, share) => {
                let mut root = OsString::from("\\\\");
                root.push(server);
                root.push("\\");
                root.push(share);
                root.push("\\");
                PathBuf::from(root)
            }
            Prefix::Disk(_) | Prefix::UNC(_, _) => return Ok(path.to_path_buf()),
            _ => anyhow::bail!("unsupported Windows linker path: {}", path.display()),
        };
        for component in components {
            if component != Component::RootDir {
                result.push(component.as_os_str());
            }
        }
        Ok(result)
    }

    fn short_path(path: &Path) -> std::io::Result<PathBuf> {
        #[link(name = "kernel32")]
        unsafe extern "system" {
            fn GetShortPathNameW(long: *const u16, short: *mut u16, size: u32) -> u32;
        }
        let input: Vec<u16> = path.as_os_str().encode_wide().chain(Some(0)).collect();
        // Both calls use live UTF-16 buffers; a zero-sized first call computes
        // the required allocation, including its terminating NUL.
        let size = unsafe { GetShortPathNameW(input.as_ptr(), std::ptr::null_mut(), 0) };
        if size == 0 {
            return Err(std::io::Error::last_os_error());
        }
        let mut output = vec![0u16; size as usize];
        let written = unsafe { GetShortPathNameW(input.as_ptr(), output.as_mut_ptr(), size) };
        if written == 0 {
            return Err(std::io::Error::last_os_error());
        }
        if written >= size {
            return Err(std::io::Error::other(
                "temporary root changed while resolving its short path",
            ));
        }
        Ok(PathBuf::from(OsString::from_wide(
            &output[..written as usize],
        )))
    }

    fn lock_workspace(directory: PathBuf, identity: &str) -> Result<LinkWorkspace> {
        fs::create_dir_all(&directory).context("create short linker workspace")?;
        // Never replace this file: every process must lock the same file.
        let lock = fs::OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .open(directory.join("link.lock"))
            .context("open linker workspace lock")?;
        lock.lock().context("lock linker workspace")?;
        let owner = directory.join("owner.json");
        let expected = json!({"schema_version": 1, "output": identity});
        match fs::read(&owner) {
            Ok(bytes) => {
                let existing: serde_json::Value =
                    serde_json::from_slice(&bytes).context("read linker workspace owner")?;
                ensure!(
                    existing == expected,
                    "linker workspace belongs to a different output: {}",
                    directory.display()
                );
            }
            Err(error) if error.kind() == ErrorKind::NotFound => {
                stcxx_driver::atomic_write(&owner, &serde_json::to_vec_pretty(&expected)?)?;
            }
            Err(error) => return Err(error).context("read linker workspace owner"),
        }
        Ok(LinkWorkspace {
            directory,
            _lock: lock,
        })
    }

    fn root() -> Result<PathBuf> {
        let root = std::env::var_os("STCXX_WORK_ROOT")
            .filter(|root| !root.is_empty())
            .map(PathBuf::from)
            .unwrap_or_else(std::env::temp_dir);
        let root = std::path::absolute(root)?;
        fs::create_dir_all(&root).context("create compiler workspace root")?;
        let canonical = root
            .canonicalize()
            .context("resolve compiler workspace root")?;
        let root = ordinary_path(&short_path(&canonical).unwrap_or(canonical))?.join("stcxx");
        fs::create_dir_all(&root)?;
        Ok(root)
    }

    fn check_plan(directory: &Path, paths: &[PathBuf]) -> Result<()> {
        ensure!(
            fits(directory, paths)?,
            "Windows compiler workspace cannot fit the native backend's path budget: {}; set STCXX_WORK_ROOT or TEMP/TMP to a shorter directory without spaces",
            directory.display()
        );
        Ok(())
    }

    pub(super) fn temporary(paths: &[PathBuf]) -> Result<tempfile::TempDir> {
        let temp = tempfile::Builder::new()
            .prefix("unit-")
            .tempdir_in(root()?)?;
        check_plan(temp.path(), paths)?;
        Ok(temp)
    }

    pub(super) fn for_output(output: &Path, paths: &[PathBuf]) -> Result<Option<LinkWorkspace>> {
        let output = ordinary_path(&std::path::absolute(output)?)?;
        let parent = output.parent().context("missing link output directory")?;
        if fits(parent, paths)?
            && stcxx_driver::backend_paths::compatible(&output.with_extension("stcxx.hex"))?
        {
            return Ok(None);
        }
        fs::create_dir_all(parent).context("create link output directory")?;
        let normalized = ordinary_path(&parent.canonicalize()?)?
            .join(output.file_name().context("missing link output filename")?);
        // Windows paths are case-insensitive. Normalize separators and case so
        // alternate spellings of one output share its workspace and lock.
        let identity = normalized
            .to_str()
            .context("link output path is not valid Unicode")?
            .replace('\\', "/")
            .to_lowercase();
        let digest = format!("{:x}", Sha256::digest(identity.as_bytes()));
        let directory = root()?.join(&digest[..16]);
        check_plan(&directory, paths)?;
        lock_workspace(directory, &identity).map(Some)
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        #[test]
        fn plan_uses_actual_filenames_instead_of_a_root_threshold() {
            let root = Path::new(r"C:\short");
            assert!(fits(root, &[PathBuf::from("input.rel")]).unwrap());
            assert!(!fits(root, &[PathBuf::from(format!("{}.rel", "n".repeat(250)))]).unwrap());
            let longer_root = PathBuf::from(format!("C:\\{}", "a".repeat(100)));
            assert!(fits(&longer_root, &[PathBuf::from("input.rel")]).unwrap());
            assert!(!fits(Path::new(r"C:\with space"), &[PathBuf::from("input.rel")]).unwrap());
        }

        #[test]
        fn extended_paths_become_ordinary_drive_and_unc_paths() {
            assert_eq!(
                ordinary_path(Path::new(r"\\?\C:\temp\file.elf")).unwrap(),
                Path::new(r"C:\temp\file.elf")
            );
            assert_eq!(
                ordinary_path(Path::new(r"\\?\UNC\server\share\file.elf")).unwrap(),
                Path::new(r"\\server\share\file.elf")
            );
        }

        #[test]
        fn owner_prevents_collisions_and_lock_lives_with_workspace() {
            let temp = tempfile::tempdir().unwrap();
            let directory = temp.path().join("workspace");
            let workspace = lock_workspace(directory.clone(), "c:/first.elf").unwrap();
            let another_lock = fs::OpenOptions::new()
                .read(true)
                .write(true)
                .open(directory.join("link.lock"))
                .unwrap();
            assert!(another_lock.try_lock().is_err());
            drop(workspace);
            another_lock.try_lock().unwrap();
            drop(another_lock);
            drop(lock_workspace(directory.clone(), "c:/first.elf").unwrap());
            assert!(lock_workspace(directory, "c:/other.elf").is_err());
        }
    }
}

#[cfg(all(test, not(windows)))]
mod tests {
    use super::*;

    #[test]
    fn other_hosts_keep_the_original_workspace() {
        assert!(
            for_output(Path::new("a long output directory/firmware.elf"), &[])
                .unwrap()
                .is_none()
        );
    }
}
