// SPDX-License-Identifier: MIT
//! Code discovery for the Cultures recompiler (milestone M2, in progress).
//!
//! So far this crate provides the image model the discoverer works on: sections mapped
//! at their virtual addresses, the import address table and the exports.

mod image;

pub use image::{Image, Section};
