This the in-development version of Gromit-MPX's flatpak packaging. The release version can be found at
in [this repository](https://github.com/flathub/net.christianbeier.Gromit-MPX).

## Checking Manifest

- Install org.flatpak.Builder
- `flatpak run --command=flatpak-builder-lint org.flatpak.Builder --exceptions manifest net.christianbeier.Gromit-MPX.yml`

## Building and Running

Build and install a local flatpak via `flatpak-builder --force-clean --user --install build-dir net.christianbeier.Gromit-MPX.yml`.

Run the local flatpak via `flatpak run net.christianbeier.Gromit-MPX`.
