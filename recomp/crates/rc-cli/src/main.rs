// SPDX-License-Identifier: MIT
//! `recomp`: the Cultures recompiler command line.
//!
//! Implemented: `identify`, `info`, `list`, `extract`. Planned (see docs/PLAN.md):
//! `discover` (M2), `lift` (M3/M4), `convert` (M14).

mod media;

use rc_manifest::{Catalog, Hit};
use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const USAGE: &str = "\
recomp - static recompiler for the Cultures games

USAGE:
    recomp identify [--games DIR] PATH...   match files (installers, ISO, binaries) by SHA-256
    recomp info [--games DIR] [--imports] FILE
                                            show PE facts and manifest matches for one file
    recomp list MEDIA                       list the files of an installer, CD image or zip
    recomp extract [--games DIR] [--dest DIR] [--force] MEDIA
                                            unpack MEDIA (default: orig/<game>/<variant>/) and
                                            verify the binaries against the manifest
    recomp help | --help
    recomp version | --version

The manifests are read from --games DIR, $RC_GAMES_DIR, or the nearest games/ directory.";

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match run(&args) {
        Ok(code) => code,
        Err(e) => {
            eprintln!("error: {e}");
            ExitCode::from(2)
        }
    }
}

struct Opts {
    games: PathBuf,
    imports: bool,
    dest: Option<PathBuf>,
    force: bool,
    paths: Vec<PathBuf>,
}

fn parse_opts(args: &[String]) -> Result<Opts, String> {
    let mut o = Opts {
        games: rc_manifest::default_games_dir(),
        imports: false,
        dest: None,
        force: false,
        paths: Vec::new(),
    };
    let mut it = args.iter();
    while let Some(a) = it.next() {
        match a.as_str() {
            "--games" => o.games = it.next().ok_or("--games needs a directory")?.into(),
            "--imports" => o.imports = true,
            "--dest" => o.dest = Some(it.next().ok_or("--dest needs a directory")?.into()),
            "--force" => o.force = true,
            s if s.starts_with("--") => return Err(format!("unknown option {s}")),
            p => o.paths.push(p.into()),
        }
    }
    Ok(o)
}

fn run(args: &[String]) -> Result<ExitCode, String> {
    let Some((cmd, rest)) = args.split_first() else {
        println!("{USAGE}");
        return Ok(ExitCode::from(2));
    };
    match cmd.as_str() {
        "identify" => identify(parse_opts(rest)?),
        "info" => info(parse_opts(rest)?),
        "list" => {
            let o = parse_opts(rest)?;
            media::list(&o.paths)
        }
        "extract" => {
            let o = parse_opts(rest)?;
            media::extract(&o.games, o.dest.as_deref(), o.force, &o.paths)
        }
        "help" | "--help" | "-h" => {
            println!("{USAGE}");
            Ok(ExitCode::SUCCESS)
        }
        "version" | "--version" | "-V" => {
            println!("recomp {}", env!("CARGO_PKG_VERSION"));
            Ok(ExitCode::SUCCESS)
        }
        other => Err(format!("unknown command {other:?} (try `recomp help`)")),
    }
}

fn load_catalog(games: &Path) -> Result<Catalog, String> {
    let ms = rc_manifest::load_dir(games).map_err(|e| e.to_string())?;
    Ok(Catalog::new(&ms))
}

/// Recursively collect regular files, sorted, without following symlinks.
fn walk(path: &Path, out: &mut Vec<PathBuf>) -> Result<(), String> {
    let meta = fs::symlink_metadata(path).map_err(|e| format!("{}: {e}", path.display()))?;
    if meta.is_file() {
        out.push(path.to_owned());
    } else if meta.is_dir() {
        let mut entries: Vec<_> = fs::read_dir(path)
            .map_err(|e| format!("{}: {e}", path.display()))?
            .filter_map(Result::ok)
            .map(|e| e.path())
            .collect();
        entries.sort();
        for e in entries {
            walk(&e, out)?;
        }
    }
    Ok(())
}

fn has_ext(p: &Path, exts: &[&str]) -> bool {
    p.extension()
        .and_then(|e| e.to_str())
        .is_some_and(|e| exts.iter().any(|x| e.eq_ignore_ascii_case(x)))
}

/// Store and installer extras that differ per release and are never used by the games
/// (GOG Galaxy glue, GOG's Ogg/Vorbis CD-audio wrapper dependencies, uninstallers).
/// They are skipped instead of being reported as unknown. Prefixes, lower case.
const IGNORED_PREFIXES: &[&str] = &[
    "goggame-",
    "gameuxinstallhelper",
    "webcache.zip",
    "libogg-",
    "libvorbis",
    "unins",
];

fn is_ignored_extra(p: &Path) -> bool {
    p.file_name()
        .and_then(|n| n.to_str())
        .map(str::to_ascii_lowercase)
        .is_some_and(|n| IGNORED_PREFIXES.iter().any(|pre| n.starts_with(pre)))
}

fn identify(o: Opts) -> Result<ExitCode, String> {
    if o.paths.is_empty() {
        return Err("identify needs at least one PATH".into());
    }
    let cat = load_catalog(&o.games)?;
    let mut files = Vec::new();
    for p in &o.paths {
        walk(p, &mut files)?;
    }

    let mut matched = 0usize;
    let mut unknown = 0usize;
    // game -> variant -> what was found for it
    let mut found: BTreeMap<String, BTreeMap<String, Vec<String>>> = BTreeMap::new();
    for f in &files {
        let size = fs::metadata(f)
            .map_err(|e| format!("{}: {e}", f.display()))?
            .len();
        let candidate = has_ext(f, &["exe", "dll", "asi", "iso", "zip"]);
        if !cat.is_known_size(size) && !candidate {
            continue;
        }
        let hash = rc_manifest::sha256_file(f).map_err(|e| format!("{}: {e}", f.display()))?;
        let hits = cat.lookup(&hash);
        if hits.is_empty() {
            if candidate && !is_ignored_extra(f) {
                unknown += 1;
                println!("unknown  {}  {}", &hash[..16], f.display());
            }
            continue;
        }
        matched += 1;
        for h in hits {
            println!("match    {h}  {}", f.display());
            match h {
                Hit::Media { game, variant, .. } => found
                    .entry(game.clone())
                    .or_default()
                    .entry(variant.clone())
                    .or_default()
                    .push("installer/media".into()),
                Hit::Binary {
                    game,
                    variants,
                    name,
                    role,
                    ..
                } => {
                    for v in variants {
                        found
                            .entry(game.clone())
                            .or_default()
                            .entry(v.clone())
                            .or_default()
                            .push(format!("{name} ({role})"));
                    }
                }
            }
        }
    }

    println!();
    println!(
        "{} files scanned, {matched} recognised, {unknown} unknown executables",
        files.len()
    );
    for (game, variants) in &found {
        for (variant, what) in variants {
            println!("  {game}/{variant}: {}", what.join(", "));
        }
    }
    Ok(if matched > 0 {
        ExitCode::SUCCESS
    } else {
        ExitCode::from(1)
    })
}

/// Days since 1970-01-01 → (year, month, day), proleptic Gregorian (H. Hinnant's algorithm).
fn civil_from_days(z: i64) -> (i64, u32, u32) {
    let z = z + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z.rem_euclid(146_097);
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = (doy - (153 * mp + 2) / 5 + 1) as u32;
    let m = if mp < 10 { mp + 3 } else { mp - 9 } as u32;
    let y = yoe + era * 400 + i64::from(m <= 2);
    (y, m, d)
}

fn fmt_timestamp(t: u32) -> String {
    let secs = i64::from(t);
    let (y, m, d) = civil_from_days(secs.div_euclid(86_400));
    let s = secs.rem_euclid(86_400);
    format!(
        "{y:04}-{m:02}-{d:02} {:02}:{:02}:{:02} UTC",
        s / 3600,
        s / 60 % 60,
        s % 60
    )
}

fn info(o: Opts) -> Result<ExitCode, String> {
    let [file] = o.paths.as_slice() else {
        return Err("info needs exactly one FILE".into());
    };
    let data = fs::read(file).map_err(|e| format!("{}: {e}", file.display()))?;
    let hash = rc_manifest::sha256_hex(&data);
    println!("file        {}", file.display());
    println!("size        {}", data.len());
    println!("sha256      {hash}");
    match load_catalog(&o.games) {
        Ok(cat) => {
            let hits = cat.lookup(&hash);
            if hits.is_empty() {
                println!("manifest    not listed (unsupported build)");
            }
            for h in hits {
                println!("manifest    {h}");
            }
        }
        Err(e) => println!("manifest    unavailable: {e}"),
    }

    let pe = match rc_pe::parse(&data) {
        Ok(pe) => pe,
        Err(e) => {
            println!("pe          not a PE32 image: {e}");
            return Ok(ExitCode::SUCCESS);
        }
    };
    println!("timestamp   {}", fmt_timestamp(pe.timestamp));
    println!(
        "linker      {}.{:02}",
        pe.linker_version.0, pe.linker_version.1
    );
    println!(
        "kind        {}, subsystem {}",
        if pe.is_dll() { "dll" } else { "exe" },
        pe.subsystem
    );
    println!("image base  {:#010x}", pe.image_base);
    println!("entry       {:#010x}", pe.entry_va());
    println!(
        "relocations {}",
        if pe.relocs_stripped() {
            "none (fixed base)"
        } else {
            "present"
        }
    );
    match &pe.rich {
        Some(r) => {
            let ids: Vec<_> = r
                .iter()
                .map(|e| format!("{}.{}x{}", e.product, e.build, e.count))
                .collect();
            println!("rich        {}", ids.join(" "));
        }
        None => println!("rich        none"),
    }
    println!("sections");
    for s in &pe.sections {
        println!(
            "  {:8} va {:#010x} vsize {:#08x} raw {:#08x}{}",
            s.name,
            pe.image_base.wrapping_add(s.virtual_address),
            s.virtual_size,
            s.raw_size,
            if s.is_executable() { "  code" } else { "" }
        );
    }
    println!("imports     {} functions", pe.import_count());
    for d in &pe.imports {
        if o.imports {
            let names: Vec<_> = d.functions.iter().map(|f| f.name.to_string()).collect();
            println!("  {}: {}", d.dll, names.join(" "));
        } else {
            println!("  {:16} {}", d.dll, d.functions.len());
        }
    }
    if !pe.exports.is_empty() {
        println!("exports     {}", pe.exports.len());
    }
    Ok(ExitCode::SUCCESS)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn store_extras_are_ignored() {
        assert!(is_ignored_extra(Path::new("app/goggame-1207663793.dll")));
        assert!(is_ignored_extra(Path::new("GameuxInstallHelper.dll")));
        assert!(is_ignored_extra(Path::new("libvorbisfile-3.dll")));
        assert!(!is_ignored_extra(Path::new("Cultures.exe")));
        assert!(!is_ignored_extra(Path::new("winmm.dll")));
    }

    #[test]
    fn timestamps() {
        assert_eq!(fmt_timestamp(0), "1970-01-01 00:00:00 UTC");
        assert_eq!(fmt_timestamp(1_000_000_000), "2001-09-09 01:46:40 UTC");
        assert_eq!(civil_from_days(11_016), (2000, 2, 29));
    }
}
