// SPDX-License-Identifier: MIT
//! `recomp template` and `recomp package`: packages that run without a compiler (rc-convert), as
//! TropicoRecomp's converter makes them.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use crate::native::{compile, programs};

/// `recomp template --game ID --system SYSTEM --shell PROGRAM -o OUT.zip`.
pub fn template(game: &str, system: &str, shell: &Path, out: &Path) -> Result<ExitCode, String> {
    let bytes = fs::read(shell).map_err(|e| format!("{}: {e}", shell.display()))?;
    let name = shell
        .file_name()
        .and_then(|n| n.to_str())
        .ok_or("--shell needs a file")?;
    let zip = rc_convert::make_template(game, system, name, &bytes)?;
    fs::write(out, zip).map_err(|e| format!("{}: {e}", out.display()))?;
    println!("template    {game} for {system}: {}", out.display());
    Ok(ExitCode::SUCCESS)
}

/// Every directory and file below `root`, '/'-separated, sorted.
fn walk(
    root: &Path,
    rel: &str,
    dirs: &mut Vec<String>,
    files: &mut Vec<(String, PathBuf)>,
) -> Result<(), String> {
    let mut entries: Vec<_> = fs::read_dir(root.join(rel))
        .map_err(|e| format!("{}: {e}", root.join(rel).display()))?
        .filter_map(Result::ok)
        .collect();
    entries.sort_by_key(|e| e.file_name());
    for e in entries {
        let name = e.file_name().to_string_lossy().into_owned();
        let path = if rel.is_empty() {
            name
        } else {
            format!("{rel}/{name}")
        };
        if e.path().is_dir() {
            dirs.push(path.clone());
            walk(root, &path, dirs, files)?;
        } else {
            files.push((path, e.path()));
        }
    }
    Ok(())
}

/// `recomp package --template T.zip [-o OUT.zip] INSTALL`: the install's programs compiled for the
/// template's targets, the template, and the install's files, as one zip.
pub fn package(
    games: &Path,
    template: &Path,
    out: Option<&Path>,
    install: &Path,
) -> Result<ExitCode, String> {
    let ms = rc_manifest::load_dir(games).map_err(|e| e.to_string())?;
    let (manifest, variant) = crate::lift::identify_dir(&ms, install)
        .ok_or_else(|| format!("{}: no known executable in it", install.display()))?;
    let id = manifest.game.id.clone();
    let tbytes = fs::read(template).map_err(|e| format!("{}: {e}", template.display()))?;
    let tpl = rc_convert::template(&tbytes)?;
    let files = crate::lift::liftable_in(&ms, install)?;
    let mut blobs = Vec::new();
    for (l, p) in programs(games, &files)? {
        let exe = l
            .known
            .as_ref()
            .map_or_else(String::new, |k| k.binary.name.to_ascii_lowercase());
        for &t in &tpl.targets {
            let blob = compile(t, &p)?;
            println!(
                "native      {exe} for {}: {:.1} MB",
                t.name(),
                blob.len() as f64 / 1e6
            );
            blobs.push((t, exe.clone(), blob));
        }
    }
    let mut dirs = Vec::new();
    let mut list = Vec::new();
    walk(install, "", &mut dirs, &mut list)?;
    let mut game_files: Vec<rc_convert::GameFile> = list
        .iter()
        .map(|(rel, path)| rc_convert::GameFile {
            path: rel.clone(),
            read: Box::new(move || fs::read(path).map_err(|e| format!("{}: {e}", path.display()))),
        })
        .collect();
    let out = match out {
        Some(o) => o.to_path_buf(),
        None => {
            let d = games
                .parent()
                .unwrap_or(Path::new("."))
                .join("out")
                .join(&id);
            fs::create_dir_all(&d).map_err(|e| format!("{}: {e}", d.display()))?;
            d.join(format!(
                "{}-{variant}-{}.zip",
                rc_convert::stem(&id),
                tpl.targets[0].name()
            ))
        }
    };
    let f = fs::File::create(&out).map_err(|e| format!("{}: {e}", out.display()))?;
    let w = std::io::BufWriter::new(f);
    let n = game_files.len();
    rc_convert::package(w, &tpl, &blobs, &dirs, &mut game_files, None, &mut |_| {})?;
    println!(
        "package     {id}/{variant}, {n} game files → {}",
        out.display()
    );
    Ok(ExitCode::SUCCESS)
}
