#!/usr/bin/env python3

import os
import shutil
from typing import Any, List, Protocol, Sequence, TYPE_CHECKING, TypedDict, cast

if TYPE_CHECKING:
    # Provided at runtime by SCons/PlatformIO.
    def Import(*names: str) -> None: ...


class FileLike(Protocol):
    def get_abspath(self) -> str: ...


class BoardConfigLike(Protocol):
    def get(self, key: str, default: str) -> str: ...


class EnvironmentLike(Protocol):
    def BoardConfig(self) -> BoardConfigLike: ...

    def Flatten(self, items: Sequence[Any]) -> List[Any]: ...

    def get(self, key: str, default: Any = ...) -> Any: ...

    def subst(self, value: str) -> str: ...

    def Execute(self, action: Any) -> Any: ...

    def VerboseAction(self, command: str, message: str) -> Any: ...

    def AddCustomTarget(self, **kwargs: Any) -> Any: ...


Import("env", "projenv")
env = cast(EnvironmentLike, globals()["env"])
projenv = cast(EnvironmentLike, globals()["projenv"])

board_config = env.BoardConfig()


class FlashImage(TypedDict):
    address: str
    file: str


firmware_bin: str = os.path.join("${BUILD_DIR}", "${PROGNAME}.bin")
default_merged_bin: str = os.path.join("${BUILD_DIR}", "${PROGNAME}-merged.bin")
merged_bin: str = os.environ.get("MERGED_BIN_PATH", default_merged_bin)
default_app0_bin: str = os.path.join("${BUILD_DIR}", "${PROGNAME}-app0.bin")
app0_bin: str = os.environ.get("APP0_BIN_PATH", default_app0_bin)


def _collect_flash_images(
    source: List[FileLike], env: EnvironmentLike, include_bootloader: bool
) -> List[FlashImage]:
    app_image: FlashImage = {
        "address": "$ESP32_APP_OFFSET",
        "file": source[0].get_abspath(),
    }

    if not include_bootloader:
        return [app_image]

    extra_images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", []))
    flash_images: List[FlashImage] = []

    # FLASH_EXTRA_IMAGES is expected to be a list of [address, file] pairs
    for i in range(0, len(extra_images) - 1, 2):
        flash_images.append(
            {"address": str(extra_images[i]), "file": str(extra_images[i + 1])}
        )

    if len(extra_images) % 2 != 0:
        print("Warning: FLASH_EXTRA_IMAGES had an odd number of entries; last item ignored")

    # Add the main firmware image
    flash_images.append(app_image)
    return flash_images


def _print_flash_images(env: EnvironmentLike, flash_images: List[FlashImage]) -> None:
    print("Flash images to merge:")
    for img in flash_images:
        resolved_address = env.subst(img["address"])
        try:
            kiB = int(resolved_address, 0) // 1024
            address_text = f"{img['address']} ({kiB:,} KiB)"
        except (TypeError, ValueError):
            address_text = img["address"]

        file_name = os.path.basename(env.subst(img["file"]))
        print(f" @ {address_text}: {file_name}")


def _build_merge_command(
    env: EnvironmentLike, flash_images: List[FlashImage], output_path: str
) -> List[str]:
    merge_cmd: List[str] = [
        "$PYTHONEXE",
        "$OBJCOPY",
        "--chip",
        board_config.get("build.mcu", "esp32"),
        "merge_bin",
        "-o",
        output_path,
        "--flash_mode",
        "keep",
        "--flash_freq",
        "${__get_board_f_flash(__env__)}",
        "--flash_size",
        board_config.get("upload.flash_size", "4MB"),
    ]

    # Add flash images (address + file pairs)
    for img in flash_images:
        merge_cmd.append(env.subst(img["address"]))
        merge_cmd.append(env.subst(img["file"]))

    return merge_cmd


def _merge_bin_action(
    source: List[FileLike],
    target: List[FileLike],
    env: EnvironmentLike,
    include_bootloader: bool,
    output_path: str,
) -> None:
    del target  # Custom target does not use this argument.
    flash_images = _collect_flash_images(
        source, env, include_bootloader=include_bootloader
    )
    _print_flash_images(env, flash_images)

    merge_cmd = _build_merge_command(env, flash_images, output_path)
    env.Execute(
        env.VerboseAction(
            " ".join(['"' + arg + '"' for arg in merge_cmd]), "Merging firmware..."
        )
    )


def _copy_app_image(source: List[FileLike], output_path: str, env: EnvironmentLike) -> None:
    app_image = _collect_flash_images(source, env, include_bootloader=False)[0]
    source_path = env.subst(app_image["file"])
    output_path_resolved = env.subst(output_path)
    output_dir = os.path.dirname(output_path_resolved)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    shutil.copyfile(source_path, output_path_resolved)
    print(
        "Copied app-only image "
        f"{os.path.basename(output_path_resolved)} "
        f"for flashing at {env.subst('$ESP32_APP_OFFSET')}"
    )


def merge_bin_action(
    source: List[FileLike], target: List[FileLike], env: EnvironmentLike
) -> None:
    _merge_bin_action(
        source,
        target,
        env,
        include_bootloader=True,
        output_path=merged_bin,
    )


def merge_bin_app0_action(
    source: List[FileLike], target: List[FileLike], env: EnvironmentLike
) -> None:
    del target
    _copy_app_image(source, app0_bin, env)


def merge_bin_seperate_bootloader(
    source: List[FileLike], target: List[FileLike], env: EnvironmentLike
) -> None:
    # Backward-compatible alias for the previous custom target name.
    merge_bin_app0_action(source, target, env)


env.AddCustomTarget(
    name="mergebin",
    dependencies=firmware_bin,
    actions=merge_bin_action,
    title="Build merged image",
    description="Build combined image",
    always_build=True,
)

env.AddCustomTarget(
    name="mergebin_seperate_bootloader",
    dependencies=firmware_bin,
    actions=merge_bin_app0_action,
    title="Build app0 image",
    description="Build app-only image for flashing at 0x10000",
    always_build=True,
)

