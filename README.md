# M5Stack Core2 for AWS IoT EduKit Factory Firmware

Factory firmware for the M5Stack Core2 for AWS IoT EduKit. Use this repository to restore your device to the original program or to investigate and freely modify.This application was written to be easy to understand and replicate instead of efficiency. View the [API reference](https://edukit.workshop.aws/en/api-reference/v2/index.html) for using the included board support package.

## Cloning

This repo uses [Git Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) to bring in dependent components.

Note: If you download the ZIP file provided by GitHub UI, you will not get the contents of the submodules. Since the downloaded zip will also not be a git repository, you will not be able to compile the code since that is a toolchain requirement. You must clone the repository using the instructions below.

If using Windows, because this repository and its submodules contain symbolic links, set `core.symlinks` to true with the following command:

```shell
git config --global core.symlinks true
```

In addition to this, either enable [Developer Mode](https://docs.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development) or, whenever using a git command that writes to the system (e.g. `git pull`, `git clone`, and `git submodule update --init --recursive`), use a console elevated as administrator so that git can properly create symbolic links for this repository. Otherwise, symbolic links will be written as normal files with the symbolic links' paths in them as text. [This](https://blogs.windows.com/windowsdeveloper/2016/12/02/symlinks-windows-10/) gives more explanation.

To clone using HTTPS:

```shell
git clone https://github.com/aws-iot-edukit/Factory_Firmware-Core2_for_AWS.git --recurse-submodules
```

Using SSH:

```shell
git clone git@github.com:aws-iot-edukit/Factory_Firmware-Core2_for_AWS.git --recurse-submodules
```

If you have downloaded the repo without using the `--recurse-submodules` argument, you need to run:

```shell
git submodule update --init --recursive
```

## Build

The project targets PlatformIO `espressif32` `7.0.1`, which bundles ESP-IDF `6.0.1` and the GCC 15.2 toolchain. Build the firmware with:

```shell
pio run -e core2foraws
```

Managed components are version-pinned in `components/Core2-for-AWS-IoT-Kit/idf_component.yml`. PlatformIO resolves them into the ignored `managed_components` directory during configuration.

ESP-IDF 6 no longer supports CryptoAuthLib's mbedTLS ALT integration. The BSP's signing and verification APIs use the supported direct ATECC608 CryptoAuthLib interface, and legacy `CONFIG_ESP_TLS_USE_SECURE_ELEMENT` integration is disabled.

Cloud-synced folders can interfere with the component manager while it replaces generated dependencies. If configuration reports a managed-component file disappearing during extraction, build from a local non-synced checkout.

## Important files/folders

### main/main.c

This is the entry point for your application. Start by investigating and/or modifying this file for your needs.

### components/Core2-for-AWS-IoT-Kit

This is the location of the board support package. It includes drivers and helper libraries for controlling the on-board peripherals on the device.

### managed_components/espressif__esp-cryptoauthlib

This generated directory contains Espressif's managed [CryptoAuthLib component](https://components.espressif.com/components/espressif/esp-cryptoauthlib). Configure its version in the BSP's `idf_component.yml`; do not edit generated files under `managed_components`.

### partitions_16MB.csv

This is the partition table recommended for most applications. It provides sufficient file system sizes for storing Wi-Fi credentials, the user application, OTA updates, additional file storage, and storage for SPIFFS in the on-board flash. This utilizes the internal + external flash memory.

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This library is licensed under the MIT-0 License. See the LICENSE file.
