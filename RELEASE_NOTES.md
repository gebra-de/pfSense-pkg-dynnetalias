# Release Notes

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
