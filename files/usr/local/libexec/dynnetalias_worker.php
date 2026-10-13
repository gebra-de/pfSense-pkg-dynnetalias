#!/usr/local/bin/php
<?php
/*
 * Copyright 2026 gebra
 * SPDX-License-Identifier: Apache-2.0
 *
 * Coalesced native interface-event synchronization for Dynamic Networks Alias.
 */
require_once('/usr/local/pkg/dynnetalias.inc');

$worker = lock('dynnetalias-worker', LOCK_EX | LOCK_NB);
if (!$worker) {
	exit(0);
}
$signal = fopen(DYNNETALIAS_SIGNAL, 'c+');
if ($signal === false) {
	unlock($worker);
	error_log('Dynamic Networks Alias worker: cannot open synchronization signal');
	exit(1);
}

try {
	$processed = null;
	while (true) {
		if (!flock($signal, LOCK_EX)) {
			throw new RuntimeException('Cannot lock the Auto Alias worker signal');
		}
		try {
			rewind($signal);
			$raw = trim(stream_get_contents($signal));
			if ($raw !== '' && !ctype_digit($raw)) {
				throw new RuntimeException('Invalid Auto Alias worker generation');
			}
			$requested = (int)$raw;
		} finally {
			flock($signal, LOCK_UN);
		}

		if ($processed !== null && $processed === $requested) {
			break;
		}

		/* Let Track6 and other native interface processing publish final addresses. */
		sleep(2);
		dynnetalias_sync('native interface or package reload event');
		$processed = $requested;
	}
} catch (Throwable $error) {
	error_log('Dynamic Networks Alias worker: ' . $error->getMessage());
	exit(1);
} finally {
	fclose($signal);
	unlock($worker);
}
