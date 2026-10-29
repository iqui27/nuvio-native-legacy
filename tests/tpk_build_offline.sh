#!/bin/bash
# Compile all four hosts without restoring packages or contacting NuGet.
set -u
cd "$(dirname "$0")/.."
dotnet_bin="$(command -v dotnet || true)"
if [ -z "$dotnet_bin" ]; then
    dotnet_bin="${HOME}/.dotnet/dotnet"
fi
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE=true
rc=0
for project in NuvioTpk40 NuvioTpk60 NuvioTpk65 NuvioTpk; do
    "$dotnet_bin" build "tizen-tpk/$project/$project.csproj" --no-restore -p:NuGetAudit=false -verbosity:quiet
    result=$?
    echo "$project build rc=$result"
    if [ "$result" -ne 0 ]; then rc=$result; fi
done
exit "$rc"
