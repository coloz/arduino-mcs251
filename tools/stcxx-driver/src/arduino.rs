//! SDK-owned build-system interface. Compiler implementation stays in STCXX.
//! Public .o/.a files are self-contained, so generic object/archive caches work.
use anyhow::{Context, Result, ensure};
use serde_json::{Value, json};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    fs,
    io::{Cursor, Read, Write},
    path::{Path, PathBuf},
    process::Command,
};
use zip::{ZipArchive, ZipWriter, write::SimpleFileOptions};

type Files = BTreeMap<String, Vec<u8>>;
fn text(path: impl AsRef<Path>) -> String {
    path.as_ref().to_string_lossy().replace('\\', "/")
}
fn digest(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}
fn absolute(path: impl AsRef<Path>) -> Result<PathBuf> {
    Ok(std::path::absolute(path)?)
}
fn suffix(path: &Path, ext: &str) -> PathBuf {
    PathBuf::from(format!("{}{ext}", path.display()))
}
fn write(path: &Path, bytes: &[u8]) -> Result<()> {
    fs::create_dir_all(path.parent().context("missing output directory")?)?;
    let mut temp = tempfile::NamedTempFile::new_in(path.parent().unwrap())?;
    temp.write_all(bytes)?;
    temp.persist(path)?;
    Ok(())
}
fn safe_name(name: &str) -> Result<()> {
    ensure!(
        !name.is_empty() && ![".", ".."].contains(&name) && !name.contains(['/', '\\', ':']),
        "invalid bundle member: {name}"
    );
    Ok(())
}
fn pack(kind: &str, mut files: Files, mut meta: Value) -> Result<Vec<u8>> {
    ensure!(
        !files.contains_key("manifest.json"),
        "reserved bundle member"
    );
    meta["format"] = json!("stcxx-arduino");
    meta["version"] = json!(1);
    meta["kind"] = json!(kind);
    meta["files"] = json!(
        files
            .iter()
            .map(|(n, b)| (n.clone(), digest(b)))
            .collect::<BTreeMap<_, _>>()
    );
    files.insert("manifest.json".into(), serde_json::to_vec(&meta)?);
    let mut zip = ZipWriter::new(Cursor::new(Vec::new()));
    for (name, bytes) in files {
        safe_name(&name)?;
        zip.start_file(name, SimpleFileOptions::default())?;
        zip.write_all(&bytes)?;
    }
    Ok(zip.finish()?.into_inner())
}
fn unpack(bytes: &[u8], kind: &str) -> Result<(Files, Value)> {
    let mut zip = ZipArchive::new(Cursor::new(bytes))?;
    let mut files = Files::new();
    let mut total = 0u64;
    for i in 0..zip.len() {
        let mut member = zip.by_index(i)?;
        safe_name(member.name())?;
        total = total
            .checked_add(member.size())
            .context("bundle size overflow")?;
        ensure!(
            member.is_file() && total <= 512 * 1024 * 1024,
            "invalid or oversized bundle"
        );
        let name = member.name().to_owned();
        let mut data = Vec::new();
        member.read_to_end(&mut data)?;
        ensure!(
            files.insert(name, data).is_none(),
            "duplicate bundle member"
        );
    }
    let meta: Value = serde_json::from_slice(
        &files
            .remove("manifest.json")
            .context("missing SDK bundle manifest; clean the build cache")?,
    )?;
    ensure!(
        meta["format"] == "stcxx-arduino" && meta["version"] == 1 && meta["kind"] == kind,
        "incompatible SDK bundle; clean the build cache"
    );
    let hashes = meta["files"]
        .as_object()
        .context("missing bundle inventory")?;
    ensure!(files.len() == hashes.len(), "incomplete bundle inventory");
    for (name, data) in &files {
        ensure!(
            hashes.get(name) == Some(&json!(digest(data))),
            "bundle checksum mismatch: {name}"
        );
    }
    Ok((files, meta))
}
fn child(platform: &Path, args: &[String]) -> Result<()> {
    let status = Command::new(std::env::current_exe()?)
        .arg("--platform")
        .arg(platform)
        .args(args)
        .status()?;
    ensure!(status.success(), "STCXX {} failed ({status})", args[0]);
    Ok(())
}

pub fn dispatch() -> Result<bool> {
    let mut args: Vec<String> = std::env::args().skip(1).collect();
    let platform = if args.first().is_some_and(|a| a == "--platform") {
        ensure!(args.len() >= 3, "missing platform directory");
        let root = absolute(&args[1])?;
        args.drain(..2);
        Some(root)
    } else {
        None
    };
    let Some(op) = args.first().map(String::as_str) else {
        return Ok(false);
    };
    if ![
        "compile-unit",
        "archive-unit",
        "rcs",
        "link-units",
        "export-hex",
        "prepare-sketch",
        "size",
    ]
    .contains(&op)
    {
        return Ok(false);
    }
    match op {
        "size" => {
            ensure!(args.len() == 2, "size requires one .mem report");
            crate::memory::size(&absolute(&args[1])?)?;
        }
        "rcs" | "archive-unit" => {
            ensure!(args.len() >= 3, "archive requires output and objects");
            archive(&absolute(&args[1])?, &args[2..], op == "archive-unit")?;
        }
        "export-hex" => {
            ensure!(
                args.len() == 3,
                "export-hex requires ELF input and HEX output"
            );
            write(
                &absolute(&args[2])?,
                &crate::firmware::to_hex(&fs::read(&args[1])?)?,
            )?;
        }
        "prepare-sketch" => {
            ensure!(
                args.len() == 3,
                "prepare-sketch requires build and sketch directories"
            );
            prepare(&absolute(&args[1])?, &absolute(&args[2])?)?;
        }
        _ => {
            let root = platform.context("SDK compilation requires --platform")?;
            if op == "compile-unit" {
                compile(&root, &args[1..])?;
            } else {
                link(&root, &args[1..])?;
            }
        }
    }
    Ok(true)
}

fn compile(platform: &Path, args: &[String]) -> Result<()> {
    ensure!(
        args.len() >= 4,
        "compile-unit requires backend, source, output and recipe"
    );
    let output = absolute(&args[2])?;
    let source = absolute(&args[1])?;
    fs::create_dir_all(output.parent().unwrap())?;
    let temp = tempfile::Builder::new()
        .prefix(".stcxx-unit-")
        .tempdir_in(output.parent().unwrap())?;
    let raw = temp
        .path()
        .join(output.file_name().context("missing object name")?);
    let dep = args
        .windows(2)
        .find(|a| a[0] == "-MF")
        .map(|a| absolute(&a[1]))
        .transpose()?
        .unwrap_or_else(|| output.with_extension("d"));
    let raw_dep = raw.with_extension("d");
    let mut forwarded = vec![
        "compile".into(),
        args[0].clone(),
        text(&source),
        text(&raw),
        args[3].clone(),
    ];
    let mut flags = args[4..].iter();
    while let Some(flag) = flags.next() {
        if flag == "-MF" {
            flags.next().context("missing dependency file")?;
        } else {
            forwarded.push(flag.clone());
        }
    }
    forwarded.extend(["-MF".into(), text(&raw_dep)]);
    child(platform, &forwarded)?;
    let mut files = Files::new();
    for path in [
        raw.clone(),
        raw.with_extension("rel"),
        raw.with_extension("lst"),
        suffix(&raw.with_extension("rel"), ".stcxx-c.json"),
        suffix(&raw, ".stcxx.json"),
        suffix(&raw, ".stcxx.bc"),
        suffix(&raw, ".stcxx.ll"),
        suffix(&raw, ".stcxx.module.cbe"),
    ] {
        if path.is_file() {
            files.insert(
                path.file_name().unwrap().to_string_lossy().into_owned(),
                fs::read(&path)?,
            );
        }
    }
    files.insert("source".into(), fs::read(&source)?);
    // Native pruning classifies library inputs before paths move into bundles.
    let native_name = raw
        .with_extension("rel")
        .file_name()
        .unwrap()
        .to_string_lossy()
        .to_string()
        + ".stcxx-c.json";
    if let Some(bytes) = files.get_mut(&native_name) {
        let mut meta: Value = serde_json::from_slice(bytes)?;
        if text(&source).to_lowercase().contains("/libraries/")
            || text(&output).to_lowercase().contains("/libraries/")
        {
            meta["library_source"] = json!(true);
        }
        *bytes = serde_json::to_vec(&meta)?;
    }
    let bytes = pack(
        "unit",
        files,
        json!({"object":raw.file_name().unwrap().to_string_lossy()}),
    )?;
    // Remove only our ASCII temporary directory. Keep the compiler's path
    // spelling, make escaping and Windows ANSI encoding in the target too.
    let dependency = fs::read(&raw_dep)?;
    let target = retarget_dependency(
        &dependency,
        temp.path().file_name().unwrap().to_str().unwrap(),
    )?;
    write(&dep, &target)?;
    write(&output, &bytes)
}

fn retarget_dependency(dependency: &[u8], temporary: &str) -> Result<Vec<u8>> {
    let split = dependency
        .windows(2)
        .position(|s| s == b": " || s == b":\t")
        .context("missing dependency target")?;
    let start = dependency[..split]
        .windows(temporary.len())
        .position(|s| s == temporary.as_bytes())
        .context("missing temporary dependency directory")?;
    let mut end = start + temporary.len();
    ensure!(
        matches!(dependency.get(end), Some(b'/' | b'\\')),
        "invalid dependency path"
    );
    while matches!(dependency.get(end), Some(b'/' | b'\\')) {
        end += 1;
    }
    let mut result = dependency[..start].to_vec();
    result.extend_from_slice(&dependency[end..]);
    Ok(result)
}

fn archive(output: &Path, args: &[String], incremental: bool) -> Result<()> {
    let mut files = if incremental && output.exists() {
        unpack(&fs::read(output)?, "archive")?.0
    } else {
        Files::new()
    };
    for arg in args {
        let object = absolute(arg)?;
        let bytes = fs::read(&object)?;
        unpack(&bytes, "unit").with_context(|| format!("invalid object {}", object.display()))?;
        // Archive caches relocate between builds. Updating a restored archive
        // must replace the same member instead of keeping an old-path duplicate.
        let member = object
            .strip_prefix(output.parent().unwrap())
            .unwrap_or(&object);
        files.insert(format!("{}.o", digest(text(member).as_bytes())), bytes);
    }
    write(output, &pack("archive", files, json!({}))?)
}

fn materialize_unit(bytes: &[u8], directory: &Path, prefix: &str) -> Result<PathBuf> {
    let (files, meta) = unpack(bytes, "unit")?;
    let name = meta["object"].as_str().context("missing object name")?;
    safe_name(name)?;
    ensure!(
        name.ends_with(".o") && files.contains_key(name) && files.contains_key("source"),
        "incomplete object bundle"
    );
    let original = Path::new(name);
    let new_name = format!("{prefix}{name}");
    let object = directory.join(&new_name);
    let source = suffix(&object, ".stcxx.source");
    let rel_name = text(original.with_extension("rel"));
    for (old, bytes) in files {
        let new = if old == "source" {
            source.clone()
        } else {
            directory.join(format!("{prefix}{old}"))
        };
        let mut bytes = bytes;
        if old == format!("{name}.stcxx.json") {
            let mut m: Value = serde_json::from_slice(&bytes)?;
            m["object"] = json!(text(&object));
            m["bitcode"] = json!(text(suffix(&object, ".stcxx.bc")));
            m["source"] = json!(text(&source));
            bytes = serde_json::to_vec(&m)?;
        }
        if old == format!("{rel_name}.stcxx-c.json") {
            let mut m: Value = serde_json::from_slice(&bytes)?;
            m["original_rel"] = json!(text(object.with_extension("rel")));
            bytes = serde_json::to_vec(&m)?;
        }
        write(&new, &bytes)?;
    }
    Ok(object)
}

fn materialize_archive(platform: &Path, sdar: &Path, bytes: &[u8], work: &Path) -> Result<PathBuf> {
    let directory = work.join("archives").join(digest(bytes));
    let output = directory.join("core.a");
    let marker = directory.join("complete.json");
    let driver = digest(&fs::read(std::env::current_exe()?)?);
    if let Ok(m) = fs::read(&marker)
        && let Ok(m) = serde_json::from_slice::<Value>(&m)
        && m["driver"] == driver
        && output.is_file()
        && m["archive"] == digest(&fs::read(&output)?)
    {
        return Ok(output);
    }
    let (units, _) = unpack(bytes, "archive")?;
    fs::create_dir_all(&directory)?;
    if output.exists() {
        fs::remove_file(&output)?;
    }
    for (key, unit) in units {
        let (_, meta) = unpack(&unit, "unit")?;
        let special = matches!(
            meta["object"].as_str(),
            Some("stcxx_heap.c.o" | "stcxx_heap_state.c.o")
        );
        let prefix = if special {
            String::new()
        } else {
            format!("{}-", &digest(key.as_bytes())[..16])
        };
        let object = materialize_unit(&unit, &directory, &prefix)?;
        child(
            platform,
            &[
                "archive".into(),
                text(sdar),
                text(&output),
                text(&object),
                "rcs".into(),
            ],
        )?;
    }
    write(
        &marker,
        &serde_json::to_vec(&json!({"driver":driver,"archive":digest(&fs::read(&output)?)}))?,
    )?;
    Ok(output)
}

fn link(platform: &Path, args: &[String]) -> Result<()> {
    ensure!(
        args.len() >= 3,
        "link-units requires backend and link flags"
    );
    let output = absolute(
        args.windows(2)
            .find(|a| a[0] == "-o")
            .context("missing link output")?
            .get(1)
            .unwrap(),
    )?;
    ensure!(
        output.extension().is_some_and(|e| e == "elf"),
        "SDK link target must be .elf"
    );
    for path in [&output, &output.with_extension("hex")] {
        if path.exists() {
            fs::remove_file(path)?;
        }
    }
    let work = output.parent().unwrap().join("stcxx-arduino");
    let hex = output.with_extension("stcxx.hex");
    let sdcc = absolute(&args[0])?;
    let sdar = sdcc
        .parent()
        .context("missing backend directory")?
        .join(format!("sdar{}", std::env::consts::EXE_SUFFIX));
    let mut forwarded = vec!["link".into(), text(&sdcc)];
    let mut flags = args[1..].iter();
    while let Some(arg) = flags.next() {
        if arg == "-o" {
            flags.next();
            forwarded.extend(["-o".into(), text(&hex)]);
        } else if ["-Wl,--whole-archive", "-Wl,--no-whole-archive"].contains(&arg.as_str()) {
            continue;
        } else if !arg.starts_with('-')
            && Path::new(arg)
                .extension()
                .is_some_and(|e| e == "o" || e == "a")
        {
            let path = absolute(arg)?;
            let bytes = fs::read(&path)
                .with_context(|| format!("missing link input {}", path.display()))?;
            let actual = if path.extension().unwrap() == "o" {
                materialize_unit(&bytes, &work.join("objects").join(digest(&bytes)), "")?
            } else {
                materialize_archive(platform, &sdar, &bytes, &work)?
            };
            forwarded.push(text(actual));
        } else {
            forwarded.push(arg.clone());
        }
    }
    child(platform, &forwarded)?;
    let elf = crate::firmware::from_hex(&fs::read(&hex)?)?;
    write(
        &output.with_extension("mem"),
        &fs::read(hex.with_extension("mem"))?,
    )?;
    write(&output, &elf)
}

fn prepare(build: &Path, sketch: &Path) -> Result<()> {
    // Arduino CLI already copies these files. Only the builder's environment
    // requires its existing generated-sketch hook; no builder version switch.
    let Some(env_build) = std::env::var_os("BUILD_PATH") else {
        return Ok(());
    };
    let Some(env_sketch) = std::env::var_os("SKETCH_DIR_PATH") else {
        return Ok(());
    };
    if absolute(env_build)? != build || absolute(env_sketch)? != sketch {
        return Ok(());
    }
    let main = std::env::var_os("SKETCH_PATH").map(absolute).transpose()?;
    mirror(build, sketch, main.as_deref())
}

fn mirror(build: &Path, sketch: &Path, main: Option<&Path>) -> Result<()> {
    ensure!(
        sketch.is_dir(),
        "missing sketch directory: {}",
        sketch.display()
    );
    let source_tree = sketch.join("src");
    let target = build.join("sketch");
    let manifest = build.join("stcxx-sketch-inputs.json");
    let previous: BTreeMap<String, String> = if manifest.exists() {
        serde_json::from_slice(&fs::read(&manifest)?)?
    } else {
        BTreeMap::new()
    };
    let mut current = BTreeMap::new();
    for entry in walkdir::WalkDir::new(sketch)
        .max_depth(1)
        .into_iter()
        .chain(
            walkdir::WalkDir::new(&source_tree)
                .into_iter()
                .filter(|e| e.as_ref().map_or(true, |e| e.path() != source_tree)),
        )
    {
        let entry = match entry {
            Ok(e) => e,
            Err(e)
                if e.path() == Some(source_tree.as_path())
                    && e.io_error()
                        .is_some_and(|e| e.kind() == std::io::ErrorKind::NotFound) =>
            {
                continue;
            }
            Err(e) => return Err(e.into()),
        };
        let input = entry.path();
        if !entry.file_type().is_file() || Some(input) == main {
            continue;
        }
        if !input.extension().and_then(|s| s.to_str()).is_some_and(|s| {
            [
                "c", "cc", "cpp", "cxx", "S", "s", "h", "hh", "hpp", "hxx", "inc",
            ]
            .contains(&s)
        }) {
            continue;
        }
        let relative = input.strip_prefix(sketch)?;
        let output = target.join(relative);
        let bytes = fs::read(input)?;
        let hash = digest(&bytes);
        if output.exists() {
            let old = digest(&fs::read(&output)?);
            ensure!(
                previous.get(&text(relative)) == Some(&old) || old == hash,
                "sketch hook output conflicts with {}",
                output.display()
            );
            if old != hash {
                write(&output, &bytes)?;
            }
        } else {
            write(&output, &bytes)?;
        }
        current.insert(text(relative), hash);
    }
    for (name, hash) in previous {
        ensure!(
            !Path::new(&name).is_absolute()
                && Path::new(&name)
                    .components()
                    .all(|c| matches!(c, std::path::Component::Normal(_))),
            "invalid generated source path"
        );
        let path = target.join(&name);
        if !current.contains_key(&name) && path.is_file() && digest(&fs::read(&path)?) == hash {
            fs::remove_file(path)?;
        }
    }
    write(&manifest, &serde_json::to_vec_pretty(&current)?)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn dependency_target_keeps_path_encoding_and_escaping() {
        for (input, expected) in [
            (
                b"C:\\legacy\xb2\xe2\\.stcxx-unit-a\\out.o: source.c\n".as_slice(),
                b"C:\\legacy\xb2\xe2\\out.o: source.c\n".as_slice(),
            ),
            (
                b"C:/with\\ space/.stcxx-unit-a/out.o: source.cpp\n",
                b"C:/with\\ space/out.o: source.cpp\n",
            ),
        ] {
            assert_eq!(
                retarget_dependency(input, ".stcxx-unit-a").unwrap(),
                expected
            );
        }
    }
    #[test]
    fn bundles_check_inventory_and_relocate_cpp_metadata() {
        let temp = tempfile::tempdir().unwrap();
        let files = BTreeMap::from([
            ("test.cpp.o".into(), b"object".to_vec()),
            ("source".into(), b"source".to_vec()),
            ("test.cpp.o.stcxx.json".into(), b"{}".to_vec()),
        ]);
        let bytes = pack("unit", files.clone(), json!({"object":"test.cpp.o"})).unwrap();
        assert!(unpack(&bytes, "archive").is_err());
        let obj = materialize_unit(&bytes, temp.path(), "a-").unwrap();
        let meta: Value =
            serde_json::from_slice(&fs::read(suffix(&obj, ".stcxx.json")).unwrap()).unwrap();
        assert_eq!(meta["object"], text(&obj));
        assert_eq!(
            fs::read(meta["source"].as_str().unwrap()).unwrap(),
            b"source"
        );
        let mut broken = bytes.clone();
        broken.truncate(broken.len() / 2);
        assert!(unpack(&broken, "unit").is_err());
        assert!(
            pack(
                "unit",
                BTreeMap::from([("../escape".into(), vec![])]),
                json!({})
            )
            .is_err()
        );
    }
    #[test]
    fn rcs_replaces_old_members_but_arduino_recipe_updates() {
        let temp = tempfile::tempdir().unwrap();
        let a = temp.path().join("a.o");
        let b = temp.path().join("b.o");
        let out = temp.path().join("core.a");
        for obj in [&a, &b] {
            write(obj, &pack("unit", Files::new(), json!({})).unwrap()).unwrap();
        }
        archive(&out, &[text(&a)], true).unwrap();
        archive(&out, &[text(&b)], true).unwrap();
        assert_eq!(
            unpack(&fs::read(&out).unwrap(), "archive").unwrap().0.len(),
            2
        );
        archive(&out, &[text(&a)], false).unwrap();
        assert_eq!(
            unpack(&fs::read(&out).unwrap(), "archive").unwrap().0.len(),
            1
        );
        let relocated = temp.path().join("relocated");
        fs::create_dir(&relocated).unwrap();
        fs::copy(&out, relocated.join("core.a")).unwrap();
        fs::copy(&a, relocated.join("a.o")).unwrap();
        archive(
            &relocated.join("core.a"),
            &[text(relocated.join("a.o"))],
            true,
        )
        .unwrap();
        assert_eq!(
            unpack(&fs::read(relocated.join("core.a")).unwrap(), "archive")
                .unwrap()
                .0
                .len(),
            1
        );
    }
    #[test]
    fn sketch_hook_updates_and_removes_only_owned_files() {
        let temp = tempfile::tempdir().unwrap();
        let src = temp.path().join("project");
        let build = temp.path().join("build");
        write(&src.join("main.cpp"), b"main").unwrap();
        write(&src.join("native.c"), b"native").unwrap();
        write(&src.join("src/nested.cpp"), b"nested").unwrap();
        write(&src.join("extras/unused.cpp"), b"unused").unwrap();
        mirror(&build, &src, Some(&src.join("main.cpp"))).unwrap();
        assert!(!build.join("sketch/main.cpp").exists());
        assert!(!build.join("sketch/extras").exists());
        assert!(build.join("sketch/src/nested.cpp").exists());
        write(&build.join("sketch/other.cpp"), b"other hook").unwrap();
        fs::remove_file(src.join("native.c")).unwrap();
        mirror(&build, &src, Some(&src.join("main.cpp"))).unwrap();
        assert!(!build.join("sketch/native.c").exists());
        assert!(build.join("sketch/other.cpp").exists());
    }

    #[test]
    fn missing_sketch_does_not_remove_previous_outputs() {
        let temp = tempfile::tempdir().unwrap();
        let src = temp.path().join("project");
        let build = temp.path().join("build");
        write(&src.join("native.c"), b"native").unwrap();
        mirror(&build, &src, None).unwrap();
        let manifest = fs::read(build.join("stcxx-sketch-inputs.json")).unwrap();
        fs::rename(&src, temp.path().join("moved-project")).unwrap();
        assert!(mirror(&build, &src, None).is_err());
        assert_eq!(fs::read(build.join("sketch/native.c")).unwrap(), b"native");
        assert_eq!(
            fs::read(build.join("stcxx-sketch-inputs.json")).unwrap(),
            manifest
        );
    }

    #[test]
    fn sketch_hook_preserves_modified_outputs_on_update() {
        let temp = tempfile::tempdir().unwrap();
        let src = temp.path().join("project");
        let build = temp.path().join("build");
        write(&src.join("native.c"), b"original").unwrap();
        mirror(&build, &src, None).unwrap();
        write(&build.join("sketch/native.c"), b"other hook").unwrap();
        write(&src.join("native.c"), b"updated").unwrap();
        assert!(mirror(&build, &src, None).is_err());
        assert_eq!(
            fs::read(build.join("sketch/native.c")).unwrap(),
            b"other hook"
        );
        // An unchanged owned copy still follows edits in the source directory.
        write(&build.join("sketch/native.c"), b"original").unwrap();
        mirror(&build, &src, None).unwrap();
        assert_eq!(fs::read(build.join("sketch/native.c")).unwrap(), b"updated");
    }
}
