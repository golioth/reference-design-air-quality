..
   Copyright (c) 2024 Golioth, Inc.
   SPDX-License-Identifier: Apache-2.0

Air Quality Monitor Reference Design
####################################

This repository contains the firmware source code and `pre-built release
firmware images <releases_>`_ for the Golioth Air Quality Monitor reference
design.

The full project details are available on the `Air Quality Monitor Project
Page`_, including follow-along guides for building an IoT Air Quality Monitor
yourself using widely available off-the-shelf development boards.

We call this **Follow-Along Hardware**, and we think it's one of the quickest
and easiest ways to get started building an IoT proof-of-concept with Golioth.
In the follow-along guides, you will learn how to assemble the hardware, flash a
pre-built firmware image onto the device, and connect to the Golioth cloud in
minutes.

Once you have completed a follow-along guide for one of our supported hardware
platforms, the instructions below will walk you through how to build and
configure the firmware yourself.

Supported Hardware
******************

This firmware can be built for a variety of supported hardware platforms.

.. pull-quote::
   [!IMPORTANT]

   In Zephyr, each of these different hardware variants is given a unique
   "board" identifier, which is used by the build system to generate firmware
   for that variant.

   When building firmware using the instructions below, make sure to use the
   correct Zephyr board identifier that corresponds to your follow-along
   hardware platform.

.. list-table:: **Follow-Along Hardware**
   :header-rows: 1

   * - Hardware
     - Zephyr Board
     - Follow-Along Guide

   * - .. image:: images/air_quality_monitor_fah_nrf9160_dk.jpg
          :width: 240
     - ``nrf9160dk_nrf9160_ns``
     - `nRF9160 DK Follow-Along Guide`_

.. list-table:: **Custom Golioth Hardware**
   :header-rows: 1

   * - Hardware
     - Zephyr Board
     - Project Page
   * - .. image:: images/air_quality_monitor_aludel_mini_v1_photo_top.jpg
          :width: 240
     - ``aludel_elixir_ns``
     - `Air Quality Monitor Project Page`_

Firmware Overview
*****************

This reference design firmware demonstrates how to measure ambient air quality
within an indoor environment using the Golioth IoT platform.

Specifically, the following environmental parameters can be monitored:

* 🦠 Airborne particulate matter (μg/m³ and #/cm³)
* 😷 CO₂ (ppm)
* 💦 Relative humidity (%RH)
* 🌡️ Temperature (°C)
* 💨 Pressure (kPa)

Local set up
************

.. pull-quote::
   [!IMPORTANT]

   Do not clone this repo using git. Zephyr's ``west`` meta tool should be used to
   set up your local workspace.

Install the Python virtual environment (recommended)
====================================================

.. code-block:: shell

   cd ~
   mkdir golioth-reference-design-air-quality
   python -m venv golioth-reference-design-air-quality/.venv
   source golioth-reference-design-air-quality/.venv/bin/activate
   pip install wheel west

Use ``west`` to initialize and install
======================================

.. code-block:: shell

   cd ~/golioth-reference-design-air-quality
   west init -m git@github.com:golioth/reference-design-air-quality.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt

Building the application
************************

Build the Zephyr sample application for the `Nordic nRF9160 DK`_
(``nrf9160dk_nrf9160_ns``) from the top level of your project. After a
successful build you will see a new ``build`` directory. Note that any changes
(and git commits) to the project itself will be inside the ``app`` folder. The
``build`` and ``deps`` directories being one level higher prevents the repo from
cataloging all of the changes to the dependencies and the build (so no
``.gitignore`` is needed).

Prior to building, update ``VERSION`` file to reflect the firmware version number you want to assign
to this build. Then run the following commands to build and program the firmware onto the device.


.. pull-quote::
   [!IMPORTANT]

   You must perform a pristine build (use ``-p`` or remove the ``build`` directory)
   after changing the firmware version number in the ``VERSION`` file for the change to take effect.

.. code-block:: text

   $ (.venv) west build -p -b nrf9160dk/nrf9160/ns --sysbuild app
   $ (.venv) west flash

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

.. code-block:: text

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold

Add Pipeline to Golioth
***********************

Golioth uses `Pipelines`_ to route stream data. This gives you flexibility to change your data
routing without requiring updated device firmware.

Whenever sending stream data, you must enable a pipeline in your Golioth project to configure how
that data is handled. Add the contents of ``pipelines/json-to-lightdb.yml`` as a new pipeline as
follows (note that this is the default pipeline for new projects and may already be present):

   1. Navigate to your project on the Golioth web console.
   2. Select ``Pipelines`` from the left sidebar and click the ``Create`` button.
   3. Give your new pipeline a name and paste the pipeline configuration into the editor.
   4. Click the toggle in the bottom right to enable the pipeline and then click ``Create``.

All data streamed to Golioth in JSON format will now be routed to LightDB Stream and may be viewed
using the web console. You may change this behavior at any time without updating firmware simply by
editing this pipeline entry.

Golioth Features
****************

This app implements:

- `Device Settings Service <https://docs.golioth.io/firmware/zephyr-device-sdk/device-settings-service>`_
- `LightDB State Client <https://docs.golioth.io/firmware/zephyr-device-sdk/light-db/>`_
- `LightDB Stream Client <https://docs.golioth.io/firmware/zephyr-device-sdk/light-db-stream/>`_
- `Logging Client <https://docs.golioth.io/firmware/zephyr-device-sdk/logging/>`_
- `Over-the-Air (OTA) Firmware Upgrade <https://docs.golioth.io/firmware/device-sdk/firmware-upgrade>`_
- `Remote Procedure Call (RPC) <https://docs.golioth.io/firmware/zephyr-device-sdk/remote-procedure-call>`_

Settings Service
================

The following settings can be set in the Device Settings menu of the `Golioth
Console`_.

``LOOP_DELAY_S``
   Adjusts the delay between sensor readings. Set to an integer value (seconds).

   Default value is ``60`` seconds.

``CO2_SENSOR_TEMPERATURE_OFFSET``
   Adjusts the temperature offset setting for the SCD4x CO₂ sensor. Set to an
   integer value (milli °C).

   Default value is ``0`` m°C.

``CO2_SENSOR_ALTITUDE``
   Adjusts the altitude setting for the SCD4x CO₂ sensor. Set to an integer
   value(meters above sea level).

   Default value is ``0`` meters.

``CO2_SENSOR_ASC_ENABLE``
   Enables or disables the automatic self-calibration setting for the SCD4x CO₂
   sensor. Set to a boolean value.

   Default value is ``true``.

``PM_SENSOR_SAMPLES_PER_MEASUREMENT``
   Adjusts the number of samples averaged together when fetching a measurement
   from the particulate matter sensor. Set to an integer value (samples).

   Note that each sample requires ~1s to fetch, so there is a tradeoff between
   getting a good average sample and the time required to fetch the measurement.

   Default value is ``30`` samples per measurement.

``PM_SENSOR_AUTO_CLEANING_INTERVAL``
   Adjusts the automatic fan cleaning interval setting for the SPS30 particulate
   matter sensor. Set to an integer value (seconds).

   Default value is ``604800`` seconds (168 hours or 1 week).

LightDB Stream Service
======================

Sensor data is periodically sent to the following ``sensor/*`` endpoints of the
LightDB Stream service:

* ``sensor/tem``: Temperature (°C)
* ``sensor/pre``: Pressure (kPa)
* ``sensor/hum``: Humidity (%RH)
* ``sensor/co2``: CO₂ (ppm)
* ``sensor/mc_1p0``: Particulate Matter Mass Concentration 1.0 (μg/m³)
* ``sensor/mc_2p5``: Particulate Matter Mass Concentration 2.5 (μg/m³)
* ``sensor/mc_4p0``: Particulate Matter Mass Concentration 4.0 (μg/m³)
* ``sensor/mc_10p0``: Particulate Matter Mass Concentration 10.0 (μg/m³)
* ``sensor/nc_0p5``: Particulate Matter Number Concentration 0.5 (#/cm³)
* ``sensor/nc_1p0``: Particulate Matter Number Concentration 1.0 (#/cm³)
* ``sensor/nc_2p5``: Particulate Matter Number Concentration 2.5 (#/cm³)
* ``sensor/nc_4p0``: Particulate Matter Number Concentration 4.0 (#/cm³)
* ``sensor/nc_10p0``: Particulate Matter Number Concentration 10.0 (#/cm³)
* ``sensor/tps``: Typical Particle Size (μm)

On hardware platforms with support for battery monitoring, battery voltage and
level readings are periodically sent to the following ``battery/*`` endpoints:

* ``battery/batt_v``: Battery Voltage (V)
* ``battery/batt_lvl``: Battery Level (%)

LightDB State Service
=====================

The concept of Digital Twin is demonstrated with the LightDB State
``example_int0`` and ``example_int1`` variables that are members of the
``desired`` and ``state`` endpoints.

* ``desired`` values may be changed from the cloud side. The device will
  recognize these, validate them for [0..65535] bounding, and then reset these
  endpoints to ``-1``

* ``state`` values will be updated by the device whenever a valid value is
  received from the ``desired`` endpoints. The cloud may read the ``state``
  endpoints to determine device status, but only the device should ever write to
  the ``state`` endpoints.

Remote Procedure Call (RPC) Service
===================================

The following RPCs can be initiated in the Remote Procedure Call menu of the
`Golioth Console`_.

``get_network_info``
   Query and return network information.

``reboot``
   Reboot the system.

``set_log_level``
   Set the log level.

   The method takes a single parameter which can be one of the following integer
   values:

   * ``0``: ``LOG_LEVEL_NONE``
   * ``1``: ``LOG_LEVEL_ERR``
   * ``2``: ``LOG_LEVEL_WRN``
   * ``3``: ``LOG_LEVEL_INF``
   * ``4``: ``LOG_LEVEL_DBG``

``clean_pm_sensor``
   Initiate the SPS30 particulate matter fan-cleaning procedure manually. The
   fan cleaning procedure takes approximately 10s to complete.

``reset_pm_sensor``
   Reset the SPS30 particulate matter sensor.

Hardware Variations
*******************

This reference design may be built for a variety of different boards.

Prior to building, update ``VERSION`` file to reflect the firmware version number you want to assign
to this build. Then run the following commands to build and program the firmware onto the device.

Golioth Aludel Elixir
=====================

This reference design may be built for the Golioth Aludel Elixir board. By default this will build
for the latest hardware revision of this board.

.. code-block:: text

   $ (.venv) west build -p -b aludel_elixir/nrf9160/ns --sysbuild app
   $ (.venv) west flash

To build for a specific board revision (e.g. Rev A) add the revision suffix ``@<rev>``.

.. code-block:: text

   $ (.venv) west build -p -b aludel_elixir@A/nrf9160/ns --sysbuild app
   $ (.venv) west flash

OTA Firmware Update
*******************

This application includes the ability to perform Over-the-Air (OTA) firmware updates:

1. Update the version number in the `VERSION` file and perform a pristine (important) build to
   incorporate the version change.
2. Upload the `build/app/zephyr/zephyr.signed.bin` file as an artifact for your Golioth project
   using `main` as the package name.
3. Create and roll out a release based on this artifact.

Visit `the Golioth Docs OTA Firmware Upgrade page`_ for more info.

External Libraries
******************

The following code libraries are installed by default. If you are not using the
custom hardware to which they apply, you can safely remove these repositories
from ``west.yml`` and remove the includes/function calls from the C code.

* `golioth-zephyr-boards`_ includes the board definitions for the Golioth
  Aludel-Elixir
* `libostentus`_ is a helper library for controlling the Ostentus ePaper
  faceplate
* `zephyr-network-info`_ is a helper library for querying, formatting, and
  returning network connection information via Zephyr log or Golioth RPC

.. _Golioth Console: https://console.golioth.io
.. _Nordic nRF9160 DK: https://www.nordicsemi.com/Products/Development-hardware/nrf9160-dk
.. _Pipelines: https://docs.golioth.io/data-routing
.. _the Golioth Docs OTA Firmware Upgrade page: https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade
.. _golioth-zephyr-boards: https://github.com/golioth/golioth-zephyr-boards
.. _libostentus: https://github.com/golioth/libostentus
.. _MikroE Arduino UNO click shield: https://www.mikroe.com/arduino-uno-click-shield
.. _MikroE Weather Click: https://www.mikroe.com/weather-click
.. _MikroE HVAC Click: https://www.mikroe.com/hvac-click
.. _Reference Design Template: https://github.com/golioth/reference-design-template
.. _Air Quality Monitor Project Page: https://projects.golioth.io/reference-designs/air-quality-monitor
.. _nRF9160 DK Follow-Along Guide: https://projects.golioth.io/reference-designs/air-quality-monitor/guide-nrf9160-dk
.. _releases: https://github.com/golioth/reference-design-air-quality/releases
.. _Zephyr Getting Started Guide: https://docs.zephyrproject.org/latest/develop/getting_started/
.. _Developer Training: https://training.golioth.io
.. _SemVer: https://semver.org
.. _the Golioth Docs OTA Firmware Upgrade page: https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade
.. _zephyr-network-info: https://github.com/golioth/zephyr-network-info
