import pathlib, sys
p = pathlib.Path(sys.argv[1])
s = p.read_text()
marker = 'ENV PATH=/opt/arm-webos-linux-gnueabi_sdk-buildroot/bin:$PATH'
assert s.count(marker) == 1
repair = """RUN for pair in dl:2 m:6 pthread:0 rt:1; do \
      name=${pair%:*}; abi=${pair#*:}; \
      base=/opt/arm-webos-linux-gnueabi_sdk-buildroot/arm-webos-linux-gnueabi/sysroot; \
      if [ ! -e "$base/usr/lib/lib$name.so" ] && [ -e "$base/lib/lib$name.so.$abi" ]; then \
        ln -sf "../../lib/lib$name.so.$abi" "$base/usr/lib/lib$name.so"; \
      fi; \
    done

"""
p.write_text(s.replace(marker, repair+marker))
