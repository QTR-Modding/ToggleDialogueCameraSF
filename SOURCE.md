# Corresponding Source

The distributed `ToggleDialogueCameraSF.dll` statically incorporates code from
these exact revisions:

- [CommonLibSF](https://github.com/QTR-Modding/commonlibsf) commit
  `9fc64c0f987e8c1bdc1964bb1aa9dec4a4839061`
- [commonlib-shared](https://github.com/libxse/commonlib-shared) commit
  `5470284e964d5510aa001dca3e0bb5548b6356a4`
- [spdlog](https://github.com/gabime/spdlog) v1.16.0, commit
  `486b55554f11c9cccc913e11a87085b2a91f706f`

The Git repository pins all three source trees as submodules. Because GitHub's
automatic source archives do not expand submodules, every distributed v0.15.0
binary must be accompanied at the same download location by
`ToggleDialogueCameraSF-v0.15.0-source.zip`. That archive contains the complete
project source and build scripts plus expanded source trees for all three
linked dependencies. Access to the private Git repository is not required to
obtain that archive.

Build instructions are in [README.md](README.md). Complete license, exception,
and third-party notice texts are in `COPYING`, `EXCEPTIONS`, `LICENSES/`, and
`THIRD_PARTY_NOTICES.md`.
