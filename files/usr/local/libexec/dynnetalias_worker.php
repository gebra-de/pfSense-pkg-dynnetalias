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
	$retry_delays = [2, 3, 5, 10, 20, 30];
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

		/* Track6 can publish addresses well after the first package reload event. */
		foreach ($retry_delays as $attempt => $delay) {
			sleep($delay);
			try {
				dynnetalias_sync('native interface or package reload event');
			} catch (Throwable $error) {
				if ($attempt === array_key_last($retry_delays)) {
					throw $error;
				}
				error_log('Dynamic Networks Alias worker: ' . $error->getMessage() .
					'; retry scheduled');
			}

			if (!flock($signal, LOCK_EX)) {
				throw new RuntimeException('Cannot lock the Auto Alias worker signal');
			}
			try {
				rewind($signal);
				$raw = trim(stream_get_contents($signal));
				if ($raw !== '' && !ctype_digit($raw)) {
					throw new RuntimeException('Invalid Auto Alias worker generation');
				}
				$current = (int)$raw;
			} finally {
				flock($signal, LOCK_UN);
			}
			if ($current !== $requested) {
				$requested = $current;
				continue 2;
			}
		}
		$processed = $requested;
	}
} catch (Throwable $error) {
	error_log('Dynamic Networks Alias worker: ' . $error->getMessage());
	exit(1);
} finally {
	fclose($signal);
	unlock($worker);
}
