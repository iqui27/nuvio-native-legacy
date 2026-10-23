# Samsung ARM32 output; Apple Container runs an ARM64 cross compiler.
# Source after tools/ctr.sh. Keep Linux's existing Docker SDK and caches.
TPK_PLATFORM=linux/arm/v5
TPK_IMAGE=nuvio-tpk-sdk
TPK_DOCKERFILE=tools/tpk/Dockerfile
TPK_EXEC=()
if [ "$NUVIO_CTR" = container ]; then
  TPK_PLATFORM=linux/arm64
  TPK_IMAGE=nuvio-tpk-sdk-arm64
  TPK_DOCKERFILE=tools/tpk/Dockerfile.arm64
  TPK_EXEC=(qemu-arm -L /)
fi

tpk_sdk() {
  ctr_check_system && { ctr_image_exists "$TPK_IMAGE" ||
    ctr_build --platform "$TPK_PLATFORM" -t "$TPK_IMAGE" -f "$TPK_DOCKERFILE" tools/tpk/; }
}
