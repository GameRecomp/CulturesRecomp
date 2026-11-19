// SPDX-License-Identifier: MIT
//! `recomp convert`: the user's installer, CD image, zip or installed folder to a program, in
//! one step: identify, unpack (to `orig/<game>/<variant>/`), lift every binary the manifest
//! lifts, build the game's shell with CMake, and put it with a launcher into `out/<game>/`.
//! The program contains code derived from the user's copy: it stays on their machine.

use crate::{lift, media};
use rc_manifest::{Manifest, Treatment};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

pub struct ConvertOpts<'a> {
    pub games: &'a Path,
    pub preset: &'a str,
    pub force: bool,
}

/// `name` in `dir`, matched without case.
fn find_file(dir: &Path, name: &str) -> Option<PathBuf> {
    fs::read_dir(dir)
        .ok()?
        .filter_map(Result::ok)
        .map(|e| e.path())
        .find(|p| {
            p.file_name()
                .and_then(|n| n.to_str())
                .is_some_and(|n| n.eq_ignore_ascii_case(name))
        })
}

/// The game and variant an installed folder holds, by the hash of its main executable.
fn identify_dir<'a>(ms: &'a [Manifest], dir: &Path) -> Option<(&'a Manifest, String)> {
    for m in ms {
        for b in &m.binaries {
            let Some(p) = find_file(dir, &b.name) else {
                continue;
            };
            let Ok(sha) = rc_manifest::sha256_file(&p) else {
                continue;
            };
            if sha == b.sha256 {
                if let Some(v) = b.variants.first() {
                    return Some((m, v.clone()));
                }
            }
        }
    }
    None
}

fn run(cmd: &mut Command) -> Result<(), String> {
    println!("+ {cmd:?}");
    let st = cmd
        .status()
        .map_err(|e| format!("{:?}: {e}", cmd.get_program()))?;
    if st.success() {
        Ok(())
    } else {
        Err(format!("{:?} failed ({st})", cmd.get_program()))
    }
}

pub fn convert(o: &ConvertOpts, paths: &[PathBuf]) -> Result<ExitCode, String> {
    let [input] = paths else {
        return Err(
            "convert needs exactly one installer, CD image, zip or installed folder".into(),
        );
    };
    let ms = rc_manifest::load_dir(o.games).map_err(|e| e.to_string())?;
    let root = o.games.parent().unwrap_or(Path::new(".")).to_path_buf();

    // 1. Which game and variant, and its files.
    let (manifest, variant, dir) = if input.is_dir() {
        let (m, v) = identify_dir(&ms, input)
            .ok_or_else(|| format!("{}: no known executable in it", input.display()))?;
        (m, v, input.clone())
    } else {
        let sha =
            rc_manifest::sha256_file(input).map_err(|e| format!("{}: {e}", input.display()))?;
        let (m, v) = media::find_variant(&ms, &sha).ok_or_else(|| {
            format!(
                "{} is not a supported installer, CD or zip",
                input.display()
            )
        })?;
        let dir = root.join("orig").join(&m.game.id).join(v);
        if o.force || !dir.exists() || media::is_empty_dir(&dir) {
            let _ = media::extract(o.games, None, true, std::slice::from_ref(input))?;
        } else {
            println!("{}: already unpacked", dir.display());
        }
        (m, v.to_string(), dir)
    };
    let id = &manifest.game.id;
    println!("game        {} ({id}/{variant})", manifest.game.title);

    // 2. Lift what the manifest lifts and the folder has.
    let mut lifted = Vec::new();
    for b in manifest
        .binaries
        .iter()
        .filter(|b| b.treatment == Treatment::Lift && b.variants.contains(&variant))
    {
        if let Some(p) = find_file(&dir, &b.name) {
            if lifted.contains(&p) {
                continue;
            }
            let _ = lift::lift(
                &lift::LiftOpts {
                    games: o.games,
                    out: None,
                    base: None,
                    comments: false,
                    verbose: false,
                },
                std::slice::from_ref(&p),
            )?;
            lifted.push(p);
        }
    }
    if lifted.is_empty() {
        return Err(format!("{}: nothing to lift", dir.display()));
    }

    // 3. Build the game's program.
    let target = format!("cultures-{id}");
    run(Command::new("cmake")
        .current_dir(&root)
        .args(["--preset", o.preset]))?;
    run(Command::new("cmake")
        .current_dir(&root)
        .args(["--build", "--preset", o.preset, "--parallel", "--target", &target]))?;
    let built = root
        .join("build")
        .join(o.preset)
        .join("games")
        .join(id)
        .join(&target);
    let built = if built.exists() {
        built
    } else {
        built.with_extension("exe")
    };

    // 4. The program and a launcher that points it at the game's files.
    let out = root.join("out").join(id);
    fs::create_dir_all(&out).map_err(|e| format!("{}: {e}", out.display()))?;
    let dest = out.join(built.file_name().unwrap_or_default());
    fs::copy(&built, &dest).map_err(|e| format!("{}: {e}", built.display()))?;
    let game_dir = fs::canonicalize(&dir).unwrap_or(dir);
    if cfg!(windows) {
        let bat = out.join("play.bat");
        let text = format!(
            "@echo off\r\n\"%~dp0{}\" --game-dir \"{}\" %*\r\n",
            target,
            game_dir.display()
        );
        fs::write(&bat, text).map_err(|e| e.to_string())?;
    } else {
        let sh = out.join("play.sh");
        let text = format!(
            "#!/bin/sh\n# Runs {} with the game's files from where they were unpacked.\nexec \"$(dirname \"$0\")/{}\" --game-dir \"{}\" \"$@\"\n",
            manifest.game.title,
            target,
            game_dir.display()
        );
        fs::write(&sh, text).map_err(|e| e.to_string())?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let _ = fs::set_permissions(&sh, fs::Permissions::from_mode(0o755));
        }
    }
    println!(
        "ready       {} (start it with {})",
        dest.display(),
        out.join(if cfg!(windows) { "play.bat" } else { "play.sh" })
            .display()
    );
    Ok(ExitCode::SUCCESS)
}
