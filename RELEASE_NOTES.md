# Release Notes

## Version 0.2.2_4 — Beta RC2

Beta RC2 supersedes the revoked Beta RC1 and its unpublished maintenance
builds. It completes native event synchronization and hardens fresh installs
and package upgrades.

### Added

- Automatic alias synchronization after native interface address and prefix
  events.
- A bounded retry window while dynamic addresses settle after boot or an
  interface restart.
- Installed build identity containing the package version, build timestamp,
  and source commit.

### Corrected

- Normalize an absent definition list before the first **Add** action, avoiding
  the fresh-install PHP `TypeError` that caused Beta RC1 to be revoked.
- Exclude the loopback interface and its networks from selectable sources.
- Preserve package-owned aliases during upgrades and reclaim only exact aliases
  previously marked `Former`, without adopting foreign aliases.
- Align port metadata and package XML with Netgate conventions.

### Validation

- 30 regression tests passed.
- PHP syntax checks passed.
- `check-plist` passed.
- Fresh installation, package upgrade, multiple selection, and live event
  synchronization were verified on pfSense CE 2.9.0.

### Package

- Version: `0.2.2_4`
- Target: pfSense CE 2.9.0 / FreeBSD 16 amd64
- License: Apache-2.0
- SHA256: `685ca89091463489cc14ab7605bb7117abd8fbae7732d27ec73443526feee9ca`

## Version 0.2.1 — Beta RC1a

This maintenance release normalizes an absent definition list before the
native package editor renders the first **Add** action. It fixes the PHP
`TypeError` seen on fresh installations without changing alias synchronization
or existing definitions.

## Version 0.2.0 — Beta RC1

Dynamic Networks Alias is a spin-off from
[DynGUA-PD](https://github.com/gebra-de/pfSense-Dyn-GUA-Delegation). The
automatic firewall-alias work developed there showed that native pfSense
aliases can safely track networks from multiple interfaces without requiring
interface groups.

The current development version provides:

- Native pfSense package installation and menu integration
- Administrator-defined aggregate network aliases
- Selection of two or more configured interfaces
- IPv4, IPv6 GUA (`2000::/3`), and IPv6 ULA (`fc00::/7`) filtering
- All useful combinations of the supported address classes
- Support for multiple active networks on one interface
- Native `network` aliases carrying the `automanaged` tag
- Protection against adopting or overwriting foreign aliases
- Empty-alias preservation when an alias is referenced by firewall or NAT rules
- Native package-framework editing and deletion
- Migration of pre-RC address-family values
- Coalesced automatic synchronization after native interface and prefix events

Alias content is synchronized when definitions are saved, edited, deleted, or
when pfSense reloads packages after native interface and prefix events. The
automatic path was verified during a live DHCPv6-PD and Track6 renumbering: the
managed aliases replaced the former prefixes without GUI interaction.

Firewall and NAT rules are never created or modified by the package. The
administrator decides where each generated alias is used.
