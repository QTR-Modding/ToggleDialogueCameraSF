# Corresponding Source

The distributed `ToggleDialogueCameraSF.dll` statically incorporates code from
these exact revisions:

- [CommonLibSF](https://github.com/QTR-Modding/commonlibsf) commit
  `229a3820bc4384fddb77931e74d1f3531351efbc`
- [commonlib-shared](https://github.com/libxse/commonlib-shared) commit
  `5470284e964d5510aa001dca3e0bb5548b6356a4`
- [spdlog](https://github.com/gabime/spdlog) v1.16.0, commit
  `486b55554f11c9cccc913e11a87085b2a91f706f`

The public Git repository provides the corresponding source and pins all three
dependency source trees as submodules. Clone it recursively to obtain the
project and the exact dependency revisions used by the binary:

```powershell
git clone --recursive https://github.com/QTR-Modding/ToggleDialogueCameraSF.git
```

GitHub's automatic source archives do not expand submodules and are not the
complete corresponding source by themselves. In an existing clone, retrieve
the pinned dependency trees with:

```powershell
git submodule update --init --recursive
```

Build instructions are in [README.md](README.md). Complete license, exception,
and third-party notice texts are in `COPYING`, `EXCEPTIONS`, `LICENSES/`, and
`THIRD_PARTY_NOTICES.md`.
