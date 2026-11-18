// SPDX-License-Identifier: MIT
//! `recomp list` and `recomp extract`.

use rc_manifest::{Manifest, Treatment};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;
use std::time::Instant;

pub fn list(paths: &[PathBuf]) -> Result<ExitCode, String> {
    let [media] = paths else {
        return Err("list needs exactly one MEDIA".into());
    };
    let (kind, ar) = rc_media::open(media).map_err(|e| format!("{}: {e}", media.display()))?;
    let mut total = 0u64;
    for e in ar.entries() {
        println!("{:>12}  {}", e.size, e.path);
        total += e.size;
    }
    println!("{kind}: {} files, {total} bytes", ar.entries().len());
    Ok(ExitCode::SUCCESS)
}

/// The manifest variant a medium belongs to, by SHA-256.
fn find_variant<'a>(ms: &'a [Manifest], sha: &str) -> Option<(&'a Manifest, &'a str)> {
    ms.iter().find_map(|m| {
        m.variants
            .iter()
            .find(|v| v.media.iter().any(|md| md.sha256 == sha))
            .map(|v| (m, v.id.as_str()))
    })
}

fn is_empty_dir(p: &Path) -> bool {
    fs::read_dir(p)
        .map(|mut d| d.next().is_none())
        .unwrap_or(true)
}

/// Find `name` (ASCII case-insensitive) directly in `dir` or one level below.
fn find_file(dir: &Path, name: &str) -> Option<PathBuf> {
    let mut stack = vec![(dir.to_path_buf(), 0)];
    while let Some((d, depth)) = stack.pop() {
        for e in fs::read_dir(&d).ok()?.flatten() {
            let p = e.path();
            if p.is_dir() && depth < 1 {
                stack.push((p, depth + 1));
            } else if e.file_name().to_string_lossy().eq_ignore_ascii_case(name) {
                return Some(p);
            }
        }
    }
    None
}

pub fn extract(
    games: &Path,
    dest: Option<&Path>,
    force: bool,
    paths: &[PathBuf],
) -> Result<ExitCode, String> {
    let [media] = paths else {
        return Err("extract needs exactly one MEDIA".into());
    };
    let ms = rc_manifest::load_dir(games).map_err(|e| e.to_string())?;
    let t0 = Instant::now();
    let sha = rc_manifest::sha256_file(media).map_err(|e| format!("{}: {e}", media.display()))?;
    let variant = find_variant(&ms, &sha);

    let dest = match (dest, variant) {
        (Some(d), _) => d.to_path_buf(),
        (None, Some((m, v))) => games
            .parent()
            .unwrap_or(Path::new("."))
            .join("orig")
            .join(&m.game.id)
            .join(v),
        (None, None) => {
            return Err(format!(
                "{} is not in any manifest (unsupported build); pass --dest to unpack it anyway",
                media.display()
            ))
        }
    };
    if !force && dest.exists() && !is_empty_dir(&dest) {
        return Err(format!(
            "{} is not empty (use --force to extract into it anyway)",
            dest.display()
        ));
    }
    fs::create_dir_all(&dest).map_err(|e| format!("{}: {e}", dest.display()))?;

    match variant {
        Some((m, v)) => println!("{}: {}/{v} ({})", media.display(), m.game.id, m.game.title),
        None => println!("{}: not in any manifest", media.display()),
    }
    let (kind, mut ar) = rc_media::open(media).map_err(|e| format!("{}: {e}", media.display()))?;
    println!("{kind}, {} files → {}", ar.entries().len(), dest.display());
    let n = ar
        .extract(&dest, &|_| true)
        .map_err(|e| format!("extracting {}: {e}", media.display()))?;
    println!("{n} files written in {:.1}s", t0.elapsed().as_secs_f64());

    let Some((m, v)) = variant else {
        return Ok(ExitCode::SUCCESS);
    };
    // Verify every binary the manifest pins for this variant.
    let mut bad = 0;
    for b in m
        .binaries
        .iter()
        .filter(|b| b.variants.iter().any(|x| x == v))
    {
        let status = match find_file(&dest, &b.name) {
            None if b.treatment == Treatment::Ignore => continue,
            None => {
                bad += 1;
                "MISSING".to_string()
            }
            Some(p) => match rc_manifest::sha256_file(&p) {
                Ok(h) if h == b.sha256 => "ok".to_string(),
                Ok(h) => {
                    bad += 1;
                    format!("MISMATCH ({})", &h[..16])
                }
                Err(e) => {
                    bad += 1;
                    format!("unreadable: {e}")
                }
            },
        };
        println!(
            "  {:<20} {:<12} {status}",
            b.name,
            format!("{:?}", b.role).to_lowercase()
        );
    }
    if bad > 0 {
        eprintln!("{bad} pinned binaries missing or different");
        return Ok(ExitCode::from(1));
    }
    println!("all pinned binaries verified");
    Ok(ExitCode::SUCCESS)
}
