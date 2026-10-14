# Building Dynamic Networks Alias

Dynamic Networks Alias is built as a native pfSense package on a compatible
pfSense development or test system with the FreeBSD ports framework installed.

## Requirements

- A pfSense development or test installation matching the target architecture
- The FreeBSD ports framework under `/usr/ports`
- `pkg`, `make`, PHP, and the pfSense package framework
- `portlint` for source validation

Do not build packages on a production firewall.

## Build

Clone the repository into a directory named `pfSense-pkg-dynnetalias`. The
directory name determines the package origin recorded by the ports framework.

```sh
git clone <repository-url> pfSense-pkg-dynnetalias
cd pfSense-pkg-dynnetalias
```

On installations without FreeBSD source headers, pass the running kernel
version explicitly:

```sh
make OSVERSION="$(uname -K)" clean
env OSVERSION="$(uname -K)" portlint -CN
make OSVERSION="$(uname -K)" package
make OSVERSION="$(uname -K)" check-plist
```

The package is written to:

```text
work/pkg/pfSense-pkg-dynnetalias-<version>.pkg
```

Inspect the package metadata before installation:

```sh
pkg info -F work/pkg/pfSense-pkg-dynnetalias-<version>.pkg
sha256 work/pkg/pfSense-pkg-dynnetalias-<version>.pkg
```

## Test installation

Back up the pfSense configuration before installing a development build.

```sh
cp /cf/conf/config.xml /root/config-before-dynnetalias.xml
pkg add -f work/pkg/pfSense-pkg-dynnetalias-<version>.pkg
```

Open **Firewall > Auto Alias**, create a definition, and confirm the resulting
native alias under **Firewall > Aliases**. Firewall and NAT rules remain under
administrator control.

The package installs a `build-info` file containing its version, build
timestamp, and source commit. Confirm these values along with the package
origin and license before publishing a release artifact.
