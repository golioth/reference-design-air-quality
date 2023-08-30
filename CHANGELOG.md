<!-- Copyright (c) 2023 Golioth, Inc. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Limit retries on SPS30 data ready flag
- Increase SPS30 mutex timeout
- Fix sensor mutex error messages
- Clear Ostentus memory when app starts

### Changed

- Merge changes from [`golioth/reference-design-template@template_v1.1.0`](https://github.com/golioth/reference-design-template/tree/template_v1.1.0)
- Change sensor readings logging level from `LOG_INF` to `LOG_DBG`

### Removed

- Remove `clang-format` from pre-commit hooks
- Remove logging of JSON data sent to Golioth

## [1.1.4] - 2023-07-31

### Changed

- Remove potentially confusing log message from the main loop
- Merge changes from [`golioth/reference-design-template@e5cdc7d`](https://github.com/golioth/reference-design-template/commit/e5cdc7d5da4d1440135a63017159d2e691ec7713)
- Upgrade dependencies:
  - [`golioth/golioth-zephyr-sdk@f01824d`](https://github.com/golioth/golioth-zephyr-sdk/commit/f01824d8f0943463ee07cb493103a63221599c79)
  - [`golioth/golioth-zephyr-boards@0a0a27d`](https://github.com/golioth/golioth-zephyr-boards/commit/0a0a27dc2facc4245be0d15b9b36ce526cbf9262)

## [1.1.3] - 2023-07-20

### Added

- Added a CHANGELOG.md to track changes moving forward.
- Merged changes from `template_v1.0.1`
