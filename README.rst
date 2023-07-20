..
   Copyright (c) 2023 Golioth, Inc.
   SPDX-License-Identifier: Apache-2.0

Air Quality Monitor
###################

Overview
********

This reference design demonstrates how to measure ambient air quality within an
indoor environment using the Golioth IoT platform.

Specifically, the following environmental parameters can be monitored:

* 🦠 Airborne particulate matter (PM)
* 😷 CO₂
* 💦 Relative humidity
* 🌡️ Temperature
* 💨 Pressure

Local set up
************

Do not clone this repo using git. Zephyr's ``west`` meta tool should be used to
set up your local workspace.

Install the Python virtual environment (recommended)
====================================================

.. code-block:: console

   cd ~
   mkdir golioth-reference-design-air-quality
   python -m venv golioth-reference-design-air-quality/.venv
   source golioth-reference-design-air-quality/.venv/bin/activate
   pip install wheel west

Use ``west`` to initialize and install
======================================

.. code-block:: console

   cd ~/golioth-reference-design-air-quality
   west init -m git@github.com:golioth/reference-design-air-quality.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt

Building the application
************************

Build Zephyr sample application for Golioth Aludel-Mini
(``aludel_mini_v1_sparkfun9160_ns``) from the top level of your project. After a
successful build you will see a new ``build`` directory. Note that any changes
(and git commmits) to the project itself will be inside the ``app`` folder. The
``build`` and ``deps`` directories being one level higher prevents the repo from
cataloging all of the changes to the dependencies and the build (so no
``.gitignore`` is needed)

During building, replace ``<your.semantic.version>`` to utilize the DFU
functionality on this Reference Design.

.. code-block:: console

   $ (.venv) west build -p -b aludel_mini_v1_sparkfun9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

.. code-block:: console

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold

Golioth Features
****************

This app currently implements Over-the-Air (OTA) firmware updates, Settings
Service, Logging, RPC, and both LightDB State and LightDB Stream data.

Settings Service
================

The following settings should be set in the Device Settings menu of the
`Golioth Console`_.

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

LightDB State and LightDB Stream data
=====================================

Time-Series Data (LightDB Stream)
---------------------------------

Sensor data is periodicaly sent to the following ``sensor/*`` endpoints of the
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

Battery voltage and level readings are periodically sent to the following
``battery/*`` endpoints:

* ``battery/batt_v``: Battery Voltage (V)
* ``battery/batt_lvl``: Battery Level (%)

Stateful Data (LightDB State)
-----------------------------

The concept of Digital Twin is demonstrated with the LightDB State
``example_int0`` and ``example_int1`` variables that are members of the ``desired``
and ``actual`` endpoints.

* ``desired`` values may be changed from the cloud side. The device will recognize
  these, validate them for [0..65535] bounding, and then reset these endpoints
  to ``-1``

* ``actual`` values will be updated by the device whenever a valid value is
  received from the ``desired`` endpoints. The cloud may read the ``actual``
  endpoints to determine device status, but only the device should ever write to
  the ``actual`` endpoints.

Further Information in Header Files
===================================

Please refer to the comments in each header file for a service-by-service
explanation of this template.

Hardware Variations
*******************

Nordic nRF9160 DK
=================

This reference design may be built for the `Nordic nRF9160 DK`_, with the
`MikroE Arduino UNO click shield`_ to interface the two click boards.

* Position the `MikroE Weather Click`_ board in Slot 1
* Position the `MikroE HVAC Click`_ board in Slot 2

Use the following commands to build and program. (Use the same console commands
from above to provision this board after programming the firmware.)

.. code-block:: console

   $ (.venv) west build -p -b nrf9160dk_nrf9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

External Libraries
******************

The following code libraries are installed by default. If you are not using the
custom hardware to which they apply, you can safely remove these repositories
from ``west.yml`` and remove the includes/function calls from the C code.

* `golioth-zephyr-boards`_ includes the board definitions for the Golioth
  Aludel-Mini
* `libostentus`_ is a helper library for controlling the Ostentus ePaper
  faceplate

Pulling in updates from the Reference Design Template
*****************************************************

This reference design was forked from the `Reference Design Template`_ repo. We
recommend the following workflow to pull in future changes:

* Setup

  * Create a ``template`` remote based on the Reference Design Template repository

* Merge in template changes

  * Fetch template changes and tags
  * Merge template release tag into your ``main`` (or other branch)
  * Resolve merge conflicts (if any) and commit to your repository

.. code-block:: console

   # Setup
   git remote add template https://github.com/golioth/reference-design-template.git
   git fetch template --tags

   # Merge in template changes
   git fetch template --tags
   git checkout your_local_branch
   git merge template_v1.0.0

   # Resolve merge conflicts if necessry
   git add resolved_files
   git commit

.. _Golioth Console: https://console.golioth.io
.. _Nordic nRF9160 DK: https://www.nordicsemi.com/Products/Development-hardware/nrf9160-dk
.. _golioth-zephyr-boards: https://github.com/golioth/golioth-zephyr-boards
.. _libostentus: https://github.com/golioth/libostentus
.. _MikroE Arduino UNO click shield: https://www.mikroe.com/arduino-uno-click-shield
.. _MikroE Weather Click: https://www.mikroe.com/weather-click
.. _MikroE HVAC Click: https://www.mikroe.com/hvac-click
.. _Reference Design Template: https://github.com/golioth/reference-design-template
