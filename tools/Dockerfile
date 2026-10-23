# Toolchain de compilacao cruzada para o app nativo (webOS 4 / ARMv7).
#
# Esta receita existe porque a imagem foi montada a mao em 30/08 e se perdeu:
# sem Dockerfile versionado nao havia como reconstruir o ambiente, e sem
# binario ARM nao ha o que instalar na TV. Nao apague.
#
#   docker build --platform linux/arm64 -t nuvio-webos-sdk tools/
#
# O SDK e o buildroot da openlgtv. `relocate-sdk.sh` precisa rodar uma vez
# depois de extrair: o buildroot grava caminhos absolutos do host onde foi
# gerado, e sem relocar o gcc nao acha o proprio sysroot.
FROM debian:bookworm-slim

ARG SDK_URL=https://github.com/openlgtv/buildroot-nc4/releases/download/webos-a38c582/arm-webos-linux-gnueabi_sdk-buildroot-aarch64.tar.gz

RUN apt-get update \
  && apt-get install -y --no-install-recommends ca-certificates curl file xz-utils \
     autoconf automake libtool make gcc g++ meson ninja-build pkg-config python3 \
  && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "$SDK_URL" -o /tmp/sdk.tar.gz \
  && mkdir -p /opt \
  && tar -xzf /tmp/sdk.tar.gz -C /opt \
  && rm /tmp/sdk.tar.gz \
  && /opt/arm-webos-linux-gnueabi_sdk-buildroot/relocate-sdk.sh

ENV PATH=/opt/arm-webos-linux-gnueabi_sdk-buildroot/bin:$PATH
ENV NUVIO_SYSROOT=/opt/arm-webos-linux-gnueabi_sdk-buildroot/arm-webos-linux-gnueabi/sysroot

# O firmware nao fornece libass. O renderer completo e os seus componentes de
# texto entram estaticos no prefixo da imagem e sao ligados pelo tools/arm.sh.
ENV NUVIO_ASS_ROOT=/opt/nuvio-ass
COPY build-ass-arm.sh /tmp/build-ass-arm.sh
RUN chmod +x /tmp/build-ass-arm.sh \
  && CC=arm-webos-linux-gnueabi-gcc NUVIO_SYSROOT="$NUVIO_SYSROOT" \
     NUVIO_ASS_ROOT="$NUVIO_ASS_ROOT" /tmp/build-ass-arm.sh \
  && rm -f /tmp/build-ass-arm.sh

# Local DTS conversion is part of the normal webOS build. Specialized images
# can omit the dependency with --build-arg NUVIO_DTS_FFMPEG=0.
ARG NUVIO_DTS_FFMPEG=1
ENV NUVIO_DTS_ROOT=/opt/nuvio-dts
COPY build-dts-ffmpeg.sh /tmp/build-dts-ffmpeg.sh
RUN if [ "$NUVIO_DTS_FFMPEG" = 1 ]; then \
      CC=arm-webos-linux-gnueabi-gcc /bin/sh /tmp/build-dts-ffmpeg.sh; \
    fi \
  && rm -f /tmp/build-dts-ffmpeg.sh

# Experimental P7 -> P8.1 metadata conversion; no software video decoder.
# Pin Rust as well as Cargo.lock. The target uses the LG SDK soft-float ABI.
ARG NUVIO_DV7=1
RUN if [ "$NUVIO_DV7" = 1 ]; then \
      curl --proto "=https" --tlsv1.2 -fsSL https://sh.rustup.rs -o /tmp/rustup.sh \
      && sh /tmp/rustup.sh -y --profile minimal --default-toolchain 1.88.0 \
      && /root/.cargo/bin/rustup target add armv7-unknown-linux-gnueabi \
      && rm /tmp/rustup.sh; \
    fi
ENV PATH=/root/.cargo/bin:$PATH

WORKDIR /work
