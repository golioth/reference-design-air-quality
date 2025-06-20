<!-- Copyright (c) 2023 Golioth, Inc. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.4.0] 2025-05-15

### Changed

- Merge changes from [`golioth/reference-design-template@template_v2.7.1`](https://github.com/golioth/reference-design-template/tree/template_v2.7.1).

## [1.3.0] 2024-10-03

### Added

- Pipeline example
- Add support for the Aludel Elixir (`aludel_elixir_ns`) board.

### Changed

- Merge changes from [`golioth/reference-design-template@template_v2.4.1`](https://github.com/golioth/reference-design-template/tree/template_v2.4.1).
- Split sensor functions into separate files for each sensor.
- Increase the Golioth CoAP request queue size to prevent requests from failing
  to enqueue in some cases.
- Prevent sending excessive warning log messages on flaky connections.
- Only attempt to send sensor data when Golioth client is connected.

### Removed

- Remove pre-commit for consistency with the Reference Design Template
- Remove Aludel Mini (hardware no longer available, replaced by Elixir)

## [1.2.0] - 2023-09-05

### Fixed

- Limit retries on SPS30 data ready flag.
- Increase SPS30 mutex timeout.
- Fix sensor mutex error messages.

### Changed

- Merge changes from [`golioth/reference-design-template@f1d2422`](https://github.com/golioth/reference-design-template/commit/f1d2422ba04e13ebf66b36529abdbb781896e479).
- Change sensor readings logging level from `LOG_INF` to `LOG_DBG`.

### Removed

- Remove `clang-format` from pre-commit hooks.
- Remove logging of JSON data sent to Golioth.

## [1.1.4] - 2023-07-31

### Changed

- Remove potentially confusing log message from the main loop.
- Merge changes from [`golioth/reference-design-template@e5cdc7d`](https://github.com/golioth/reference-design-template/commit/e5cdc7d5da4d1440135a63017159d2e691ec7713).
- Upgrade dependencies:
  - [`golioth/golioth-zephyr-sdk@f01824d`](https://github.com/golioth/golioth-zephyr-sdk/commit/f01824d8f0943463ee07cb493103a63221599c79)
  - [`golioth/golioth-zephyr-boards@0a0a27d`](https://github.com/golioth/golioth-zephyr-boards/commit/0a0a27dc2facc4245be0d15b9b36ce526cbf9262)

## [1.1.3] - 2023-07-20

### Added

- Add a CHANGELOG.md to track changes moving forward.
- Merge changes from [`golioth/reference-design-template@template_v1.0.1`](https://github.com/golioth/reference-design-template/tree/template_v1.0.1).
