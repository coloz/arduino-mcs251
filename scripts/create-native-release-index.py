#!/usr/bin/env python3
"""Verify native ZIP payloads and add a release while retaining older platforms.

The tool manifest supplies immutable tool URLs, versions, sizes and SHA-256.
Only hosts with a driver in the platform and both tool dependencies are exposed.
This command writes an index; it never publishes or installs anything.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def payload(path, root):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        require(len(names) == len(set(names)), f'Duplicate ZIP member: {path}')
        for name in names:
            relative = PurePosixPath(name)
            require(not relative.is_absolute() and '..' not in relative.parts
                    and '\\' not in name and ':' not in name
                    and name.startswith(root + '/'), f'Unsafe ZIP member: {name}')
        files = {name[len(root) + 1:]: archive.read(name)
                 for name in names if not name.endswith('/')}
    require('MANIFEST.sha256' in files, f'Missing payload manifest: {path}')
    expected = {}
    for line in files['MANIFEST.sha256'].decode().splitlines():
        digest, name = line.split('  ', 1)
        require(name not in expected and re.fullmatch('[0-9a-f]{64}', digest),
                f'Invalid manifest entry: {name}')
        expected[name] = digest
    actual = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()
              if name != 'MANIFEST.sha256'}
    require(expected == actual, f'Payload manifest mismatch: {path}')
    require(not any(PurePosixPath(name).suffix.lower() in
                    {'.py', '.pyc', '.pyd', '.ps1', '.sh'} for name in files),
            f'Interpreter script in native package: {path}')
    return files


def create(platform, assets, previous, manifest):
    version = platform.name.removeprefix('arduino-mcs251-').removesuffix('.zip')
    require(re.fullmatch(r'\d+\.\d+\.\d+', version), 'Invalid platform archive name')
    files = payload(platform, 'arduino-mcs251-' + version)
    properties = dict(line.split('=', 1) for line in files['platform.txt'].decode().splitlines()
                      if '=' in line and not line.startswith('#'))
    require(properties['version'] == version, 'Platform version mismatch')
    driver = properties.get('compiler.driver', '')
    for key in ('compiler.path', 'compiler.cpp.cmd'):
        driver = driver.replace('{' + key + '}', properties.get(key, ''))
    require('tools/stcxx-driver/stcxx' in driver,
            'Platform does not use the native compiler driver')
    result = copy.deepcopy(previous)
    require(len(result['packages']) == 1 and result['packages'][0]['name'] == 'stc',
            'Expected the stc package')
    package = result['packages'][0]
    require(not any(p['version'] == version for p in package['platforms']),
            'Platform version already exists; releases are immutable')
    tools = manifest['tools']
    require({t['id'] for t in tools} == {'stcxx-toolchain', 'stc-cli'} and len(tools) == 2,
            'Expected native compiler and uploader dependencies')
    hosts = [{s['host'] for s in tool['systems']} for tool in tools]
    require(hosts[0] and hosts[0] == hosts[1], 'Dependency hosts differ')
    drivers = {'x86_64-mingw32': ('stcxx.exe', 'windows-x86_64'),
               'arm64-apple-darwin': ('stcxx', 'macos-arm64')}
    for host in hosts[0]:
        require(host in drivers, f'Unsupported host: {host}')
        binary, lock = drivers[host]
        require('tools/stcxx-driver/' + binary in files, f'Missing driver for {host}')
        require(f'tools/stcxx-driver/toolchain-lock.{lock}.json' in files,
                f'Missing lock for {host}')
    for tool in tools:
        existing = [t for t in package['tools']
                    if t['name'] == tool['id'] and t['version'] == tool['version']]
        systems = []
        for system in tool['systems']:
            name = system['archiveFileName']
            require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]*', name), 'Unsafe asset name')
            path = assets / name
            require(path.stat().st_size == system['size'] and sha256(path) == system['sha256'],
                    f'Tool archive differs from manifest: {name}')
            if not existing:
                require(system['url'] == 'https://github.com/coloz/arduino-mcs251/releases/download/v'
                        + version + '/' + name, f'Unexpected tool URL: {name}')
            contents = payload(path, system['archiveRoot'])
            if tool['id'] == 'stcxx-toolchain':
                info = json.loads(contents['toolchain.json'])
                require(info['version'] == tool['version'] and info['execution'] == 'native'
                        and info['runtime_interpreters'] == [], 'Invalid native toolchain')
            else:
                binary = 'stc-cli.exe' if system['host'] == 'x86_64-mingw32' else 'stc-cli'
                require(binary in contents, 'Missing native uploader')
                require(json.loads(contents['build-info.json'])['sourceCommit'] == tool['sourceCommit'],
                        'Uploader source commit mismatch')
            systems.append({'host': system['host'], 'url': system['url'],
                            'archiveFileName': name, 'checksum': 'SHA-256:' + system['sha256'],
                            'size': str(system['size'])})
        entry = {'name': tool['id'], 'version': tool['version'], 'systems': systems}
        if existing:
            require(len(existing) == 1 and existing[0] == entry,
                    f'Published tool version is immutable: {tool["id"]} {tool["version"]}')
        else:
            package['tools'].insert(0, entry)
    devices = json.loads(files['tools/variants/devices.json'])['devices']
    require(devices and all(d['target'] == 'mcs251' for d in devices), 'Invalid devices')
    package['platforms'].insert(0, {
        'name': 'mcs251', 'architecture': 'mcs251', 'version': version,
        'category': 'Contributed',
        'url': f'https://github.com/coloz/arduino-mcs251/releases/download/v{version}/{platform.name}',
        'archiveFileName': platform.name, 'checksum': 'SHA-256:' + sha256(platform),
        'size': str(platform.stat().st_size), 'boards': [{'name': d['model']} for d in devices],
        'toolsDependencies': [{'packager': 'stc', 'name': t['id'], 'version': t['version']} for t in tools],
    })
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--previous', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, default=Path(__file__).resolve().parents[1] / 'tools/toolchain-manifest.json')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = create(args.platform, args.assets, json.loads(args.previous.read_text(encoding='utf-8')),
                    json.loads(args.manifest.read_text(encoding='utf-8')))
    encoded = json.dumps(result, indent=2) + '\n'
    args.output.write_text(encoded, encoding='utf-8', newline='\n')
    print('PASS: native payload manifests, immutable archive bindings and retained previous releases')


if __name__ == '__main__':
    main()
