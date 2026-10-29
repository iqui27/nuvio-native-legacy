# Install Nuvio on Hisense VIDAA

Sideload Nuvio using DNS spoofing. Runs a DNS and HTTPS server that intercepts
requests for vidaahub.com and serves an install page.

## How it works

Hisense restricts `Hisense_installApp()` to being called ONLY from a server that
answers as vidaahub.com. The script works like this:

1. Starts a DNS server that resolves vidaahub.com to this machine's IP
2. Starts an HTTPS server with a self-signed certificate
3. When the TV opens https://vidaahub.com/, it serves an HTML page that calls
   `Hisense_installApp()` to install Nuvio
4. You switch the TV's DNS back to automatic afterward

## Requirements

- Linux, macOS or Windows with Python 3
- Root or administrator privileges (ports 53 and 443)
- A Hisense VIDAA TV with internet access
- Developer Mode enabled on the TV

## Install

```bash
sudo python3 tools/vidaa-instalar/instalar.py
```

Options:

```bash
# With a custom IP (useful on networks with multiple subnets)
sudo python3 tools/vidaa-instalar/instalar.py --ip 192.168.1.100

# With a custom URL (if hosted elsewhere)
sudo python3 tools/vidaa-instalar/instalar.py --url https://your-domain.com/app/

# Skip DNS, HTTPS only (if the TV already has manual DNS set)
sudo python3 tools/vidaa-instalar/instalar.py --https-only

# See all options
python3 tools/vidaa-instalar/instalar.py --help
```

## Step by step on the TV

1. **Enable Developer Mode:**
   - Go to Settings > System > About
   - Type **1234** using the remote
   - Developer Mode should show as "ON"

2. **Change DNS:**
   - Go to Settings > Network > DNS
   - Enter the machine's IP (the script prints it on screen)
   - Save

3. **Install Nuvio:**
   - Open the TV's browser
   - Go to `https://vidaahub.com/`
   - Ignore the certificate warning (self-signed)
   - Click the "Install" button
   - The TV does the install

4. **Set DNS back to automatic:**
   - Go to Settings > Network > DNS
   - Set "Automatic"

## Uninstall

1. Open `https://vidaahub.com/` again
2. Click "Uninstall"

(Only works if `Hisense_uninstallApp()` is supported on that VIDAA version)

## Security notes

- Self-signed certificate: the TV will warn it doesn't trust it. That's expected.
- DNS spoofing: you're intercepting vidaahub.com. NOBODY should change the TV's
  DNS while this is running, or the install may break.
- Undo the DNS change afterward: switch back to automatic as soon as you're done.

## Certificate files

On first run, the script generates:
```
tools/vidaa-instalar/certs/vidaahub.com.crt
tools/vidaa-instalar/certs/vidaahub.com.key
```

You can delete them and the script will regenerate them next time.

## Reference

Implementation based on research of these repositories:
- https://github.com/trialuser/vidaa-appstore
- https://github.com/NoobyGains/stremio-vidaa-tv

Neither carries an explicit license. This implementation is new code, written from scratch.

## Credits

VIDAA is a Hisense trademark. Nuvio is a NuvioMedia trademark.
This is an unofficial port.
