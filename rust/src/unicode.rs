//! Correct unicode handling.
//!
//! Correct unicode is hard, I'll spare you the gory details...
//! This module adds a slight runtime speed cost and a considerable binary size cost.
//! Uses the crates [`unicode-normalization`] and [`caseless`].
//!
//! In javascript [`String.prototype.normalize()`] defaults to NFC so we use it too.
//!
//! We use the default `String` order, which is probably not correct unicode order.
//!
//!
//! # Normalization
//!
//! Two unicode strings with different bytes can be considered equal.
//! Normalizing makes them have the same bytes as long as the same normalization form is used.
//!
//! We use [canonical equivalence] to ensure characters have the same visual appearance and behavior.
//!
//! There is a runtime space-time tradeoff between the two canonical [normalization forms]:
//!  * NFD = more string space, less normalization time (decomposition)
//!  * NFC = less string space, more normalization time (decomposition + composition)
//!
//!
//! # Partial strings
//!
//! A concatenated string must be normalized even if the original strings were normalized.
//!
//! I'm not sure, but utf8 substrings are probably normalized if the original was normalized.
//!
//!
//! # Ordering
//!
//! Should depend on the locale.
//!
//!
//! # Case sensitive
//!
//! Compare normalized strings.
//!
//!
//! # Case insensitive
//!
//! Compare caseless normalized strings.
//!
//! To get a caseless string we must normalize, then get a caseless representation with [case folding] and normalize again.
//!
//!
//! [`caseless`]: https://crates.io/crates/caseless
//! [`unicode-normalization`]: https://crates.io/crates/unicode-normalization
//! [`String.prototype.normalize()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/normalize
//! [canonical equivalence]: http://www.unicode.org/reports/tr15/#Canon_Compat_Equivalence
//! [normalization forms]: http://www.unicode.org/reports/tr15/#Norm_Forms
//! [case folding]: https://www.w3.org/International/wiki/Case_folding
#![allow(dead_code)]

use std::fmt;
use std::ops;

use caseless::Caseless;
use unicode_normalization::{is_nfc, UnicodeNormalization};

/// A unicode string normalized with NFC.
///
/// Can be used transparently as a `&str`.
/// The inner `String` is private to ensure it remains normalized.
/// Uses default `String` order, which is probably not correct unicode order.
#[derive(Clone, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub struct Nfc {
    inner: String,
}

impl Nfc {
    /// Creates a normalized string.
    pub fn from_str(s: &str) -> Self {
        if is_nfc(s) {
            let string = s.to_owned();
            Self { inner: string }
        } else {
            let string = s.chars().nfc().collect();
            Self { inner: string }
        }
    }

    /// Creates a normalized caseless string.
    pub fn caseless(s: &str) -> Self {
        let string = s.chars().nfc().default_case_fold().nfc().collect();
        Self { inner: string }
    }

    /// Creates a normalized path string.
    /// Converts '\\' to '/'.
    pub fn path(s: &str) -> Self {
        if is_nfc(s) && !s.contains('\\') {
            let string = s.to_owned();
            Self { inner: string }
        } else {
            let string = s
                .chars()
                .map(|x| if x == '\\' { '/' } else { x })
                .nfc()
                .collect();
            Self { inner: string }
        }
    }

    /// Creates a normalized caseless path string.
    /// Converts '\\' to '/'.
    pub fn caseless_path(s: &str) -> Self {
        let string = s
            .chars()
            .map(|x| if x == '\\' { '/' } else { x })
            .nfc()
            .default_case_fold()
            .nfc()
            .collect();
        Self { inner: string }
    }

    /// Match `String::as_str()`.
    pub fn as_str(&self) -> &str {
        &self.inner
    }
}

impl AsRef<[u8]> for Nfc {
    fn as_ref(&self) -> &[u8] {
        self.inner.as_bytes()
    }
}

impl AsRef<str> for Nfc {
    fn as_ref(&self) -> &str {
        &self.inner
    }
}

/// Converts to a normalized string.
impl From<&str> for Nfc {
    fn from(s: &str) -> Self {
        Nfc::from_str(s)
    }
}

/// Converts to a normalized string.
/// Consumes the original string.
impl From<String> for Nfc {
    fn from(string: String) -> Self {
        if is_nfc(&string) {
            Self { inner: string }
        } else {
            let string = string.chars().nfc().collect();
            Self { inner: string }
        }
    }
}

/// Unwraps the inner string.
impl Into<String> for Nfc {
    fn into(self) -> String {
        self.inner
    }
}

/// Inherits all the methods of `str`.
impl ops::Deref for Nfc {
    type Target = str;

    fn deref(&self) -> &str {
        &self.inner
    }
}

/// Adds a string.
impl ops::Add<&str> for Nfc {
    type Output = Self;

    fn add(self, other: &str) -> Self {
        let string = self.inner.chars().chain(other.chars()).nfc().collect();
        Self { inner: string }
    }
}

/// Matches the inner string.
impl fmt::Display for Nfc {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let display: &fmt::Display = &self.inner;
        display.fmt(f)
    }
}

/// Matches the inner string.
impl fmt::Debug for Nfc {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let debug: &fmt::Debug = &self.inner;
        debug.fmt(f)
    }
}

#[cfg(test)]
mod tests {
    use super::Nfc;

    // from the caseless crate

    macro_rules! nfc_caseless {
        ($input: expr, $expected: expr) => {
            assert_eq!(Nfc::caseless($input).as_str(), $expected);
        };
    }

    #[test]
    fn caseless() {
        nfc_caseless!("Test Case", "test case");
        nfc_caseless!("Teſt Caſe", "test case");
        nfc_caseless!("spiﬃest", "spiffiest");
        nfc_caseless!("straße", "strasse");
    }

    // from http://www.unicode.org/reports/tr15/

    macro_rules! nfc_eq {
        ($left: expr, $right: expr) => {
            assert_eq!(Nfc::from_str($left), Nfc::from_str($right));
        };
    }

    macro_rules! nfc {
        ($input: expr, $expected: expr) => {
            assert_eq!(Nfc::from_str($input).as_str(), $expected);
        };
    }

    macro_rules! nfc_add {
        ($left: expr, $right: expr, $expected: expr) => {
            assert_eq!((Nfc::from_str($left) + $right).as_str(), $expected);
        };
    }

    #[test]
    fn examples_of_canonical_equivalence() {
        // Figure 1. Examples of Canonical Equivalence
        nfc_eq!("\u{00C7}", "\u{0043}\u{0327}");
        nfc_eq!("\u{0071}\u{0307}\u{0323}", "\u{0071}\u{0323}\u{0307}");
        nfc_eq!("\u{AC00}", "\u{1100}\u{1161}");

        // NOTE this had U+03A9 GREEK CAPITAL LETTER OMEGA on both sides
        //      I replaced one with U+2126 OHM SIGN
        nfc_eq!("\u{03A9}", "\u{2126}");
    }

    #[test]
    fn singletons() {
        // Figure 3. Singletons
        nfc!("\u{212B}", "\u{00C5}");
        nfc!("\u{2126}", "\u{03A9}");
    }

    #[test]
    fn canonical_composites() {
        // Figure 4. Canonical Composites
        nfc!("\u{00C5}", "\u{00C5}");
        nfc!("\u{00F4}", "\u{00F4}");
    }

    #[test]
    fn multiple_combining_marks() {
        // Figure 5. Multiple Combining Marks
        nfc!("\u{1E69}", "\u{1E69}");
        nfc!("\u{1E0B}\u{0323}", "\u{1E0D}\u{0307}");
        nfc!("\u{0071}\u{0307}\u{0323}", "\u{0071}\u{0323}\u{0307}");
    }

    #[test]
    fn string_concatenation() {
        // Table 2. String Concatenation
        nfc_add!("\u{0061}", "\u{0302}", "\u{00E2}");
        nfc_add!("\u{1100}", "\u{1161}\u{11A8}", "\u{AC01}");
    }

    #[test]
    fn basic_examples() {
        // Table 6. Basic Examples
        nfc!("\u{1E0A}", "\u{1E0A}"); // a: D-dot_above => D-dot_above
        nfc!("\u{0044}\u{0307}", "\u{1E0A}"); // b: D + dot_above => D-dot_above
        nfc!("\u{1E0C}\u{0307}", "\u{1E0C}\u{0307}"); // c: D-dot_below + dot_above => D-dot_below + dot_above
        nfc!("\u{1E0A}\u{0323}", "\u{1E0C}\u{0307}"); // d: D-dot_above + dot_below => D-dot_below + dot_above
        nfc!("\u{0044}\u{0307}\u{0323}", "\u{1E0C}\u{0307}"); // e: D + dot_above + dot_below => D-dot_below + dot_above
        nfc!(
            "\u{0044}\u{0307}\u{031B}\u{0323}",
            "\u{1E0C}\u{031B}\u{0307}"
        ); // f: D + dot_above + horn + dot_below => D-dot_below + horn + dot_above
        nfc!("\u{1E16}", "\u{1E16}"); // f: E-macron-grave => E-macron-grave
        nfc!("\u{0112}\u{0301}", "\u{1E16}"); // h: E-macron + grave => E-macron-grave
        nfc!("\u{00C8}\u{0304}", "\u{00C8}\u{0304}"); // i: E-grave + macron => E-grave + macron
        nfc!("\u{212B}", "\u{00C5}"); // j: angstrom_sign => A-ring
        nfc!("\u{00C5}", "\u{00C5}"); // k: A-ring => A-ring
    }

    #[test]
    fn nfc_applied_to_compatibility_equivalent_string() {
        // Table 7. NFD and NFC Applied to Compatibility-Equivalent Strings
        nfc!("Äffin", "Äffin"); // l
        nfc!("Ä\u{FB03}n", "Ä\u{FB03}n"); // m
        nfc!("Henry IV", "Henry IV"); // n
        nfc!("Henry \u{2163}", "Henry \u{2163}"); // o
        nfc!("\u{30AC}", "\u{30AC}"); // p: ga => ga
        nfc!("\u{30AB}\u{3099}", "\u{30AC}"); // q: ka + ten => ga
        nfc!("\u{FF76}\u{FF9E}", "\u{FF76}\u{FF9E}"); // r: hw_ka + hw_ten => hw_ka + hw_ten
        nfc!("\u{30AB}\u{FF9E}", "\u{30AB}\u{FF9E}"); // s: ka + hw_ten => ka + hw_ten
        nfc!("\u{FF76}\u{3099}", "\u{FF76}\u{3099}"); // t: hw_ka + ten => hw_ka + ten

        // I'm not sure, but this one is probably U+CE8C HANGUL SYLLABLE KAK
        nfc!("\u{CE8C}", "\u{CE8C}"); // u: kaks => kaks
    }
}
