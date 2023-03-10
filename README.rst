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

This will also install the `golioth-zephyr-boards`_ definitions for the Golioth
Aludel-Mini carrier board.

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

   $ (.venv) west build -b aludel_mini_v1_sparkfun9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
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
Service, Logging, and RPC.

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

.. _Golioth Console: https://console.golioth.io
.. _golioth-zephyr-boards: https://github.com/golioth/golioth-zephyr-boards
