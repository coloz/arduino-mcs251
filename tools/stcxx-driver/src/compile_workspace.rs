//! Compile in a short directory without exposing its paths in public bundles.
//!
//! Clang reads the original C++/assembly input. Only an incompatible native C
//! input needs a snapshot: literal include edges are resolved before copying,
//! and each include root retains its relative directory layout. Inputs whose
//! include search cannot be established conservatively fail explicitly.
use anyhow::{Context, Result, bail, ensure};
use serde_json::Value;
use sha2::{Digest, Sha256};
use std::{
    collections::{BTreeMap, BTreeSet, VecDeque},
    fs,
    path::{Path, PathBuf},
};

pub struct CompileWorkspace {
    pub source: PathBuf,
    pub object: PathBuf,
    pub dependency: PathBuf,
    pub flags: Vec<String>,
    original_source: PathBuf,
    original_output: PathBuf,
    native_dependencies: bool,
    mappings: Vec<(PathBuf, PathBuf)>,
    snapshot_files: BTreeMap<PathBuf, PathBuf>,
}

fn raw_name(output: &Path) -> &str {
    match output.file_name().and_then(|name| name.to_str()) {
        Some("stcxx_heap.c.o") => "stcxx_heap.c.o",
        Some("stcxx_heap_state.c.o") => "stcxx_heap_state.c.o",
        _ => "unit.o",
    }
}

pub fn output_paths(output: &Path) -> Vec<PathBuf> {
    let object = PathBuf::from(raw_name(output));
    let mut paths = vec![object.clone()];
    paths.extend(["rel", "lst", "asm", "sym", "d"].map(|ext| object.with_extension(ext)));
    paths.extend(
        [
            ".stcxx.json",
            ".stcxx.bc",
            ".stcxx.ll",
            ".stcxx.module.cbe",
            ".stcxx-command.txt",
            ".stcxx-placeholder.c",
        ]
        .map(|ext| PathBuf::from(format!("{}{ext}", object.display()))),
    );
    paths.push(PathBuf::from(format!(
        "{}.stcxx-c.json",
        object.with_extension("rel").display()
    )));
    paths
}

pub fn needs_source_staging(source: &Path, recipe: &str) -> Result<bool> {
    Ok(recipe == "re1" && !stcxx_driver::backend_paths::compatible(source)?)
}

impl CompileWorkspace {
    pub fn new(
        directory: &Path,
        source: &Path,
        output: &Path,
        recipe: &str,
        flags: &[String],
    ) -> Result<Self> {
        let source = std::path::absolute(source)?;
        let output = std::path::absolute(output)?;
        fs::create_dir_all(directory)?;
        for path in output_paths(&output) {
            stcxx_driver::backend_paths::check(&directory.join(path))?;
        }
        let object = directory.join(raw_name(&output));
        let mut result = Self {
            dependency: object.with_extension("d"),
            source: source.clone(),
            object,
            flags: flags.to_vec(),
            original_source: source,
            original_output: output,
            native_dependencies: recipe == "re1",
            mappings: Vec::new(),
            snapshot_files: BTreeMap::new(),
        };
        // Search directories are not opened compiler inputs. In particular an
        // unused long project -I must not force native-compatible SDK C sources
        // through the deliberately conservative literal-include fallback.
        if needs_source_staging(&result.source, recipe)? {
            result.snapshot_c(directory)?;
        }
        let original_stem = result.original_output.with_extension("");
        result
            .mappings
            .push((result.object.with_extension(""), original_stem));
        result
            .mappings
            .sort_by_key(|(path, _)| std::cmp::Reverse(path.as_os_str().len()));
        Ok(result)
    }

    /// The on-disk basename is an implementation detail; bundle member names
    /// continue to use the name requested by the build system.
    pub fn member_name(&self, path: &Path) -> Result<String> {
        let name = path
            .file_name()
            .context("missing compiled member name")?
            .to_string_lossy();
        let raw = self.object.file_stem().unwrap().to_string_lossy();
        let original = self.original_output.file_stem().unwrap().to_string_lossy();
        let tail = name
            .strip_prefix(raw.as_ref())
            .context("unexpected compiled member")?;
        ensure!(tail.starts_with('.'), "unexpected compiled member suffix");
        Ok(format!("{original}{tail}"))
    }

    /// Preserve the prerequisite bytes emitted by the compiler. Snapshot paths
    /// retain native ANSI encoding when every restored path can use it. A
    /// non-ANSI original path switches the entire native rule to UTF-8.
    pub fn rewrite_dependency(&self, dependency: &[u8]) -> Result<Vec<u8>> {
        if self.native_dependencies {
            #[cfg(windows)]
            return self.rewrite_dependency_with_code_page(dependency, native_code_page());
        }
        self.rewrite_dependency_encoded(dependency, None)
    }

    #[cfg(windows)]
    fn rewrite_dependency_with_code_page(
        &self,
        dependency: &[u8],
        code_page: u32,
    ) -> Result<Vec<u8>> {
        let unicode = std::iter::once(&self.original_output)
            .chain(std::iter::once(&self.original_source))
            .chain(self.mappings.iter().map(|(_, original)| original))
            .any(|path| encode_native_dependency(&spelling(path, false), code_page).is_err());
        if unicode {
            // Convert the complete rule, including untouched external headers.
            // Replacing only snapshot names would produce mixed ANSI/UTF-8.
            let decoded = decode_native_dependency(dependency, code_page)?;
            self.rewrite_dependency_encoded(decoded.as_bytes(), None)
        } else {
            self.rewrite_dependency_encoded(dependency, Some(code_page))
        }
    }

    fn rewrite_dependency_encoded(
        &self,
        dependency: &[u8],
        code_page: Option<u32>,
    ) -> Result<Vec<u8>> {
        let split = dependency
            .windows(2)
            .position(|bytes| bytes[0] == b':' && bytes[1].is_ascii_whitespace())
            .context("missing dependency target")?;
        let backslashes = dependency[..split].contains(&b'\\');
        let mut result = make_path_encoded(&self.original_output, backslashes, code_page)?;
        let mut prerequisites = dependency[split..].to_vec();
        for (from, to) in &self.mappings {
            // Object paths only occur in the target. The source snapshot maps
            // below cover all prerequisites and leave unrelated bytes alone.
            if !self.snapshot_files.contains_key(from)
                && !self
                    .snapshot_files
                    .keys()
                    .any(|path| path.starts_with(from))
            {
                continue;
            }
            for backslashes in [false, true] {
                prerequisites = replace_bytes(
                    &prerequisites,
                    &make_path_encoded(from, backslashes, code_page)?,
                    &make_path_encoded(to, backslashes, code_page)?,
                );
            }
        }
        result.extend(prerequisites);
        Ok(result)
    }

    pub fn rewrite_metadata(&self, metadata: &mut Value) -> Result<()> {
        self.rewrite_value(metadata);
        if metadata.get("source").is_some() {
            metadata["source"] = Value::String(spelling(&self.original_source, false));
            metadata["source_sha256"] = Value::String(hash(&self.original_source)?);
        }
        if let Some(files) = metadata
            .get_mut("source_dependencies")
            .and_then(Value::as_object_mut)
        {
            let original = std::mem::take(files);
            for (path, digest) in original {
                let restored = self.restore_string(&path);
                let file = Path::new(&restored);
                let digest = if self.snapshot_files.values().any(|path| path == file) {
                    Value::String(hash(file)?)
                } else {
                    digest
                };
                files.insert(restored, digest);
            }
        }
        Ok(())
    }

    fn restore_string(&self, input: &str) -> String {
        let mut result = input.to_owned();
        for (from, to) in &self.mappings {
            for backslashes in [false, true] {
                result = result.replace(&spelling(from, backslashes), &spelling(to, backslashes));
            }
        }
        result
    }

    fn rewrite_value(&self, value: &mut Value) {
        match value {
            Value::String(text) => *text = self.restore_string(text),
            Value::Array(values) => values
                .iter_mut()
                .for_each(|value| self.rewrite_value(value)),
            Value::Object(values) => values
                .values_mut()
                .for_each(|value| self.rewrite_value(value)),
            _ => {}
        }
    }

    fn snapshot_c(&mut self, directory: &Path) -> Result<()> {
        let source = self.original_source.clone();
        let includes: Vec<PathBuf> = self
            .flags
            .iter()
            .filter_map(|flag| flag.strip_prefix("-I"))
            .map(std::path::absolute)
            .collect::<std::io::Result<_>>()?;
        let mut groups: Vec<Group> = std::iter::once(source.parent().unwrap().to_path_buf())
            .chain(includes.iter().cloned())
            .map(Group::new)
            .collect();
        let mut queue = VecDeque::from([(source.clone(), 0usize)]);
        let mut seen = BTreeMap::new();
        let mut source_referenced = false;
        while let Some((file, group)) = queue.pop_front() {
            if let Some(previous) = seen.insert(file.clone(), group) {
                ensure!(
                    previous == group,
                    "cannot safely stage C include reached through different search roots: {}",
                    file.display()
                );
                continue;
            }
            ensure!(
                seen.len() <= 4096,
                "C literal include closure is too large to stage safely"
            );
            groups[group].add(&file)?;
            let bytes = fs::read(&file)
                .with_context(|| format!("read C staging input {}", file.display()))?;
            for include in literal_includes(&bytes).with_context(|| {
                format!(
                    "cannot safely stage C source with a native-incompatible path: {}",
                    file.display()
                )
            })? {
                ensure!(
                    !Path::new(&include.name).is_absolute(),
                    "cannot safely stage absolute C include {} in {}",
                    include.name,
                    file.display()
                );
                let local = std::path::absolute(file.parent().unwrap().join(&include.name))?;
                let resolved = if !include.system && local.is_file() {
                    Some((local, group))
                } else {
                    includes.iter().enumerate().find_map(|(index, root)| {
                        let candidate = std::path::absolute(root.join(&include.name)).ok()?;
                        candidate.is_file().then_some((candidate, index + 1))
                    })
                };
                let (file, group) = resolved.with_context(|| format!(
                    "cannot safely stage unresolved literal C include {}; provide its explicit -I directory", include.name))?;
                if group > 0
                    && stcxx_driver::backend_paths::compatible(&includes[group - 1])?
                    && stcxx_driver::backend_paths::compatible(&file)?
                {
                    // Let the original preprocessor read compatible external
                    // headers itself, including conditional and dynamic SDK
                    // includes. Their lookup location and bytes do not change.
                    continue;
                }
                source_referenced |= file == source;
                queue.push_back((file, group));
            }
        }
        for (index, group) in groups.iter().enumerate() {
            if index > 0 && group.files.is_empty() {
                // A skipped native header can use this directory through a
                // dynamic include. An empty modeled group does not establish
                // that the original search directory is unused.
                continue;
            }
            let copied_root = directory.join(format!("src{index}"));
            fs::create_dir_all(&copied_root)?;
            for original in &group.files {
                let copied = copied_root.join(original.strip_prefix(&group.root)?);
                if *original != source {
                    stcxx_driver::backend_paths::check(&copied).with_context(|| format!(
                        "C literal include snapshot cannot fit the native backend path budget: {}", original.display()))?;
                }
                fs::create_dir_all(copied.parent().unwrap())?;
                let mut bytes = format!(
                    "#line 1 {}\n",
                    serde_json::to_string(&spelling(original, false))?
                )
                .into_bytes();
                let original_bytes = fs::read(original)?;
                bytes.extend(
                    original_bytes
                        .strip_prefix(b"\xef\xbb\xbf")
                        .unwrap_or(&original_bytes),
                );
                stcxx_driver::atomic_write(&copied, &bytes)?;
                preserve_modified_time(original, &copied)?;
                self.mappings.push((copied.clone(), original.clone()));
                self.snapshot_files.insert(copied, original.clone());
            }
            // Include flags may refer below a root that was extended to retain
            // a ../ edge. Keep the original search directory within that tree.
            if index > 0 {
                let original_include = &includes[index - 1];
                let copied_include = copied_root.join(original_include.strip_prefix(&group.root)?);
                fs::create_dir_all(&copied_include)?;
                stcxx_driver::backend_paths::check(&copied_include)?;
                let mut flags = Vec::with_capacity(self.flags.len() + 1);
                for flag in self.flags.drain(..) {
                    if flag.starts_with("-I")
                        && std::path::absolute(&flag[2..])? == *original_include
                    {
                        flags.push(format!("-I{}", spelling(&copied_include, false)));
                    }
                    // Retain native fallback at the same search position for
                    // headers reached by unmodeled native dynamic includes.
                    flags.push(flag);
                }
                self.flags = flags;
                self.mappings
                    .push((copied_root.clone(), group.root.clone()));
            }
        }
        let source_group = &groups[0];
        let copied_parent = directory
            .join("src0")
            .join(source.parent().unwrap().strip_prefix(&source_group.root)?);
        let original_copy = copied_parent.join(source.file_name().unwrap());
        let keep_name = stcxx_driver::backend_paths::compatible(&original_copy)?;
        self.source = if keep_name {
            original_copy.clone()
        } else {
            ensure!(
                !source_referenced,
                "cannot safely rename a self-including C source for long-path staging"
            );
            ensure!(
                !matches!(
                    source.file_name().and_then(|name| name.to_str()),
                    Some("stcxx_heap.c" | "stcxx_heap_state.c")
                ),
                "heap source basename cannot fit native C staging path"
            );
            let mut index = 0;
            let destination = loop {
                let name = format!("stcxx_input_{index}.c");
                if !source.parent().unwrap().join(&name).exists() {
                    break copied_parent.join(name);
                }
                index += 1;
            };
            stcxx_driver::backend_paths::check(&destination)?;
            fs::copy(&original_copy, &destination)?;
            preserve_modified_time(&source, &destination)?;
            self.mappings.push((destination.clone(), source.clone()));
            self.snapshot_files
                .insert(destination.clone(), source.clone());
            destination
        };
        stcxx_driver::backend_paths::check(&self.source)?;
        // A renamed main source still searches its original snapshot directory
        // first; main-file self-inclusion is rejected before renaming.
        self.mappings
            .push((directory.join("src0"), source_group.root.clone()));
        if !overrides_base_file(&self.flags) {
            // Native headers also observe the translation unit's base file.
            // A command-line definition restores it without inspecting their
            // conditional or dynamic include subgraphs.
            self.flags.push(format!(
                "-D__BASE_FILE__={}",
                c_string_literal_utf8(&source.to_string_lossy())
            ));
        }
        Ok(())
    }
}

fn overrides_base_file(flags: &[String]) -> bool {
    let mut flags = flags.iter();
    while let Some(flag) = flags.next() {
        let name = if flag == "-D" || flag == "-U" {
            flags.next().map(String::as_str)
        } else {
            flag.strip_prefix("-D").or_else(|| flag.strip_prefix("-U"))
        };
        if name.is_some_and(|name| name.split('=').next() == Some("__BASE_FILE__")) {
            return true;
        }
    }
    false
}

struct Group {
    root: PathBuf,
    files: BTreeSet<PathBuf>,
}

impl Group {
    fn new(root: PathBuf) -> Self {
        Self {
            root,
            files: BTreeSet::new(),
        }
    }
    fn add(&mut self, file: &Path) -> Result<()> {
        while !file.starts_with(&self.root) {
            self.root = self
                .root
                .parent()
                .context("C include crosses filesystem roots")?
                .to_path_buf();
        }
        self.files.insert(file.to_path_buf());
        Ok(())
    }
}

struct Include {
    name: String,
    system: bool,
}

fn literal_includes(bytes: &[u8]) -> Result<Vec<Include>> {
    let source = std::str::from_utf8(bytes.strip_prefix(b"\xef\xbb\xbf").unwrap_or(bytes))
        .context("C long-path staging requires UTF-8 source files")?;
    let source = source.replace("\\\r\n", "").replace("\\\n", "");
    let bytes = source.as_bytes();
    let mut clean = Vec::with_capacity(bytes.len());
    let (mut index, mut quote) = (0, None);
    while index < bytes.len() {
        let byte = bytes[index];
        if let Some(end) = quote {
            clean.push(byte);
            if byte == b'\\' && index + 1 < bytes.len() {
                index += 1;
                clean.push(bytes[index]);
            } else if byte == end {
                quote = None;
            }
        } else if matches!(byte, b'\'' | b'"') {
            quote = Some(byte);
            clean.push(byte);
        } else if bytes[index..].starts_with(b"//") {
            while index < bytes.len() && bytes[index] != b'\n' {
                index += 1;
            }
            clean.push(b'\n');
        } else if bytes[index..].starts_with(b"/*") {
            clean.push(b' ');
            index += 2;
            while index < bytes.len() && !bytes[index..].starts_with(b"*/") {
                if bytes[index] == b'\n' {
                    clean.push(b'\n');
                }
                index += 1;
            }
            ensure!(index < bytes.len(), "unterminated C comment");
            index += 1;
        } else {
            clean.push(byte);
        }
        index += 1;
    }
    let clean = String::from_utf8(clean)?;
    ensure!(
        !clean.contains("__has_include"),
        "C long-path staging cannot preserve __has_include; move this C source to a native-compatible path"
    );
    let mut result = Vec::new();
    for line in clean.lines() {
        let Some(directive) = line.trim_start().strip_prefix('#') else {
            continue;
        };
        let directive = directive.trim_start();
        let end = directive
            .find(|c: char| !c.is_ascii_alphanumeric() && c != '_')
            .unwrap_or(directive.len());
        let (name, value) = directive.split_at(end);
        if name == "include_next" || name == "import" {
            bail!(
                "C long-path staging cannot preserve #{name}; move this C source to a native-compatible path"
            );
        }
        if name != "include" {
            continue;
        }
        let value = value.trim_start();
        let (closing, system) = match value.as_bytes().first() {
            Some(b'"') => ('"', false),
            Some(b'<') => ('>', true),
            _ => bail!(
                "C long-path staging requires literal #include paths; dynamic includes require a native-compatible C source path"
            ),
        };
        let end = value[1..]
            .find(closing)
            .context("unterminated literal C include")?
            + 1;
        result.push(Include {
            name: value[1..end].into(),
            system,
        });
    }
    Ok(result)
}

fn spelling(path: &Path, backslashes: bool) -> String {
    let value = path.to_string_lossy();
    if backslashes {
        value.replace('/', "\\")
    } else {
        value.replace('\\', "/")
    }
}

// Native Windows argv uses the ANSI code page. Keep this logical UTF-8 file
// identity in an ASCII C literal so it survives that conversion losslessly.
fn c_string_literal_utf8(text: &str) -> String {
    let mut literal = String::from("\"");
    for byte in text.bytes() {
        match byte {
            b'"' => literal.push_str("\\\""),
            b'\\' => literal.push_str("\\\\"),
            b' '..=b'~' => literal.push(byte as char),
            _ => literal.push_str(&format!("\\{byte:03o}")),
        }
    }
    literal.push('"');
    literal
}

fn hash(path: &Path) -> Result<String> {
    Ok(format!("{:x}", Sha256::digest(fs::read(path)?)))
}

fn preserve_modified_time(original: &Path, copied: &Path) -> Result<()> {
    let modified = fs::metadata(original)?.modified()?;
    fs::OpenOptions::new()
        .write(true)
        .open(copied)?
        .set_times(fs::FileTimes::new().set_modified(modified))
        .with_context(|| {
            format!(
                "preserve C staging modification time for {}",
                original.display()
            )
        })
}

fn make_path_encoded(path: &Path, backslashes: bool, code_page: Option<u32>) -> Result<Vec<u8>> {
    let text = spelling(path, backslashes);
    let mut escaped = String::new();
    for ch in text.chars() {
        if matches!(ch, ' ' | '\t' | '#') {
            escaped.push('\\');
        }
        if ch == '$' {
            escaped.push('$');
        }
        escaped.push(ch);
    }
    #[cfg(windows)]
    if let Some(code_page) = code_page {
        return encode_native_dependency(&escaped, code_page)
            .with_context(|| format!("encode dependency path {}", path.display()));
    }
    let _ = code_page;
    Ok(escaped.into_bytes())
}

#[cfg(windows)]
fn native_code_page() -> u32 {
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn GetACP() -> u32;
    }
    unsafe { GetACP() }
}

#[cfg(windows)]
fn decode_native_dependency(bytes: &[u8], code_page: u32) -> Result<String> {
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn MultiByteToWideChar(
            code_page: u32,
            flags: u32,
            bytes: *const u8,
            bytes_len: i32,
            wide: *mut u16,
            wide_len: i32,
        ) -> i32;
    }
    if bytes.is_empty() {
        return Ok(String::new());
    }
    let length = i32::try_from(bytes.len())?;
    let size = unsafe {
        MultiByteToWideChar(code_page, 8, bytes.as_ptr(), length, std::ptr::null_mut(), 0)
    };
    ensure!(
        size > 0,
        "cannot decode native dependencies in Windows code page {code_page}: {}",
        std::io::Error::last_os_error()
    );
    let mut wide = vec![0u16; size as usize];
    let written = unsafe {
        MultiByteToWideChar(code_page, 8, bytes.as_ptr(), length, wide.as_mut_ptr(), size)
    };
    ensure!(written == size, "cannot decode native dependencies");
    String::from_utf16(&wide).context("native dependency contains invalid UTF-16")
}

#[cfg(windows)]
fn encode_native_dependency(text: &str, code_page: u32) -> Result<Vec<u8>> {
    use std::os::windows::ffi::OsStrExt;
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn WideCharToMultiByte(
            code_page: u32,
            flags: u32,
            wide: *const u16,
            wide_len: i32,
            bytes: *mut u8,
            bytes_len: i32,
            default: *const u8,
            used: *mut i32,
        ) -> i32;
    }
    let wide: Vec<_> = std::ffi::OsStr::new(text).encode_wide().collect();
    let mut used_default = 0;
    let (flags, used) = if code_page == 65001 {
        (0x80, std::ptr::null_mut()) // WC_ERR_INVALID_CHARS
    } else {
        (0x400, &mut used_default as *mut i32) // WC_NO_BEST_FIT_CHARS
    };
    let size = unsafe {
        WideCharToMultiByte(
            code_page,
            flags,
            wide.as_ptr(),
            wide.len().try_into()?,
            std::ptr::null_mut(),
            0,
            std::ptr::null(),
            used,
        )
    };
    ensure!(
        size > 0,
        "cannot encode dependency path in Windows code page {code_page}: {}",
        std::io::Error::last_os_error()
    );
    ensure!(
        used_default == 0,
        "dependency path cannot be represented in Windows code page {code_page}: {text}"
    );
    let mut bytes = vec![0; size as usize];
    let written = unsafe {
        WideCharToMultiByte(
            code_page,
            flags,
            wide.as_ptr(),
            wide.len().try_into()?,
            bytes.as_mut_ptr(),
            size,
            std::ptr::null(),
            used,
        )
    };
    ensure!(
        written == size && used_default == 0,
        "dependency path cannot be represented in Windows code page {code_page}: {text}"
    );
    Ok(bytes)
}

fn replace_bytes(input: &[u8], from: &[u8], to: &[u8]) -> Vec<u8> {
    if from.is_empty() {
        return input.to_vec();
    }
    let mut result = Vec::new();
    let mut remaining = input;
    while let Some(index) = remaining.windows(from.len()).position(|part| part == from) {
        result.extend_from_slice(&remaining[..index]);
        result.extend_from_slice(to);
        remaining = &remaining[index + from.len()..];
    }
    result.extend_from_slice(remaining);
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn planned_outputs_and_bundle_names_do_not_repeat_long_object_names() {
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("source.cpp");
        let output = temp.path().join(format!("{}.cpp.o", "name".repeat(40)));
        let workspace =
            CompileWorkspace::new(&temp.path().join("out"), &source, &output, "re2", &[]).unwrap();
        assert_eq!(workspace.object.file_name().unwrap(), "unit.o");
        assert_eq!(
            workspace.member_name(&workspace.object).unwrap(),
            output.file_name().unwrap().to_str().unwrap()
        );
        assert_eq!(
            workspace
                .member_name(&workspace.object.with_extension("rel"))
                .unwrap(),
            output
                .with_extension("rel")
                .file_name()
                .unwrap()
                .to_str()
                .unwrap()
        );
        assert!(
            output_paths(&output)
                .iter()
                .all(|path| path.as_os_str().len() < 40)
        );
        assert_eq!(raw_name(Path::new("stcxx_heap.c.o")), "stcxx_heap.c.o");
    }

    #[test]
    fn dependency_retargets_only_the_rule_and_metadata_keeps_original_inputs() {
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("source.cpp");
        fs::write(&source, b"source").unwrap();
        let output = temp.path().join("with space/long.cpp.o");
        let workspace =
            CompileWorkspace::new(&temp.path().join("out"), &source, &output, "re2", &[]).unwrap();
        let dependency = b"C:/stage/unit.o: source.cpp \\\n legacy\xb2\xe2.h\n";
        let rewritten = workspace.rewrite_dependency(dependency).unwrap();
        assert!(rewritten.ends_with(&dependency[15..]));
        assert!(rewritten.windows(6).any(|part| part == b"with\\ "));
        let mut meta = json!({"object":spelling(&workspace.object,false),"bitcode":format!("{}.stcxx.bc",spelling(&workspace.object,false)),"source":spelling(&source,false),"source_sha256":"old"});
        workspace.rewrite_metadata(&mut meta).unwrap();
        assert_eq!(meta["object"], spelling(&output, false));
        assert_eq!(meta["source_sha256"], hash(&source).unwrap());
    }

    #[test]
    fn literal_include_scanning_handles_comments_and_rejects_dynamic_search() {
        let includes=literal_includes(b"/* #include MACRO */\n# include \\\n \"../local.h\" // ignored\n#include <stdint.h>\n").unwrap();
        assert_eq!(includes.len(), 2);
        assert_eq!(includes[0].name, "../local.h");
        assert!(includes[1].system);
        assert!(literal_includes(b"#define H \"x.h\"\n#include H\n").is_err());
        assert!(literal_includes(b"#include_next <x.h>\n").is_err());
    }

    #[test]
    fn native_compatible_c_inputs_keep_dynamic_include_semantics() {
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("native.c");
        fs::write(&source, b"#define HEADER \"header.h\"\n#include HEADER\n").unwrap();
        let mut unused = temp.path().join("unused");
        while unused.as_os_str().len() < 300 {
            unused.push("long-unused-project-directory");
        }
        let flags = vec![format!("-I{}", unused.display())];
        let workspace = CompileWorkspace::new(
            &temp.path().join("out"),
            &source,
            &temp.path().join("native.c.o"),
            "re1",
            &flags,
        )
        .unwrap();
        assert_eq!(workspace.source, source);
        assert_eq!(workspace.flags, flags);
        assert!(workspace.snapshot_files.is_empty());
    }

    #[test]
    fn native_external_headers_retain_their_original_preprocessing() {
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("original/source.c");
        let includes = temp.path().join("include");
        fs::create_dir_all(source.parent().unwrap()).unwrap();
        fs::create_dir_all(&includes).unwrap();
        fs::write(&source, b"#include <external.h>\n").unwrap();
        // This header is compatible and must be left to SDCC, whose selected
        // conditional branches can legitimately contain nonliteral includes.
        fs::write(includes.join("external.h"), b"#include SOME_SDK_MACRO\n").unwrap();
        let flags = vec![format!("-I{}", includes.display())];
        let mut workspace = CompileWorkspace::new(
            &temp.path().join("out"),
            &source,
            &temp.path().join("source.c.o"),
            "re2",
            &flags,
        )
        .unwrap();
        workspace.snapshot_c(&temp.path().join("out")).unwrap();
        assert_eq!(&workspace.flags[..flags.len()], flags);
        assert!(
            workspace
                .flags
                .last()
                .unwrap()
                .starts_with("-D__BASE_FILE__=")
        );
        assert_eq!(workspace.snapshot_files.len(), 1);
    }

    #[test]
    fn partial_include_snapshots_preserve_original_search_positions_and_empty_roots() {
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("original/main.c");
        let includes = temp.path().join("include with space");
        let dynamic = temp.path().join("dynamic include with space");
        fs::create_dir_all(source.parent().unwrap()).unwrap();
        fs::create_dir_all(&includes).unwrap();
        fs::create_dir_all(&dynamic).unwrap();
        fs::write(&source, b"#include <captured.h>\n").unwrap();
        fs::write(includes.join("captured.h"), b"#define VALUE 1\n").unwrap();
        let flags = vec![
            format!("-I{}", includes.display()),
            format!("-I{}", dynamic.display()),
        ];
        let mut workspace = CompileWorkspace::new(
            &temp.path().join("out"),
            &source,
            &temp.path().join("main.c.o"),
            "re2",
            &flags,
        )
        .unwrap();
        workspace.snapshot_c(&temp.path().join("out")).unwrap();
        #[cfg(windows)]
        {
            assert!(workspace.flags[0].contains("src1"));
            assert_eq!(workspace.flags[1], flags[0]);
            assert_eq!(workspace.flags[2], flags[1]);
        }
        // Other hosts can read these external paths directly, so their
        // original include search roots remain in place.
        #[cfg(not(windows))]
        assert_eq!(&workspace.flags[..flags.len()], flags);
        assert!(!temp.path().join("out/src2").exists());
    }

    #[test]
    fn snapshots_and_renamed_main_sources_preserve_modification_times() {
        use std::time::{Duration, SystemTime};
        let temp = tempfile::tempdir().unwrap();
        let source_root = temp.path().join("original");
        fs::create_dir_all(&source_root).unwrap();
        let source = source_root.join(format!("{}.c", "n".repeat(230)));
        let header = source_root.join("local.h");
        fs::write(
            &source,
            b"#include \"local.h\"\nconst char *stamp = __TIMESTAMP__;\n",
        )
        .unwrap();
        fs::write(
            &header,
            b"static const char *header_stamp = __TIMESTAMP__;\n",
        )
        .unwrap();
        let modified = SystemTime::UNIX_EPOCH + Duration::from_secs(1_600_000_000);
        for file in [&source, &header] {
            fs::OpenOptions::new()
                .write(true)
                .open(file)
                .unwrap()
                .set_times(fs::FileTimes::new().set_modified(modified))
                .unwrap();
        }
        let mut workspace = CompileWorkspace::new(
            &temp.path().join("out"),
            &source,
            &temp.path().join("main.c.o"),
            "re2",
            &[],
        )
        .unwrap();
        workspace.snapshot_c(&temp.path().join("out")).unwrap();
        for copied in workspace.snapshot_files.keys() {
            assert_eq!(fs::metadata(copied).unwrap().modified().unwrap(), modified);
        }
        assert_eq!(
            fs::metadata(&workspace.source).unwrap().modified().unwrap(),
            modified
        );
        #[cfg(windows)]
        assert_ne!(workspace.source.file_name(), source.file_name());
    }

    #[test]
    fn staged_base_file_definition_preserves_user_overrides() {
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("original.c");
        fs::write(&source, b"const char *base = __BASE_FILE__;\n").unwrap();
        for (index, flags) in [
            vec!["-D__BASE_FILE__=\"custom.c\"".into()],
            vec!["-U__BASE_FILE__".into()],
            vec!["-D".into(), "__BASE_FILE__=\"custom.c\"".into()],
            vec!["-U".into(), "__BASE_FILE__".into()],
        ]
        .into_iter()
        .enumerate()
        {
            let directory = temp.path().join(format!("out-{index}"));
            let mut workspace = CompileWorkspace::new(
                &directory,
                &source,
                &temp.path().join("main.c.o"),
                "re2",
                &flags,
            )
            .unwrap();
            workspace.snapshot_c(&directory).unwrap();
            assert_eq!(workspace.flags, flags);
        }
    }

    #[test]
    fn base_file_literal_preserves_utf8_bytes_through_ascii_native_arguments() {
        let literal = c_string_literal_utf8("C:/é/中9\\\".c");
        assert!(literal.is_ascii());
        assert_eq!(literal, "\"C:/\\303\\251/\\344\\270\\2559\\\\\\\".c\"");
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("中文 source/main.c");
        fs::create_dir_all(source.parent().unwrap()).unwrap();
        fs::write(&source, b"const char *base = __BASE_FILE__;\n").unwrap();
        let directory = temp.path().join("out");
        let mut workspace = CompileWorkspace::new(
            &directory,
            &source,
            &temp.path().join("main.c.o"),
            "re2",
            &[],
        )
        .unwrap();
        workspace.snapshot_c(&directory).unwrap();
        assert_eq!(
            workspace.flags.last().unwrap(),
            &format!(
                "-D__BASE_FILE__={}",
                c_string_literal_utf8(&source.to_string_lossy())
            )
        );
        assert!(workspace.flags.last().unwrap().is_ascii());
        let content = fs::read_to_string(&workspace.source).unwrap();
        assert!(content.starts_with(&format!(
            "#line 1 {}\n",
            serde_json::to_string(&spelling(&source, false)).unwrap()
        )));
    }

    #[test]
    #[cfg(windows)]
    fn non_ansi_original_paths_transcode_the_entire_native_dependency_rule() {
        let staged = PathBuf::from("C:/stage/src/main.c");
        let original = PathBuf::from("C:/中文 project/main.c");
        let mut workspace = CompileWorkspace {
            source: staged.clone(),
            object: PathBuf::from("C:/stage/unit.o"),
            dependency: PathBuf::from("C:/stage/unit.d"),
            flags: Vec::new(),
            original_source: original.clone(),
            original_output: PathBuf::from("C:/中文 project/main.c.o"),
            native_dependencies: true,
            mappings: vec![(staged.clone(), original.clone())],
            snapshot_files: BTreeMap::from([(staged, original)]),
        };
        let dependency = b"C:/stage/unit.o: C:/stage/src/main.c C:/sdk/caf\xe9.h\nC:/sdk/caf\xe9.h:\n";
        let rewritten = workspace
            .rewrite_dependency_with_code_page(dependency, 1252)
            .unwrap();
        assert_eq!(
            std::str::from_utf8(&rewritten).unwrap(),
            "C:/中文\\ project/main.c.o: C:/中文\\ project/main.c C:/sdk/café.h\nC:/sdk/café.h:\n"
        );
        // Fully representable originals keep every untouched native byte.
        workspace.original_source = PathBuf::from("C:/original/main.c");
        workspace.original_output = PathBuf::from("C:/original/main.c.o");
        workspace.mappings[0].1 = workspace.original_source.clone();
        let native = workspace
            .rewrite_dependency_with_code_page(dependency, 1252)
            .unwrap();
        assert_eq!(
            native,
            b"C:/original/main.c.o: C:/original/main.c C:/sdk/caf\xe9.h\nC:/sdk/caf\xe9.h:\n"
        );
        assert!(decode_native_dependency(b"invalid\xff", 65001).is_err());
    }

    #[test]
    #[cfg(windows)]
    fn dependency_encoding_rejects_best_fit_and_keeps_clang_utf8() {
        assert!(encode_native_dependency("C:/Ł/🦀.o", 1252).is_err());
        assert_eq!(
            encode_native_dependency("C:/ascii.o", 1252).unwrap(),
            b"C:/ascii.o"
        );
        let unicode = "C:/unicode/🦀.o";
        assert_eq!(
            encode_native_dependency(unicode, 65001).unwrap(),
            unicode.as_bytes()
        );
        assert_eq!(
            make_path_encoded(Path::new(unicode), false, None).unwrap(),
            unicode.as_bytes()
        );
    }

    #[test]
    fn c_snapshot_keeps_parent_includes_file_macros_and_original_dependency_hashes() {
        let temp = tempfile::tempdir().unwrap();
        let source_root = temp.path().join("original");
        let source = source_root.join("child/source.c");
        fs::create_dir_all(source.parent().unwrap()).unwrap();
        fs::write(&source, b"#include \"local.h\"\n#include \"../parent.h\"\n").unwrap();
        let local = source.parent().unwrap().join("local.h");
        let parent = source_root.join("parent.h");
        fs::write(&local, b"static const char *local_name = __FILE__;\n").unwrap();
        fs::write(&parent, b"#define VALUE 1\n").unwrap();
        let mut workspace = CompileWorkspace::new(
            &temp.path().join("out"),
            &source,
            &temp.path().join("source.c.o"),
            "re2",
            &[],
        )
        .unwrap();
        workspace.snapshot_c(&temp.path().join("out")).unwrap();
        let copied_parent = workspace.source.parent().unwrap().join("../parent.h");
        assert!(copied_parent.is_file());
        let content =
            fs::read_to_string(workspace.source.parent().unwrap().join("local.h")).unwrap();
        assert!(content.starts_with(&format!(
            "#line 1 {}\n",
            serde_json::to_string(&spelling(&local, false)).unwrap()
        )));
        let copy = workspace.source.clone();
        let mut meta = json!({"source":spelling(&copy,false),"source_sha256":"snapshot","source_dependencies":{(spelling(&copy,false)):"snapshot"}});
        workspace.rewrite_metadata(&mut meta).unwrap();
        assert_eq!(meta["source"], spelling(&source, false));
        assert_eq!(
            meta["source_dependencies"][spelling(&source, false)],
            hash(&source).unwrap()
        );
        let dep = format!(
            "{}: {}\n",
            spelling(&workspace.object, false),
            spelling(&copy, false)
        );
        let rewritten =
            String::from_utf8(workspace.rewrite_dependency(dep.as_bytes()).unwrap()).unwrap();
        assert!(rewritten.contains(&spelling(&source, false)));
        assert!(!rewritten.contains("src0"));
    }
}
