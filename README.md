# Golioth Air Quality Monitor Reference Design

The Air Quality Monitor demonstrates how to measure ambient air quality within an indoor environment
using the Golioth IoT platform. Specifically, the following environmental parameters can be monitored:

  - 🦠 Airborne particulate matter (μg/m³ and \#/cm³)
  - 😷 CO₂ (ppm)
  - 💦 Relative humidity (%RH)
  - 🌡️ Temperature (°C)
  - 💨 Pressure (kPa)

From the Golioth web console you may view data, deploy OTA firmware updates, adjust device Settings,
and issue Remote Procedure Calls (RPCs).

Business use cases and hardware build details are available on [the Air Quality Monitor Project
Page](https://projects.golioth.io/reference-designs/air-quality-monitor).

This repository contains the firmware source code and [pre-built release firmware
images](https://github.com/golioth/reference-design-air-quality/releases).

## Supported Hardware

- Nordic nRF9160-DK
- Golioth Aludel Elixir

### Additional Sensors/Components

- Sensirion SCD41 carbon dioxide sensor
- Sensirion SPS30 particulate matter sensor
- Bosch BME280 digital humidity, pressure, and temperature sensor

## Golioth Features

This app implements:

  - [Device Settings
    Service](https://docs.golioth.io/firmware/golioth-firmware-sdk/device-settings-service)
  - [Remote Procedure Call
    (RPC)](https://docs.golioth.io/firmware/golioth-firmware-sdk/remote-procedure-call)
  - [Stream Client](https://docs.golioth.io/firmware/golioth-firmware-sdk/stream-client)
  - [LightDB State Client](https://docs.golioth.io/firmware/golioth-firmware-sdk/light-db-state/)
  - [Over-the-Air (OTA) Firmware
    Upgrade](https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade)
  - [Backend Logging](https://docs.golioth.io/device-management/logging/)

### Settings Service

The following settings should be set in the Device Settings menu of the
[Golioth Console](https://console.golioth.io).

  - `LOOP_DELAY_S`
    Adjusts the delay between sensor readings. Set to an integer value
    (seconds).

    Default value is `60` seconds.

  - `CO2_SENSOR_TEMPERATURE_OFFSET`
    Adjusts the temperature offset setting for the SCD4x CO₂ sensor. Set
    to an integer value (milli °C).

    Default value is `0` m°C.

  - `CO2_SENSOR_ALTITUDE`
    Adjusts the altitude setting for the SCD4x CO₂ sensor. Set to an
    integer value(meters above sea level).

    Default value is `0` meters.

  - `CO2_SENSOR_ASC_ENABLE`
    Enables or disables the automatic self-calibration setting for the
    SCD4x CO₂ sensor. Set to a boolean value.

    Default value is `true`.

  - `PM_SENSOR_SAMPLES_PER_MEASUREMENT`
    Adjusts the number of samples averaged together when fetching a
    measurement from the particulate matter sensor. Set to an integer
    value (samples).

    Note that each sample requires \~1s to fetch, so there is a tradeoff
    between getting a good average sample and the time required to fetch
    the measurement.

    Default value is `30` samples per measurement.

  - `PM_SENSOR_AUTO_CLEANING_INTERVAL`
    Adjusts the automatic fan cleaning interval setting for the SPS30
    particulate matter sensor. Set to an integer value (seconds).

    Default value is `604800` seconds (168 hours or 1 week).

### Remote Procedure Call (RPC) Service

The following RPCs can be initiated in the Remote Procedure Call menu of
the [Golioth Console](https://console.golioth.io).

  - `get_network_info`
    Query and return network information.

  - `reboot`
    Reboot the system.

  - `set_log_level`
    Set the log level.

    The method takes a single parameter which can be one of the
    following integer values:

      - `0`: `LOG_LEVEL_NONE`
      - `1`: `LOG_LEVEL_ERR`
      - `2`: `LOG_LEVEL_WRN`
      - `3`: `LOG_LEVEL_INF`
      - `4`: `LOG_LEVEL_DBG`

  - `clean_pm_sensor`
    Initiate the SPS30 particulate matter fan-cleaning procedure
    manually. The fan cleaning procedure takes approximately 10s to
    complete.

  - `reset_pm_sensor`
    Reset the SPS30 particulate matter sensor.

### Time-Series Stream data

Sensor data is periodically sent to the following `sensor/*` endpoints
of the LightDB Stream service:

  - `sensor/tem`: Temperature (°C)
  - `sensor/pre`: Pressure (kPa)
  - `sensor/hum`: Humidity (%RH)
  - `sensor/co2`: CO₂ (ppm)
  - `sensor/mc_1p0`: Particulate Matter Mass Concentration 1.0 (μg/m³)
  - `sensor/mc_2p5`: Particulate Matter Mass Concentration 2.5 (μg/m³)
  - `sensor/mc_4p0`: Particulate Matter Mass Concentration 4.0 (μg/m³)
  - `sensor/mc_10p0`: Particulate Matter Mass Concentration 10.0 (μg/m³)
  - `sensor/nc_0p5`: Particulate Matter Number Concentration 0.5
    (\#/cm³)
  - `sensor/nc_1p0`: Particulate Matter Number Concentration 1.0
    (\#/cm³)
  - `sensor/nc_2p5`: Particulate Matter Number Concentration 2.5
    (\#/cm³)
  - `sensor/nc_4p0`: Particulate Matter Number Concentration 4.0
    (\#/cm³)
  - `sensor/nc_10p0`: Particulate Matter Number Concentration 10.0
    (\#/cm³)
  - `sensor/tps`: Typical Particle Size (μm)

``` json
{
  "sensor": {
    "co2": 1419,
    "hum": 35.804687,
    "mc_10p0": 0.117215,
    "mc_1p0": 0.110846,
    "mc_2p5": 0.117215,
    "mc_4p0": 0.117215,
    "nc_0p5": 0.748922,
    "nc_10p0": 0.885024,
    "nc_1p0": 0.881767,
    "nc_2p5": 0.884648,
    "nc_4p0": 0.884848,
    "pre": 98.689242,
    "tem": 23.01,
    "tps": 0.439595
  }
}
```

If your board includes a battery, voltage and level readings
will be sent to the `battery` path.

> [!NOTE]
> Your Golioth project must have a Pipeline enabled to receive this
> data. See the [Add Pipeline to Golioth](#add-pipeline-to-golioth)
> section below.

### Stateful Data (LightDB State)

The concept of Digital Twin is demonstrated with the LightDB State
`example_int0` and `example_int1` variables that are subpaths of the
`desired` and `state` paths.

  - `desired` values may be changed from the cloud side. The device will
    recognize these, validate them for \[0..65535\] bounding, and then
    reset these values to `-1`
  - `state` values will be updated by the device to reflect the device's
    actual stored value. The cloud may read the `state` endpoints to
    determine device status. In this arrangement, only the device
    should ever write to the `state` endpoints.

``` json
{
  "desired": {
    "example_int0": -1,
    "example_int1": -1
  },
  "state": {
    "example_int0": 0,
    "example_int1": 1
  }
}
```

By default the state values will be `0` and `1`. Try updating the
`desired` values and observe how the device updates its state.

### OTA Firmware Update

This application includes the ability to perform Over-the-Air (OTA)
firmware updates. To do so, you need a binary compiled with a different
version number than what is currently running on the device.

> [!NOTE]
> If a newer release is available than what your device is currently
> running, you may download the pre-compiled binary that ends in
> `_update.bin` and use it in step 2 below.

1. Update the version number in the `VERSION` file and perform a
   pristine (important) build to incorporate the version change.
2. Upload the `build/app/zephyr/zephyr.signed.bin` file as a Package for
   your Golioth project.

   - Use either `nrf9160dk` or `aludel_elixir` as the package name,
     depending on which board the update file was built for. (These
     package names were configured in this repository's board `.conf`
     files.) Use `main` as the package name.
   - Use the same version number from step 1.

3. Create a Cohort and add your device to it.
4. Create a Deployment for your Cohort using the package name and
   version number from step 2.
5. Devices in your Cohort will automatically upgrade to the most
   recently deployed firmware.

Visit [the Golioth Docs OTA Firmware Upgrade
page](https://docs.golioth.io/firmware/golioth-firmware-sdk/firmware-upgrade/firmware-upgrade)
for more info.

## Add Pipeline to Golioth

Golioth uses [Pipelines](https://docs.golioth.io/data-routing) to route
stream data. This gives you flexibility to change your data routing
without requiring updated device firmware.

Whenever sending stream data, you must enable a pipeline in your Golioth
project to configure how that data is handled. Add the contents of
`pipelines/json-to-lightdb.yml` as a new pipeline as follows (note that
this is the default pipeline for new projects and may already be
present):

> 1.  Navigate to your project on the Golioth web console.
> 2.  Select `Pipelines` from the left sidebar and click the `Create`
>     button.
> 3.  Give your new pipeline a name and paste the pipeline configuration
>     into the editor.
> 4.  Click the toggle in the bottom right to enable the pipeline and
>     then click `Create`.

All data streamed to Golioth in JSON format will now be routed to
LightDB Stream and may be viewed using the web console. You may change
this behavior at any time without updating firmware simply by editing
this pipeline entry.

## Local set up

> [!IMPORTANT]
> Do not clone this repo using git. Zephyr's `west` meta tool should be
> used to set up your local workspace.

### Install the Python virtual environment (recommended)

``` shell
cd ~
mkdir golioth-reference-design-air-quality
python -m venv golioth-reference-design-air-quality/.venv
source golioth-reference-design-air-quality/.venv/bin/activate
pip install wheel west ecdsa
```

### Use `west` to initialize and install

``` shell
cd ~/golioth-reference-design-air-quality
west init -m git@github.com:golioth/reference-design-air-quality.git .
west update
west zephyr-export
pip install -r deps/zephyr/scripts/requirements.txt
```

## Building the application

Build the Zephyr sample application for the [Nordic nRF9160
DK](https://www.nordicsemi.com/Products/Development-hardware/nrf9160-dk)
(`nrf9160dk_nrf9160_ns`) from the top level of your project. After a
successful build you will see a new `build` directory. Note that any
changes (and git commits) to the project itself will be inside the `app`
folder. The `build` and `deps` directories being one level higher
prevents the repo from cataloging all of the changes to the dependencies
and the build (so no `.gitignore` is needed).

Prior to building, update `VERSION` file to reflect the firmware version
number you want to assign to this build. Then run the following commands
to build and program the firmware onto the device.

> [!WARNING]
> You must perform a pristine build (use `-p` or remove the `build`
> directory) after changing the firmware version number in the `VERSION`
> file for the change to take effect.

``` text
$ (.venv) west build -p -b nrf9160dk/nrf9160/ns --sysbuild app
$ (.venv) west flash
```

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

``` text
uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
uart:~$ settings set golioth/psk <my-psk>
uart:~$ kernel reboot cold
```

## External Libraries

The following code libraries are installed by default. If you are not
using the custom hardware to which they apply, you can safely remove
these repositories from `west.yml` and remove the includes/function
calls from the C code.

  - [golioth-zephyr-boards](https://github.com/golioth/golioth-zephyr-boards)
    includes the board definitions for the Golioth Aludel-Elixir
  - [libostentus](https://github.com/golioth/libostentus) is a helper
    library for controlling the Ostentus ePaper faceplate
  - [zephyr-network-info](https://github.com/golioth/zephyr-network-info)
    is a helper library for querying, formatting, and returning network
    connection information via Zephyr log or Golioth RPC

## Have Questions?

Please get in touch with Golioth engineers by starting a new thread on
the [Golioth Forum](https://forum.golioth.io/).
