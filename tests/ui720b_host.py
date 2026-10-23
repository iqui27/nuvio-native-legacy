"""Compile the real Tizen NUI host against cached references, without restore."""
from pathlib import Path
import subprocess
import tempfile

home = Path.home()
sdk = sorted((home / '.dotnet/sdk').glob('*/Roslyn/bincore/csc.dll'))[-1]
framework = sorted((home / '.dotnet/packs/Microsoft.NETCore.App.Ref').glob('*/ref/net8.0'))[-1]
sources = [Path('tizen-tpk') / p for p in
           ['Program.cs', 'Apps.cs', 'Video.cs', 'VideoWindowMetrics.cs', 'Carga.cs', 'Voz.cs']]
with tempfile.TemporaryDirectory(prefix='ui720b-host-') as directory:
    work = Path(directory)
    for api in [8, 9, 11]:
        package = home / '.nuget/packages' / f'tizen.net.api{api}' if api != 11 else home / '.dotnet/packs/Samsung.Tizen.Ref.API11'
        refs = sorted(package.glob('*/ref/*'))[-1]
        assemblies = list(framework.glob('*.dll')) + [p for p in refs.glob('*.dll') if not (framework / p.name).exists()]
        response = work / 'host.rsp'
        response.write_text('\n'.join('-r:"' + str(p) + '"' for p in assemblies) + '\n' +
                            '\n'.join('"' + str(p.resolve()) + '"' for p in sources))
        subprocess.run([str(home / '.dotnet/dotnet'), str(sdk), '-nologo', '-langversion:8',
                        '-target:library', f'-define:NV_API{api}', '-out:' + str(work / 'host.dll'),
                        '@' + str(response)], check=True)
        print(f'PASS Tizen API{api} production host compile (offline)')
