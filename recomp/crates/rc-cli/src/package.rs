// SPDX-License-Identifier: MIT
//! `recomp template` and `recomp package`: packages that run without a compiler (rc-convert), as
//! TropicoRecomp's converter makes them.

use std::fs;
use std::path::Path;
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
                "{}-{variant}-{}.{}",
                rc_convert::stem(&id),
                tpl.targets[0].name(),
                if tpl.apk { "apk" } else { "zip" }
            ))
        }
    };
    let f = fs::File::create(&out).map_err(|e| format!("{}: {e}", out.display()))?;
    let w = std::io::BufWriter::new(f);
    let mut n = 0;
    rc_convert::package(
        w,
        &tpl,
        &blobs,
        &mut |sink| rc_convert::folder_files(install, sink),
        None,
        &mut |_| n += 1,
    )?;
    println!(
        "package     {id}/{variant}, {n} game files → {}",
        out.display()
    );
    Ok(ExitCode::SUCCESS)
}
