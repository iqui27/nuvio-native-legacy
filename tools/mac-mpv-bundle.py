#!/usr/bin/env python3
"""Copy/relink the complete non-system dylib closure before signing an app."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def system(path):
    return path.startswith(('/usr/lib/', '/System/Library/'))


def dependencies(path):
    return [line.strip().split(' (compatibility')[0]
            for line in run('otool', '-L', str(path)).splitlines()[1:]]


def rpaths(path):
    return re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset',
                      run('otool', '-l', str(path)))


def closure(root):
    found = {}
    pending = [Path(root).resolve()]
    while pending:
        path = pending.pop()
        if path in found:
            continue
        links = {}
        for dep in dependencies(path):
            if system(dep):
                continue
            if dep.startswith('@loader_path/'):
                source = path.parent / dep[len('@loader_path/'):]
            elif dep.startswith('@rpath/'):
                candidates = [Path(p.replace('@loader_path', str(path.parent))) /
                              dep[len('@rpath/'):] for p in rpaths(path)]
                source = next((p for p in candidates if p.exists()), None)
                if source is None:
                    raise RuntimeError(f'Unresolved dependency {dep} in {path}')
            else:
                source = Path(dep)
            source = source.resolve(strict=True)
            if source == path:  # LC_ID_DYLIB
                continue
            links[dep] = source
            pending.append(source)
        found[path] = links
    names = {}
    for path in found:
        if path.name in names and names[path.name] != path:
            raise RuntimeError(f'Dylib basename collision: {path}')
        names[path.name] = path
    return found


def metadata(graph):
    arches = None
    minimum = (11, 0)
    for path in graph:
        current = set(run('lipo', '-archs', str(path)).split())
        arches = current if arches is None else arches & current
        commands = run('otool', '-l', str(path))
        versions = re.findall(r'cmd LC_BUILD_VERSION\s+cmdsize \d+\s+platform \d+\s+minos (\d+\.\d+(?:\.\d+)?)', commands)
        versions += re.findall(r'cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+\.\d+(?:\.\d+)?)', commands)
        # Only load-command deployment versions; dylib compatibility versions
        # use different labels (current/compatibility) and do not match here.
        for version in versions:
            minimum = max(minimum, tuple(map(int, version.split('.'))))
    supported = [arch for arch in ('arm64', 'x86_64') if arch in arches]
    if not supported:
        raise RuntimeError('No common desktop architecture in mpv dependency closure')
    return {'architectures': supported, 'minimum_macos': '.'.join(map(str, minimum)),
            'libraries': len(graph)}


def bundle(root, app):
    graph = closure(root)
    info = metadata(graph)
    framework_dir = app / 'Contents/Frameworks'
    framework_dir.mkdir(parents=True, exist_ok=True)
    for path, links in graph.items():
        target = framework_dir / path.name
        shutil.copy2(path, target)
        target.chmod(0o755)
        subprocess.run(['codesign', '--remove-signature', str(target)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(['install_name_tool', '-id', '@rpath/' + path.name, str(target)], check=True)
        for old, source in links.items():
            subprocess.run(['install_name_tool', '-change', old,
                            '@loader_path/' + source.name, str(target)], check=True)
        for entry in rpaths(target):
            if entry.startswith('/') and not system(entry + '/'):
                subprocess.run(['install_name_tool', '-delete_rpath', entry, str(target)], check=True)
    executable = app / 'Contents/MacOS/Nuvio'
    for old in dependencies(executable):
        if 'libmpv' in old:
            subprocess.run(['install_name_tool', '-change', old,
                            '@rpath/' + Path(root).resolve().name, str(executable)], check=True)
    # Sign all leaves before the enclosing application is signed by the caller.
    for path in graph:
        subprocess.run(['codesign', '--force', '--sign', '-', str(framework_dir / path.name)], check=True)
    for path in [executable, *(framework_dir / p.name for p in graph)]:
        for dep in dependencies(path):
            if dep.startswith('/') and not system(dep):
                raise RuntimeError(f'Unbundled dependency {dep} in {path}')
    return info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('libmpv')
    parser.add_argument('--app', type=Path)
    args = parser.parse_args()
    print(json.dumps(bundle(args.libmpv, args.app) if args.app else metadata(closure(args.libmpv))))


if __name__ == '__main__':
    main()
