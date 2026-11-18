// SPDX-License-Identifier: MIT
//! A PE image as the discoverer sees it: sections mapped at their virtual addresses
//! (zero-filled beyond the raw data) plus the import address table.

use rc_pe::{ImportName, Pe};
use std::collections::BTreeMap;

pub struct Section {
    pub name: String,
    pub va: u32,
    pub size: u32,
    pub exec: bool,
    pub data: Vec<u8>,
}

pub struct Image {
    pub base: u32,
    pub entry: u32,
    pub sections: Vec<Section>,
    /// IAT slot VA → "dll!name" (lower-case DLL).
    pub imports: BTreeMap<u32, String>,
    /// Exported functions (VA, name).
    pub exports: Vec<(u32, String)>,
    pub is_dll: bool,
}

impl Image {
    pub fn new(file: &[u8], pe: &Pe) -> Self {
        let base = pe.image_base;
        let sections = pe
            .sections
            .iter()
            .map(|s| {
                let size = s.mapped_size();
                let mut data = vec![0u8; size as usize];
                let n = (s.raw_size.min(size)) as usize;
                let off = s.raw_offset as usize;
                if let Some(src) = file.get(off..off.saturating_add(n).min(file.len())) {
                    data[..src.len()].copy_from_slice(src);
                }
                Section {
                    name: s.name.clone(),
                    va: base.wrapping_add(s.virtual_address),
                    size,
                    exec: s.is_executable(),
                    data,
                }
            })
            .collect();
        let mut imports = BTreeMap::new();
        for d in &pe.imports {
            let dll = d.dll.to_ascii_lowercase();
            for f in &d.functions {
                let name = match &f.name {
                    ImportName::Name { name, .. } => name.clone(),
                    ImportName::Ordinal(o) => format!("#{o}"),
                };
                imports.insert(base.wrapping_add(f.iat_rva), format!("{dll}!{name}"));
            }
        }
        let exports = pe
            .exports
            .iter()
            .map(|e| {
                (
                    base.wrapping_add(e.rva),
                    e.name.clone().unwrap_or_else(|| format!("#{}", e.ordinal)),
                )
            })
            .collect();
        Image {
            base,
            entry: pe.entry_va(),
            sections,
            imports,
            exports,
            is_dll: pe.is_dll(),
        }
    }

    pub fn section_of(&self, va: u32) -> Option<&Section> {
        self.sections
            .iter()
            .find(|s| va >= s.va && va - s.va < s.size)
    }

    pub fn is_exec(&self, va: u32) -> bool {
        self.section_of(va).is_some_and(|s| s.exec)
    }

    /// Bytes from `va` to the end of its section.
    pub fn tail(&self, va: u32) -> Option<&[u8]> {
        let s = self.section_of(va)?;
        Some(&s.data[(va - s.va) as usize..])
    }

    pub fn byte(&self, va: u32) -> Option<u8> {
        self.tail(va).and_then(|t| t.first().copied())
    }

    pub fn dword(&self, va: u32) -> Option<u32> {
        let t = self.tail(va)?;
        Some(u32::from_le_bytes(t.get(..4)?.try_into().ok()?))
    }
}
