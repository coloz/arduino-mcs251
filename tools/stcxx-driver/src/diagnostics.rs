//! Export diagnostic files without retaining a functional dependency on TEMP.
//!
//! Ordinary reports retain their build/stcxx layout. JSON references pull in
//! only the workspace files they actually name, including selected cache files.
//! The success manifest is published after every referenced file is available.
use anyhow::{Context, Result, ensure};
use serde_json::{Value, json};
use std::{
    collections::BTreeMap,
    fs,
    io::ErrorKind,
    path::{Component, Path, PathBuf},
};

fn text(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

fn write(path: &Path, bytes: &[u8]) -> Result<()> {
    stcxx_driver::atomic_write(path, bytes)
        .with_context(|| format!("export diagnostic {}", path.display()))
}

fn write_json(path: &Path, value: &Value) -> Result<()> {
    let mut bytes = serde_json::to_vec_pretty(value)?;
    bytes.push(b'\n');
    write(path, &bytes)
}

fn remove_manifest(path: &Path) -> Result<()> {
    match fs::remove_file(path) {
        Ok(()) => Ok(()),
        Err(error) if error.kind() == ErrorKind::NotFound => Ok(()),
        Err(error) => Err(error).context("invalidate exported link manifest"),
    }
}

struct Exporter {
    directory: PathBuf,
    canonical_directory: PathBuf,
    target: PathBuf,
    files: BTreeMap<PathBuf, PathBuf>,
    destinations: BTreeMap<PathBuf, PathBuf>,
}

impl Exporter {
    fn relative(&self, path: &Path) -> Option<PathBuf> {
        if !path.is_absolute() {
            return None;
        }
        let relative = path
            .strip_prefix(&self.directory)
            .or_else(|_| path.strip_prefix(&self.canonical_directory))
            .ok()?;
        // Component matching excludes similarly named sibling directories;
        // parent traversal must not turn a workspace reference into an outside
        // file. Missing references are left verbatim as failure evidence.
        if relative.components().any(|c| c == Component::ParentDir) {
            return None;
        }
        Some(relative.to_path_buf())
    }

    fn destination(&self, relative: &Path) -> PathBuf {
        if let Ok(report) = relative.strip_prefix("stcxx") {
            // A report may name an old manifest. Export that snapshot as an
            // input, so a failed link can never recreate the success marker.
            if report != Path::new("manifest.json") && report != Path::new("workspace.json") {
                return self.target.join(report);
            }
        }
        self.target.join("workspace-files").join(relative)
    }

    fn export_file(&mut self, path: &Path) -> Result<PathBuf> {
        let source = path
            .canonicalize()
            .with_context(|| format!("resolve diagnostic {}", path.display()))?;
        ensure!(
            source.starts_with(&self.canonical_directory),
            "diagnostic link escapes workspace"
        );
        if let Some(destination) = self.files.get(&source) {
            return Ok(destination.clone());
        }
        let relative = self
            .relative(path)
            .context("diagnostic outside workspace")?;
        let destination = self.destination(&relative);
        if let Some(existing) = self.destinations.get(&destination) {
            ensure!(
                existing == &source,
                "conflicting exported diagnostic: {}",
                destination.display()
            );
        }
        // Register before following JSON references, which may contain cycles.
        self.files.insert(source.clone(), destination.clone());
        self.destinations
            .insert(destination.clone(), source.clone());
        let bytes =
            fs::read(&source).with_context(|| format!("read diagnostic {}", source.display()))?;
        if source
            .extension()
            .is_some_and(|extension| extension == "json")
        {
            let mut value: Value = serde_json::from_slice(&bytes)
                .with_context(|| format!("parse diagnostic {}", source.display()))?;
            self.relocate_json(&mut value)?;
            write_json(&destination, &value)?;
        } else {
            // Source, IR, assembly and object bytes retain their recorded hashes.
            write(&destination, &bytes)?;
        }
        Ok(destination)
    }

    fn relocate_path(&mut self, value: &str) -> Result<Option<String>> {
        let path = Path::new(value);
        if self.relative(path).is_none() {
            return Ok(None);
        }
        match fs::metadata(path) {
            Ok(metadata) if metadata.is_file() => {
                self.export_file(path).map(|path| Some(text(&path)))
            }
            Ok(_) => Ok(None),
            Err(error) if error.kind() == ErrorKind::NotFound => Ok(None),
            Err(error) => {
                Err(error).with_context(|| format!("inspect diagnostic reference {value}"))
            }
        }
    }

    fn relocate_json(&mut self, value: &mut Value) -> Result<()> {
        match value {
            Value::String(value) => {
                if let Some(relocated) = self.relocate_path(value)? {
                    *value = relocated;
                }
            }
            Value::Array(values) => {
                for value in values {
                    self.relocate_json(value)?;
                }
            }
            Value::Object(values) => {
                for (key, mut value) in std::mem::take(values) {
                    self.relocate_json(&mut value)?;
                    let key = self.relocate_path(&key)?.unwrap_or(key);
                    ensure!(
                        values.insert(key.clone(), value).is_none(),
                        "conflicting diagnostic JSON key: {key}"
                    );
                }
            }
            _ => {}
        }
        Ok(())
    }
}

/// Copy linker diagnostics and their existing workspace-file references into
/// build/stcxx. workspace.json retains only the original workspace provenance.
pub fn export(directory: &Path, build: &Path, success: bool) -> Result<()> {
    let directory = std::path::absolute(directory)?;
    let build = std::path::absolute(build)?;
    fs::create_dir_all(&build).context("create exported diagnostic directory")?;
    let source = directory.join("stcxx");
    let target = build.join("stcxx");
    let canonical_directory = match directory.canonicalize() {
        Ok(path) => Some(path),
        Err(error) if error.kind() == ErrorKind::NotFound => None,
        Err(error) => return Err(error).context("resolve diagnostic workspace"),
    };
    if let Some(directory) = &canonical_directory {
        ensure!(
            !build.canonicalize()?.starts_with(directory),
            "exported diagnostics must be outside the linker workspace"
        );
    }
    remove_manifest(&target.join("manifest.json"))?;
    let provenance = json!({
        "schema_version": 1,
        "directory": text(&directory),
        "provenance_only": true,
        "self_contained": true,
    });
    let present = match fs::metadata(&source) {
        Ok(metadata) => {
            ensure!(
                metadata.is_dir(),
                "link diagnostic source is not a directory"
            );
            true
        }
        Err(error) if error.kind() == ErrorKind::NotFound => false,
        Err(error) => return Err(error).context("inspect linker diagnostics"),
    };
    if !present {
        ensure!(!success, "successful link has no diagnostic directory");
        return write_json(&target.join("workspace.json"), &provenance);
    }
    let mut exporter = Exporter {
        directory,
        canonical_directory: canonical_directory.context("missing diagnostic workspace")?,
        target: target.clone(),
        files: BTreeMap::new(),
        destinations: BTreeMap::new(),
    };
    for entry in walkdir::WalkDir::new(&source)
        .into_iter()
        .filter_entry(|entry| {
            !entry.file_type().is_dir()
                || !matches!(
                    entry.file_name().to_str(),
                    Some("core-cache" | "native-cache")
                )
        })
    {
        let entry = entry?;
        let relative = entry.path().strip_prefix(&source)?;
        if entry.file_type().is_file()
            && ![Path::new("manifest.json"), Path::new("workspace.json")].contains(&relative)
        {
            exporter.export_file(entry.path())?;
        }
    }
    let manifest = if success {
        let path = source.join("manifest.json");
        let mut value: Value =
            serde_json::from_slice(&fs::read(&path).context("read successful link manifest")?)?;
        exporter.relocate_json(&mut value)?;
        Some(value)
    } else {
        None
    };
    write_json(&target.join("workspace.json"), &provenance)?;
    // This remains the final write: partial exports never advertise success.
    if let Some(manifest) = manifest {
        write_json(&target.join("manifest.json"), &manifest)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use sha2::{Digest, Sha256};

    fn read_json(path: &Path) -> Value {
        serde_json::from_slice(&fs::read(path).unwrap()).unwrap()
    }

    fn digest(bytes: &[u8]) -> String {
        format!("{:x}", Sha256::digest(bytes))
    }

    #[test]
    fn exported_reports_and_advanced_size_survive_workspace_deletion() {
        let temp = tempfile::tempdir().unwrap();
        let directory = temp.path().join("workspace");
        let build = temp.path().join("build");
        let heap = directory.join("stcxx/core-cache/hash/stcxx_heap.c.rel");
        let generated = directory.join("stcxx/native-source-closure/hash/library.c");
        let output_rel = generated.with_extension("rel");
        let original_rel = directory.join("stcxx-arduino/archives/hash/library.rel");
        let original_archive = directory.join("stcxx/core-cache/hash/core.lib");
        let unused = directory.join("stcxx/core-cache/hash/unused.rel");
        let external = temp.path().join("outside.c");
        let sibling = temp.path().join("workspace-other/file.rel");
        let missing = directory.join("missing.rel");
        for (path, bytes) in [
            (&heap, b"A XSEG size 1000 flags 0\n".as_slice()),
            (&generated, b"int library(void) { return 1; }\n"),
            (&output_rel, b"generated REL"),
            (&original_rel, b"original REL"),
            (&original_archive, b"original archive"),
            (&unused, b"unreferenced cache member"),
            (&external, b"external source"),
            (&sibling, b"similarly named sibling"),
        ] {
            write(path, bytes).unwrap();
        }
        let hashes =
            serde_json::Map::from_iter([(text(&original_rel), json!(digest(b"original REL")))]);
        write_json(
            &directory.join("stcxx/native-source-closure.json"),
            &json!({
                "units": [{"source":text(&external),"generated":text(&generated),
                    "generated_sha256":digest(&fs::read(&generated).unwrap()),
                    "output_rel":text(&output_rel),"original_rel":text(&original_rel)}],
                "archives": [{"original":text(&original_archive),"output":text(&output_rel)}],
                "files": hashes,
                "missing":text(&missing), "sibling":text(&sibling),
                "escaped":text(&directory.join("../outside.c")),
            }),
        )
        .unwrap();
        write_json(
            &directory.join("stcxx/manifest.json"),
            &json!({"heap":text(&heap),"outcome":"pass"}),
        )
        .unwrap();
        let memory = build.join("sketch.mem");
        write(
            &memory,
            b"ROM/EPROM/FLASH 1 65536\nEXTERNAL RAM 4096 32768\n",
        )
        .unwrap();
        export(&directory, &build, true).unwrap();
        let manifest = read_json(&build.join("stcxx/manifest.json"));
        let report = read_json(&build.join("stcxx/native-source-closure.json"));
        assert!(!build.join("stcxx/core-cache/hash/unused.rel").exists());
        assert_eq!(report["missing"], text(&missing));
        assert_eq!(report["sibling"], text(&sibling));
        assert_eq!(report["escaped"], text(&directory.join("../outside.c")));
        assert_eq!(report["units"][0]["source"], text(&external));
        assert!(
            directory
                .canonicalize()
                .unwrap()
                .starts_with(temp.path().canonicalize().unwrap())
        );
        fs::remove_dir_all(&directory).unwrap();
        assert_eq!(
            fs::read(manifest["heap"].as_str().unwrap()).unwrap(),
            b"A XSEG size 1000 flags 0\n"
        );
        for key in ["generated", "output_rel", "original_rel"] {
            assert!(Path::new(report["units"][0][key].as_str().unwrap()).is_file());
        }
        assert_eq!(
            digest(&fs::read(report["units"][0]["generated"].as_str().unwrap()).unwrap()),
            report["units"][0]["generated_sha256"]
        );
        for key in report["files"].as_object().unwrap().keys() {
            assert_eq!(fs::read(key).unwrap(), b"original REL");
        }
        for key in ["original", "output"] {
            assert!(Path::new(report["archives"][0][key].as_str().unwrap()).is_file());
        }
        stcxx_driver::run(vec!["advanced-size".into(), text(&memory)]).unwrap();
    }

    #[test]
    fn referenced_json_cycles_are_exported_and_failure_has_no_success_marker() {
        let temp = tempfile::tempdir().unwrap();
        let directory = temp.path().join("workspace");
        let build = temp.path().join("build");
        let first = directory.join("stcxx/a.json");
        let second = directory.join("stcxx/core-cache/hash/b.json");
        let source_manifest = directory.join("stcxx/manifest.json");
        write_json(
            &first,
            &json!({"next":text(&second),"previous_manifest":text(&source_manifest)}),
        )
        .unwrap();
        write_json(&second, &json!({"next":text(&first)})).unwrap();
        write_json(&source_manifest, &json!({"outcome":"pass"})).unwrap();
        write_json(
            &build.join("stcxx/manifest.json"),
            &json!({"outcome":"old pass"}),
        )
        .unwrap();
        export(&directory, &build, false).unwrap();
        assert!(!build.join("stcxx/manifest.json").exists());
        let report = read_json(&build.join("stcxx/a.json"));
        let next = read_json(Path::new(report["next"].as_str().unwrap()));
        assert_eq!(next["next"], text(&build.join("stcxx/a.json")));
        assert_eq!(
            read_json(Path::new(report["previous_manifest"].as_str().unwrap()))["outcome"],
            "pass"
        );
    }

    #[test]
    fn failed_exports_remove_old_success_and_do_not_modify_sources() {
        let temp = tempfile::tempdir().unwrap();
        let directory = temp.path().join("workspace");
        let build = temp.path().join("build");
        let invalid = directory.join("stcxx/a.json");
        write(&invalid, b"broken JSON").unwrap();
        write_json(
            &directory.join("stcxx/manifest.json"),
            &json!({"outcome":"pass"}),
        )
        .unwrap();
        write_json(
            &build.join("stcxx/manifest.json"),
            &json!({"outcome":"old pass"}),
        )
        .unwrap();
        assert!(export(&directory, &build, true).is_err());
        assert!(!build.join("stcxx/manifest.json").exists());
        assert_eq!(fs::read(invalid).unwrap(), b"broken JSON");
    }
}
