#!/usr/bin/env python3
"""Build release evidence and immutable assets; never publish a release."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile


ROOT = Path(__file__).resolve().parents[1]
HOSTS = {
    "windows-x86_64": ("x86_64-mingw32", "stcxx.exe", "sdcc.exe"),
    "macos-arm64": ("arm64-apple-darwin", "stcxx", "sdcc"),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def run(args, log=None):
    command = [str(arg) for arg in args]
    print("+ " + subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=ROOT, text=True, encoding="utf-8", errors="replace",
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    print(result.stdout, end="", flush=True)
    if log:
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(result.stdout, encoding="utf-8", newline="\n")
    require(result.returncode == 0, f"Command failed ({result.returncode}): {command}")
    return result.stdout


def tools(output, host=None):
    """Download exact manifest bindings, verifying before extraction or use."""
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ROOT / "tools/toolchain-manifest.json").read_text(encoding="utf-8"))
    installed = {}
    for tool in manifest["tools"]:
        for system in tool["systems"]:
            if host and system["host"] != HOSTS[host][0]:
                continue
            name = system["archiveFileName"]
            require(PurePosixPath(name).name == name and "\\" not in name, "Unsafe asset name")
            archive = output / name
            valid = lambda: archive.is_file() and archive.stat().st_size == system["size"] and digest(archive) == system["sha256"]
            if not valid():
                pending = archive.with_suffix(archive.suffix + ".part")
                for attempt in range(3):
                    try:
                        request = urllib.request.Request(system["url"], headers={"User-Agent": "arduino-mcs251-release"})
                        with urllib.request.urlopen(request, timeout=120) as response, pending.open("wb") as target:
                            shutil.copyfileobj(response, target)
                        require(pending.stat().st_size == system["size"] and digest(pending) == system["sha256"],
                                f"Archive checksum mismatch: {name}")
                        pending.replace(archive)
                        break
                    except Exception:
                        if attempt == 2:
                            raise
                        time.sleep(2)
            require(valid(), f"Invalid tool archive: {name}")
            if host:
                destination = output / "extracted" / tool["id"]
                destination.mkdir(parents=True, exist_ok=True)
                with zipfile.ZipFile(archive) as source:
                    for member in source.infolist():
                        relative = PurePosixPath(member.filename)
                        require(not relative.is_absolute() and ".." not in relative.parts and
                                "\\" not in member.filename and ":" not in member.filename,
                                f"Unsafe ZIP member: {member.filename}")
                        require((member.external_attr >> 16) & 0o170000 != 0o120000,
                                f"Unexpected symbolic link: {member.filename}")
                        path = Path(source.extract(member, destination))
                        if os.name != "nt" and not member.is_dir():
                            mode = (member.external_attr >> 16) & 0o777
                            path.chmod(mode or 0o644)
                installed[tool["id"]] = destination / system["archiveRoot"]
    if host:
        require(set(installed) == {"stcxx-toolchain", "stc-cli"}, "Incomplete native dependencies")
    return installed


def validate_host(host, work, arduino_cli, output):
    expected_machine = "arm64" if host == "macos-arm64" else "amd64"
    machine = platform.machine().lower()
    require(machine in ({"arm64", "aarch64"} if expected_machine == "arm64" else {"amd64", "x86_64"}),
            f"Native {host} build required; current machine is {machine}")
    work, output = work.resolve(), output.resolve()
    dependencies = tools(work / "tools", host)
    evidence = output / "validation"
    evidence.mkdir(parents=True, exist_ok=True)
    native = ROOT / "tools/stcxx-driver" / HOSTS[host][1]
    require(native.is_file(), f"Build native driver first: {native}")
    run(["node", "scripts/check-eeprom-layout.mjs", "--sdcc",
         dependencies["stcxx-toolchain"] / "sdcc/bin" / HOSTS[host][2]], evidence / "eeprom-layout.log")

    # Stage an isolated source platform. Override only the tool installation
    # paths; all compile/link recipes and menu options come from this checkout.
    user = work / "user"
    destination = user / "hardware/stc/mcs251"
    destination.mkdir(parents=True, exist_ok=True)
    for name in ("boards.txt", "platform.txt"):
        shutil.copy2(ROOT / name, destination / name)
    for name in ("cores", "variants", "libraries", "tools/stcxx-driver"):
        shutil.copytree(ROOT / name, destination / name, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("target", ".git"))
    config = work / "arduino-cli.yaml"
    save(config, {"directories": {"data": str(work / "data"), "downloads": str(work / "downloads"), "user": str(user)}})
    run([arduino_cli, "core", "update-index", "--config-file", config], evidence / "arduino-index.log")
    sketches = []
    for chip, start, end in (("stc32g12k128", 0xfe0800, 0x1000000), ("stc32g8k64", 0xff0000, 0xfff800)):
        build = work / ("b-" + chip)
        run([arduino_cli, "compile", "--clean", "--config-file", config,
             "--fqbn", f"stc:mcs251:{chip}:eeprom=k2", "--build-path", build,
             "--build-property", f"runtime.tools.stcxx-toolchain.path={dependencies['stcxx-toolchain'].as_posix()}",
             "--build-property", f"runtime.tools.stc-cli.path={dependencies['stc-cli'].as_posix()}",
             ROOT / "examples/Blink"], evidence / (chip + "-blink.log"))
        firmware = build / "Blink.ino.hex"
        require(firmware.is_file(), f"Missing Blink firmware: {firmware}")
        check_hex(firmware, start, end)
        shutil.copy2(firmware, evidence / (chip + "-Blink.hex"))
        sketches.append({"chip": chip, "eeprom": "k2", "sha256": digest(firmware),
                         "program_start": start, "program_end_exclusive": end})
    shared = run(["git", "-C", ROOT.parent / "stcxx", "rev-parse", "HEAD"]).strip()
    report = {"host": host, "machine": machine, "source_commit": os.environ.get("GITHUB_SHA"),
              "shared_compiler_commit": shared, "driver_sha256": digest(native),
              "native_layout_images": 40, "rejected_eeprom_overlaps": 5,
              "arduino_blink_builds": sketches, "hardware_tested": False}
    shutil.copy2(native, output / native.name)
    shutil.copytree(ROOT / "tools/stcxx-driver/LICENSES", output / "LICENSES", dirs_exist_ok=True)
    save(evidence / "host-validation.json", report)


def check_hex(path, start, end):
    base, count, home, eof = 0, 0, False, False
    for line in path.read_text(encoding="ascii").splitlines():
        if not line:
            continue
        record = bytes.fromhex(line.removeprefix(":"))
        require(line.startswith(":") and len(record) >= 5 and len(record) == record[0] + 5 and sum(record) % 256 == 0,
                f"Invalid HEX: {path}")
        address = int.from_bytes(record[1:3], "big")
        data = record[4:-1]
        if record[3] == 4:
            base = int.from_bytes(data, "big") << 16
        elif record[3] == 0:
            require(start <= base + address and base + address + len(data) <= end, f"HEX occupies EEPROM: {path}")
            home = home or base + address <= 0xff0000 < base + address + len(data)
            count += len(data)
        elif record[3] == 1:
            eof = True
        else:
            raise ValueError(f"Unsupported HEX record: {record[3]}")
    require(count and home and eof, f"Missing program/reset/EOF: {path}")


def assemble(drivers, output):
    drivers, output = drivers.resolve(), output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    target = ROOT / "tools/stcxx-driver"
    reports = []
    for host, (_, binary, _) in HOSTS.items():
        source = drivers / ("native-driver-" + host)
        report = json.loads((source / "validation/host-validation.json").read_text(encoding="utf-8"))
        require(report["host"] == host and report["source_commit"] == os.environ.get("GITHUB_SHA") and
                report["native_layout_images"] == 40 and report["rejected_eeprom_overlaps"] == 5 and
                len(report["arduino_blink_builds"]) == 2 and report["driver_sha256"] == digest(source / binary),
                f"Invalid host qualification: {host}")
        shutil.copy2(source / binary, target / binary)
        for path in (source / "LICENSES").rglob("*"):
            if path.is_file():
                destination = target / "LICENSES" / path.relative_to(source / "LICENSES")
                destination.parent.mkdir(parents=True, exist_ok=True)
                if destination.exists():
                    require(destination.read_bytes().replace(b"\r\n", b"\n") == path.read_bytes().replace(b"\r\n", b"\n"),
                            f"Conflicting dependency notice: {destination}")
                else:
                    shutil.copy2(path, destination)
        reports.append(report)
    require(len({r["shared_compiler_commit"] for r in reports}) == 1, "Host compiler source differs")
    version = next(line.split("=", 1)[1] for line in (ROOT / "platform.txt").read_text().splitlines() if line.startswith("version="))
    archive = output / f"arduino-mcs251-{version}.zip"
    run([target / "stcxx.exe", "package-platform", ROOT, archive])
    with zipfile.ZipFile(archive) as package:
        for binary in ("stcxx", "stcxx.exe"):
            member = package.getinfo(f"arduino-mcs251-{version}/tools/stcxx-driver/{binary}")
            require((member.external_attr >> 16) & 0o111, f"Missing executable permission: {binary}")
    tools(output)
    index = output / "package_mcs251_index.json"
    run([sys.executable, ROOT / "scripts/create-native-release-index.py", "--platform", archive,
         "--assets", output, "--previous", ROOT / "package_mcs251_index.json", "--output", index])
    run([sys.executable, ROOT / "scripts/verify-release-assets.py", index, output, "--version", version])
    report = output / "release-validation.json"
    save(report, {"version": version, "source_commit": os.environ.get("GITHUB_SHA"),
                  "hosts": reports, "platform_sha256": digest(archive), "index_sha256": digest(index),
                  "run_url": f"https://github.com/{os.environ.get('GITHUB_REPOSITORY')}/actions/runs/{os.environ.get('GITHUB_RUN_ID')}",
                  "hardware_tested": False})
    (output / "SHA256SUMS").write_text("".join(f"{digest(path)}  {path.name}\n" for path in (archive, index, report)),
                                       encoding="ascii", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate-host")
    validate.add_argument("--host", choices=HOSTS, required=True)
    validate.add_argument("--work", type=Path, required=True)
    validate.add_argument("--arduino-cli", default="arduino-cli")
    validate.add_argument("--output", type=Path, required=True)
    package = commands.add_parser("assemble")
    package.add_argument("--drivers", type=Path, required=True)
    package.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "validate-host":
        validate_host(args.host, args.work, args.arduino_cli, args.output)
    else:
        assemble(args.drivers, args.output)


if __name__ == "__main__":
    main()
