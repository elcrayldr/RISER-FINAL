# RiserLab

RiserLab is a CRAY LDR audio plugin built with JUCE.

## Supported formats

- Windows: VST3
- macOS: Universal VST3 and Audio Unit (AU), targeting Intel x86_64 and Apple Silicon arm64

## macOS distribution

The macOS packaging workflow creates a ZIP, PKG installer, and DMG. The binaries are ad-hoc signed for bundle integrity, but are not Developer ID signed or notarized by Apple. macOS may therefore require the user to approve the installer in Privacy & Security.

The macOS workflow validates the bundle structure, architecture slices, and linked system dependencies before publishing build artifacts.

## Build

GitHub Actions builds Windows and macOS plugins on pushes to main and macos-distribution. The full macOS installer workflow runs on pushes to macos-distribution, published releases, or manual dispatch.
