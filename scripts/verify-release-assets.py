#!/usr/bin/env python3
"""Verify release assets against an Arduino index or an internal manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_file(path, asset):
    require(path.is_file(), f'Missing {path}')
    require(path.stat().st_size == asset['size'], f'Size mismatch: {path}')
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    require(digest.hexdigest() == asset['sha256'], f'SHA-256 mismatch: {path}')


def verify(manifest_path, directory, platform_archive=None, version=None):
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if 'packages' in manifest:
        packages = manifest['packages']
        require(isinstance(packages, list) and len(packages) == 1, 'Expected one Arduino package')
        package = packages[0]
        platforms = [p for p in package.get('platforms', []) if version is None or p['version'] == version]
        require(len(platforms) == 1, 'Expected one published platform version')
        dependencies = {(d['name'], d['version']) for d in platforms[0]['toolsDependencies']}
        selected_tools = [t for t in package.get('tools', []) if (t['name'], t['version']) in dependencies]
        require({(t['name'], t['version']) for t in selected_tools} == dependencies, 'Missing tool dependency')
        rows = platforms + [system for tool in selected_tools for system in tool['systems']]
        assets = {}
        for row in rows:
            checksum = row.get('checksum', '')
            require(checksum.startswith('SHA-256:'), 'Expected SHA-256 archive checksum')
            asset = {'name': row['archiveFileName'], 'size': int(row['size']), 'sha256': checksum[8:]}
            require(asset['name'] not in assets or assets[asset['name']] == asset,
                    'Conflicting archive bindings: ' + asset['name'])
            assets[asset['name']] = asset
        manifest = {'schema_version': 1, 'version': platforms[0]['version'], 'assets': list(assets.values())}
    require(manifest.get('schema_version') == 1, 'Unsupported manifest schema')
    require(isinstance(manifest.get('version'), str) and
            re.fullmatch(r'\d+\.\d+\.\d+', manifest['version']), 'Invalid release version')
    assets = manifest.get('assets')
    require(isinstance(assets, list) and assets, 'Release assets must be a nonempty list')
    names = set()
    directory = directory.resolve()
    # Validate the complete inventory before inspecting any payload.
    for asset in assets:
        require(isinstance(asset, dict), 'Invalid asset entry')
        name = asset.get('name')
        require(isinstance(name, str) and
                re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]*', name), 'Unsafe asset name')
        require(name not in names, f'Duplicate asset: {name}')
        names.add(name)
        require(type(asset.get('size')) is int and asset['size'] > 0, f'Invalid size: {name}')
        require(isinstance(asset.get('sha256'), str) and
                re.fullmatch(r'[0-9a-f]{64}', asset['sha256']), f'Invalid SHA-256: {name}')
        path = directory / name
        require(path.resolve().parent == directory, f'Asset escapes release directory: {name}')
    for asset in assets:
        verify_file(directory / asset['name'], asset)
        print('PASS', asset['name'])
    if platform_archive:
        name = platform_archive.name
        require(name in names, f'Missing platform asset: {name}')
        verify_file(platform_archive, next(asset for asset in assets if asset['name'] == name))
    print(f'Verified {len(assets)} immutable release assets')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path, help='Arduino package index or internal asset manifest')
    parser.add_argument('directory', type=Path)
    parser.add_argument('--platform-archive', type=Path)
    parser.add_argument('--version', help='select a platform version from a multi-version index')
    args = parser.parse_args()
    try:
        verify(args.manifest, args.directory, args.platform_archive, args.version)
    except (ValueError, OSError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
