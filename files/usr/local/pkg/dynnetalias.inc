<?php
/*
 * Copyright 2026 gebra
 * SPDX-License-Identifier: Apache-2.0
 *
 * Dynamic Networks Alias: native pfSense interface network aliases.
 */
if (!defined('DYNNETALIAS_TESTING')) {
	require_once('/etc/inc/config.inc');
	require_once('/etc/inc/interfaces.inc');
	require_once('/etc/inc/filter.inc');
	require_once('/etc/inc/util.inc');
}

const DYNNETALIAS_CONFIG = 'installedpackages/dynnetalias/config';
const DYNNETALIAS_DESCRIPTION = 'Managed by Auto Alias';
const DYNNETALIAS_FORMER_DESCRIPTION = 'Former Auto Alias; review referenced firewall and NAT rules';
if (!defined('DYNNETALIAS_SIGNAL')) {
	define('DYNNETALIAS_SIGNAL', '/var/run/dynnetalias-sync.generation');
}

/* Return the selected address classes and accept configuration from version 0.1.0. */
function dynnetalias_family_classes(string $family): array {
	$legacy = ['inet' => 'ipv4', 'inet6' => 'gua', 'both' => 'ipv4_gua'];
	$family = $legacy[$family] ?? $family;
	$classes = explode('_', $family);
	$valid = ['ipv4', 'ula', 'gua'];
	return count($classes) === count(array_unique($classes)) &&
		count(array_diff($classes, $valid)) === 0 ? $classes : [];
}

/* Identify an IPv6 Unique Local Address from fc00::/7. */
function dynnetalias_is_ula(mixed $address): bool {
	$packed = @inet_pton((string)$address);
	return $packed !== false && strlen($packed) === 16 && (ord($packed[0]) & 0xfe) === 0xfc;
}

/* Convert the native multiple-selection representation to logical interface names. */
function dynnetalias_interfaces(mixed $value): array {
	if (is_string($value)) {
		$value = preg_split('/[\s,]+/', trim($value), -1, PREG_SPLIT_NO_EMPTY);
	}
	if (!is_array($value)) {
		return [];
	}
	$interfaces = [];
	foreach ($value as $interface) {
		if (is_string($interface) && preg_match('/^[a-z][a-z0-9_]*$/D', $interface)) {
			$interfaces[$interface] = $interface;
		}
	}
	return array_values($interfaces);
}

/* Return enabled package definitions in their canonical in-memory form. */
function dynnetalias_definitions(): array {
	$definitions = config_get_path(DYNNETALIAS_CONFIG, []);
	if (!is_array($definitions)) {
		return [];
	}
	$result = [];
	foreach ($definitions as $definition) {
		if (!is_array($definition)) {
			continue;
		}
		$result[] = [
			'name' => trim((string)($definition['name'] ?? '')),
			'family' => (string)($definition['family'] ?? 'gua'),
			'interfaces' => dynnetalias_interfaces($definition['interfaces'] ?? []),
		];
	}
	return $result;
}

/* Identify aliases owned by this package, not merely any native managed alias. */
function dynnetalias_owns(array $alias): bool {
	return str_starts_with((string)($alias['descr'] ?? ''), DYNNETALIAS_DESCRIPTION) &&
		in_array('automanaged', get_tags_from_alias_item($alias), true);
}

/* Return a human-readable label for an address-family selection. */
function dynnetalias_family_label(string $family): string {
	$labels = [
		'ipv4' => gettext('IPv4 only'),
		'ula' => gettext('IPv6 ULA only'),
		'gua' => gettext('IPv6 GUA only'),
		'ipv4_gua' => gettext('IPv4 and IPv6 GUA'),
		'ipv4_ula' => gettext('IPv4 and IPv6 ULA'),
		'ipv4_gua_ula' => gettext('IPv4, IPv6 GUA, and IPv6 ULA'),
		'ula_gua' => gettext('IPv6 ULA and GUA'),
	];
	$legacy = dynnetalias_family_classes($family);
	$canonical = implode('_', $legacy);
	return $labels[$family] ?? $labels[$canonical] ?? $family;
}

/* Replace stored values with readable labels in the native package list only. */
function dynnetalias_prepare_list(mixed &$definitions): void {
	if (!is_array($definitions)) {
		$definitions = [];
		return;
	}
	$configured = get_configured_interface_with_descr();
	foreach ($definitions as &$definition) {
		if (!is_array($definition)) {
			continue;
		}
		$definition['family'] = dynnetalias_family_label((string)($definition['family'] ?? ''));
		$labels = [];
		foreach (dynnetalias_interfaces($definition['interfaces'] ?? []) as $interface) {
			$labels[] = $configured[$interface] ?? strtoupper($interface);
		}
		$definition['interfaces'] = implode(', ', $labels);
	}
	unset($definition);
}

/* Generate the description requested by the administrator-facing design. */
function dynnetalias_description(array $interfaces): string {
	$configured = get_configured_interface_with_descr();
	$labels = [];
	foreach ($interfaces as $interface) {
		$labels[] = ($configured[$interface] ?? strtoupper($interface)) . ' net';
	}
	return DYNNETALIAS_DESCRIPTION . '; content: ' . implode(', ', $labels);
}

/* Return the primary active IPv4 network for a logical interface. */
function dynnetalias_ipv4_network(string $interface): ?string {
	$address = get_interface_ip($interface);
	$length = get_interface_subnet($interface);
	if (!is_ipaddrv4($address) || !is_numeric($length) || (int)$length < 0 || (int)$length > 32) {
		return null;
	}
	return gen_subnet($address, (int)$length) . '/' . (int)$length;
}

/* Isolate the native address lookup so discovery can be regression-tested. */
if (!function_exists('dynnetalias_get_ifaddrs')) {
	function dynnetalias_get_ifaddrs(string $interface): array {
		$addresses = pfSense_get_ifaddrs($interface);
		return is_array($addresses) ? $addresses : [];
	}
}

/* Return every unique active GUA or ULA network currently present on an interface. */
function dynnetalias_ipv6_networks(string $interface, string $class = 'gua'): array {
	$physical = get_real_interface($interface);
	if (!is_string($physical) || $physical === '') {
		return [];
	}
	$addresses = dynnetalias_get_ifaddrs($physical);
	$networks = [];
	foreach ($addresses['addrs6'] ?? [] as $address) {
		$is_selected = is_array($address) &&
			(($class === 'gua' && is_v6gua($address['addr'] ?? null)) ||
			($class === 'ula' && dynnetalias_is_ula($address['addr'] ?? null)));
		if (!$is_selected || !isset($address['subnetbits']) ||
		    !is_numeric($address['subnetbits'])) {
			continue;
		}
		if (isset($address['tentative']) || isset($address['duplicated']) ||
		    isset($address['detached']) || isset($address['deprecated'])) {
			continue;
		}
		$length = (int)$address['subnetbits'];
		if ($length < 0 || $length > 128) {
			continue;
		}
		$network = inet_ntop(inet_pton(gen_subnetv6($address['addr'], $length)));
		if ($network !== false) {
			$networks["{$network}/{$length}"] = "{$network}/{$length}";
		}
	}
	return array_values($networks);
}

/* Resolve one definition to canonical, unique network entries and matching details. */
function dynnetalias_resolve(array $definition): array {
	$networks = [];
	$details = [];
	$classes = dynnetalias_family_classes($definition['family']);
	foreach ($definition['interfaces'] as $interface) {
		$resolved = [];
		if (in_array('ipv4', $classes, true)) {
			$network = dynnetalias_ipv4_network($interface);
			if ($network !== null) {
				$resolved[] = $network;
			}
		}
		foreach (['ula', 'gua'] as $class) {
			if (in_array($class, $classes, true)) {
				$resolved = array_merge($resolved, dynnetalias_ipv6_networks($interface, $class));
			}
		}
		foreach ($resolved as $network) {
			if (!isset($networks[$network])) {
				$networks[$network] = $network;
				$details[$network] = strtoupper($interface) . ' net';
			}
		}
	}
	ksort($networks, SORT_NATURAL);
	$ordered_details = [];
	foreach (array_keys($networks) as $network) {
		$ordered_details[] = $details[$network];
	}
	return ['address' => implode(' ', $networks), 'detail' => implode('||', $ordered_details)];
}

/* Validate a single native editor submission without changing configuration. */
function dynnetalias_validate(array $post, array &$input_errors, string|int|null $id = null): void {
	$name = trim((string)($post['name'] ?? ''));
	if (!is_validaliasname($name)) {
		$input_errors[] = invalidaliasnamemsg($name);
	}
	if (!dynnetalias_family_classes((string)($post['family'] ?? ''))) {
		$input_errors[] = gettext('Select a valid address family');
	}
	$interfaces = dynnetalias_interfaces($post['interfaces'] ?? []);
	if (count($interfaces) < 2) {
		$input_errors[] = gettext('Select at least two interfaces');
	}
	$configured = get_configured_interface_with_descr();
	foreach ($interfaces as $interface) {
		if ($interface === 'loopback') {
			$input_errors[] = gettext('Loopback cannot be used as an alias source');
			continue;
		}
		if (!array_key_exists($interface, $configured)) {
			$input_errors[] = sprintf(gettext('Interface %s is not configured'), $interface);
		}
	}
	foreach (dynnetalias_definitions() as $index => $definition) {
		if ((string)$index !== (string)$id &&
		    strcasecmp((string)($definition['name'] ?? ''), $name) === 0) {
			$input_errors[] = sprintf(gettext('Alias name %s is already defined'), $name);
		}
	}
	$aliases = config_get_path('aliases/alias', []);
	foreach (is_array($aliases) ? $aliases : [] as $alias) {
		if (strcasecmp((string)($alias['name'] ?? ''), $name) === 0 && !dynnetalias_owns($alias)) {
			$input_errors[] = sprintf(gettext('Alias %s already exists and is not owned by Auto Alias'), $name);
		}
	}
}

/* Synchronize owned aliases and return whether native alias configuration changed. */
function dynnetalias_sync_config(): bool {
	$desired = [];
	foreach (dynnetalias_definitions() as $definition) {
		if (!is_validaliasname($definition['name']) ||
		    !dynnetalias_family_classes($definition['family']) ||
		    count($definition['interfaces']) < 2) {
			throw new RuntimeException(gettext('Invalid Dynamic Networks Alias configuration'));
		}
		$key = strtolower($definition['name']);
		if (isset($desired[$key])) {
			throw new RuntimeException(sprintf(gettext('Alias name %s is defined more than once'),
				$definition['name']));
		}
		$resolved = dynnetalias_resolve($definition);
		$desired[$key] = [
			'name' => $definition['name'],
			'type' => 'network',
			'address' => $resolved['address'],
			'descr' => dynnetalias_description($definition['interfaces']),
			'detail' => $resolved['detail'],
			'tags' => 'automanaged',
		];
	}
	$aliases = config_get_path('aliases/alias', []);
	$aliases = is_array($aliases) ? $aliases : [];
	$result = [];
	foreach ($aliases as $alias) {
		$key = strtolower((string)($alias['name'] ?? ''));
		if (isset($desired[$key])) {
			$reclaimable = (string)($alias['descr'] ?? '') === DYNNETALIAS_FORMER_DESCRIPTION;
			if (!dynnetalias_owns($alias) && !$reclaimable) {
				throw new RuntimeException(sprintf(
					gettext('Alias %s already exists and is not owned by Auto Alias'),
					$desired[$key]['name']));
			}
			$result[] = $desired[$key];
			unset($desired[$key]);
			continue;
		}
		if (dynnetalias_owns($alias)) {
			if (is_alias_inuse((string)$alias['name'])) {
				$alias['address'] = '';
				$alias['detail'] = '';
				$result[] = $alias;
			}
			continue;
		}
		$result[] = $alias;
	}
	foreach ($desired as $alias) {
		$result[] = $alias;
	}
	if ($result === $aliases) {
		return false;
	}
	config_set_path('aliases/alias', $result);
	return true;
}

/* Convert address-family values written by version 0.1.0 to current values. */
function dynnetalias_migrate_config(): bool {
	$definitions = config_get_path(DYNNETALIAS_CONFIG, []);
	if (!is_array($definitions)) {
		return false;
	}
	$legacy = ['inet' => 'ipv4', 'inet6' => 'gua', 'both' => 'ipv4_gua'];
	$changed = false;
	foreach ($definitions as &$definition) {
		if (!is_array($definition)) {
			continue;
		}
		if (isset($legacy[$definition['family'] ?? ''])) {
			$definition['family'] = $legacy[$definition['family']];
			$changed = true;
		}
		if (array_key_exists('dynnetalias_usage', $definition)) {
			unset($definition['dynnetalias_usage']);
			$changed = true;
		}
	}
	unset($definition);
	if ($changed) {
		config_set_path(DYNNETALIAS_CONFIG, $definitions);
	}
	return $changed;
}

/* Serialize writes and reload native alias/filter state only after a real change. */
function dynnetalias_sync(string $reason = 'synchronize managed network aliases'): bool {
	$lock = lock('dynnetalias', LOCK_EX | LOCK_NB);
	if (!$lock) {
		throw new RuntimeException(gettext('Auto Alias is busy; retry the operation'));
	}
	try {
		config_read_file(false, false);
		if (dynnetalias_migrate_config() &&
		    write_config('Dynamic Networks Alias: migrate address family values') === false) {
			throw new RuntimeException(gettext('Failed to migrate managed alias definitions'));
		}
		if (!dynnetalias_sync_config()) {
			return false;
		}
		if (write_config("Dynamic Networks Alias: {$reason}") === false) {
			throw new RuntimeException(gettext('Failed to save managed aliases'));
		}
		alias_make_table();
		filter_configure();
		return true;
	} finally {
		unlock($lock);
	}
}

function dynnetalias_requirements(): void {
	foreach (['config_get_path', 'config_set_path', 'config_read_file', 'write_config',
	    'get_configured_interface_with_descr', 'get_real_interface', 'get_interface_ip',
	    'get_interface_subnet', 'pfSense_get_ifaddrs', 'is_validaliasname',
	    'invalidaliasnamemsg', 'get_tags_from_alias_item', 'is_alias_inuse',
	    'alias_make_table', 'filter_configure', 'lock', 'unlock', 'mwexec_bg'] as $function) {
		if (!function_exists($function)) {
			throw new RuntimeException(sprintf(
				gettext('Required pfSense function is unavailable: %s'), $function));
		}
	}
}

/* Coalesce native package reload events and hand synchronization to a worker. */
function dynnetalias_request_sync(): void {
	$signal = fopen(DYNNETALIAS_SIGNAL, 'c+');
	if ($signal === false) {
		throw new RuntimeException(gettext('Cannot open the Auto Alias worker signal'));
	}
	try {
		if (!flock($signal, LOCK_EX)) {
			throw new RuntimeException(gettext('Cannot lock the Auto Alias worker signal'));
		}
		rewind($signal);
		$raw = trim(stream_get_contents($signal));
		$generation = ctype_digit($raw) ? (int)$raw + 1 : 1;
		if (!ftruncate($signal, 0) || !rewind($signal) ||
		    fwrite($signal, (string)$generation) === false || !fflush($signal)) {
			throw new RuntimeException(gettext('Cannot save the Auto Alias worker signal'));
		}
	} finally {
		flock($signal, LOCK_UN);
		fclose($signal);
	}
	mwexec_bg('/usr/local/bin/php /usr/local/libexec/dynnetalias_worker.php');
}

function dynnetalias_package_initialize(): void {
	try {
		dynnetalias_requirements();
		dynnetalias_sync('initialize managed network aliases');
	} catch (Throwable $error) {
		error_log('Dynamic Networks Alias initialization: ' . $error->getMessage());
	}
}

function dynnetalias_package_resync(): void {
	try {
		dynnetalias_requirements();
		dynnetalias_request_sync();
	} catch (Throwable $error) {
		error_log('Dynamic Networks Alias synchronization request: ' . $error->getMessage());
	}
}

/* Remove unreferenced owned aliases and leave referenced aliases empty and editable. */
function dynnetalias_package_deinstall(): void {
	@unlink(DYNNETALIAS_SIGNAL);
	try {
		dynnetalias_requirements();
		config_read_file(false, false);
		$aliases = config_get_path('aliases/alias', []);
		$result = [];
		foreach (is_array($aliases) ? $aliases : [] as $alias) {
			if (!dynnetalias_owns($alias)) {
				$result[] = $alias;
				continue;
			}
			if (is_alias_inuse((string)$alias['name'])) {
				$alias['address'] = '';
				$alias['detail'] = '';
				$alias['descr'] = DYNNETALIAS_FORMER_DESCRIPTION;
				unset($alias['tags']);
				$result[] = $alias;
			}
		}
		if ($result !== $aliases) {
			config_set_path('aliases/alias', $result);
			write_config('Dynamic Networks Alias: release managed aliases during package removal');
			alias_make_table();
			filter_configure();
		}
	} catch (Throwable $error) {
		error_log('Dynamic Networks Alias deinstallation: ' . $error->getMessage());
	}
}
