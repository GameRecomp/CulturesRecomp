// SPDX-License-Identifier: MIT
//! Packages that run without a compiler, as TropicoRecomp's converter makes them: a shell template
//! (the game's program without lifted code, built by CI for a system) with the game's native code
//! and its files in the template's `data/` directory.
//!
//! - [`make_template`]: a shell and the targets it runs, as a template zip.
//! - [`template`]: read one.
//! - [`package`]: the template's files, `data/code-<target>-<exe>.bin` for each program and target,
//!   and the game's files under `data/game/`.
//!
//! Nothing here touches the file system: the caller reads and writes, so the crate can run in a
//! browser later as TropicoRecomp's does.

pub mod zip;

use std::io::Write;

use rc_emit_native::Target;

pub type Error = String;

/// The target names a template lists for each system (TropicoRecomp's).
pub fn default_targets(system: &str) -> Option<&'static [&'static str]> {
    match system {
        "windows" => Some(&["x86_64-windows"]),
        "linux" => Some(&["x86_64-sysv"]),
        "macos" => Some(&["arm64-apple", "x86_64-sysv"]),
        "android" => Some(&["arm64-linux", "x86_64-sysv"]),
        _ => None,
    }
}

/// How a game's packages are named: the folder (and file stem) in the zip.
pub fn stem(game: &str) -> &'static str {
    match game {
        "cultures1" => "Cultures",
        "cultures2" => "Cultures2",
        "northland" => "Northland",
        "eighthwonder" => "EighthWonder",
        "saga" => "CulturesSaga",
        _ => "Game",
    }
}

/// A template: the shell `shell_name` (its bytes) and the targets it runs, in `<stem>/`.
pub fn make_template(
    game: &str,
    system: &str,
    shell_name: &str,
    shell: &[u8],
) -> Result<Vec<u8>, Error> {
    let targets = default_targets(system).ok_or_else(|| format!("unknown system {system:?}"))?;
    let io = |e: std::io::Error| e.to_string();
    let root = format!("{}/", stem(game));
    let mut z = zip::ZipWriter::new(Vec::new());
    z.dir(&root).map_err(io)?;
    z.file(&format!("{root}{shell_name}"), 0o755, shell)
        .map_err(io)?;
    z.dir(&format!("{root}data/")).map_err(io)?;
    z.file(
        &format!("{root}data/targets"),
        0o644,
        format!("{}\n", targets.join(" ")).as_bytes(),
    )
    .map_err(io)?;
    z.finish().map_err(io)
}

/// A read template.
#[derive(Debug)]
pub struct Template<'a> {
    pub entries: Vec<zip::StoredEntry<'a>>,
    /// The `data/` directory's path in the zip, ending in `/`.
    pub data: String,
    pub targets: Vec<Target>,
}

/// A target by its name in a template's `targets`.
pub fn target_named(name: &str) -> Option<Target> {
    [
        Target::X64SysV,
        Target::X64Windows,
        Target::Arm64Linux,
        Target::Arm64Apple,
    ]
    .into_iter()
    .find(|t| t.name() == name)
}

pub fn template(bytes: &[u8]) -> Result<Template<'_>, Error> {
    let entries = zip::read_stored(bytes)?;
    let data = entries
        .iter()
        .find(|e| e.is_dir && (e.name == "data/" || e.name.ends_with("/data/")))
        .map(|e| e.name.clone())
        .ok_or("shell template: no data/ directory")?;
    let list = entries
        .iter()
        .find(|e| e.name == format!("{data}targets"))
        .ok_or("shell template: no data/targets")?;
    let targets = String::from_utf8_lossy(&list.data)
        .split_whitespace()
        .map(|t| target_named(t).ok_or_else(|| format!("shell template: target {t}")))
        .collect::<Result<Vec<_>, _>>()?;
    if targets.is_empty() {
        return Err("shell template: no targets".into());
    }
    Ok(Template {
        entries,
        data,
        targets,
    })
}

/// One game file for a package: its path below the game's folder ('/'-separated) and a way to
/// read it.
pub struct GameFile<'a> {
    pub path: String,
    pub read: Box<dyn FnMut() -> Result<Vec<u8>, Error> + 'a>,
}

/// Write the package: the template's files (but `targets`), `data/code-<target>-<exe>.bin` for
/// each blob (`blobs`: target, executable's lower-case name, bytes), then the game's files under
/// `data/game/`. `progress` hears each file written.
pub fn package<W: Write>(
    out: W,
    tpl: &Template<'_>,
    blobs: &[(Target, String, Vec<u8>)],
    dirs: &[String],
    files: &mut [GameFile<'_>],
    progress: &mut dyn FnMut(&str),
) -> Result<W, Error> {
    let io = |e: std::io::Error| e.to_string();
    let mut z = zip::ZipWriter::new(out);
    for e in &tpl.entries {
        if e.name == format!("{}targets", tpl.data) {
            continue;
        }
        if e.is_dir {
            z.dir(&e.name).map_err(io)?;
        } else {
            z.file(&e.name, e.mode, &e.data).map_err(io)?;
        }
    }
    for (t, exe, blob) in blobs {
        z.file(
            &format!("{}code-{}-{exe}.bin", tpl.data, t.name()),
            0o644,
            blob,
        )
        .map_err(io)?;
    }
    let game = format!("{}game/", tpl.data);
    z.dir(&game).map_err(io)?;
    for d in dirs {
        z.dir(&format!("{game}{d}/")).map_err(io)?;
    }
    for f in files.iter_mut() {
        let data = (f.read)()?;
        z.file(&format!("{game}{}", f.path), 0o644, &data)
            .map_err(io)?;
        progress(&f.path);
    }
    z.finish().map_err(io)
}
