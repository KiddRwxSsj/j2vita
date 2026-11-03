#!/usr/bin/env bash
# Builds the phoneME/PSPKVM core libraries for the Vita and then the J2Vita VPK.
#
# Needs: VitaSDK (VITASDK), a JDK 8 (JDK_DIR), and a host compiler that can do -m32.
# HOST32_BIN can point at a directory with gcc/g++ wrappers for the 32-bit romgen.
set -euo pipefail

WS_ROOT="$(cd "$(dirname "$0")" && pwd)"
: "${VITASDK:=/usr/local/vitasdk}"
: "${JDK_DIR:?set JDK_DIR to a JDK 8 root}"
export VITASDK JDK_DIR JAVA_HOME="$JDK_DIR"
export PATH="${HOST32_BIN:+$HOST32_BIN:}$JDK_DIR/bin:$VITASDK/bin:$PATH"
hash -r

die() { echo "error: $*" >&2; exit 1; }
[ -x "$VITASDK/bin/arm-vita-eabi-gcc" ] || die "arm-vita-eabi-gcc not found in $VITASDK"
"$JDK_DIR/bin/javac" -version 2>&1 | grep -q '1\.8' || die "JDK_DIR must be a JDK 8"
for f in include/freetype2/ft2build.h lib/libfreetype.a lib/libvitaGL.a lib/libSDL2_mixer.a; do
  [ -f "$VITASDK/arm-vita-eabi/$f" ] || die "missing $VITASDK/arm-vita-eabi/$f (vdpm)"
done
t="$(mktemp)"
printf 'int main(){return 0;}\n' | g++ -m32 -x c++ - -o "$t" 2>/dev/null || { rm -f "$t"; die "g++ -m32 does not work (romgen needs it)"; }
rm -f "$t"

JC_OUT="$WS_ROOT/javacall/configuration/phoneMEFeature/vita_arm/output"
PCSL_OUT="$WS_ROOT/pcsl/output-vita"
JOBS="${JOBS:-$(nproc)}"
# J2V_VM=asm (default) is phoneME's generated ARM interpreter, J2V_VM=gcc the portable C one
CLDC_BUILD="$WS_ROOT/cldc/build/javacall_vita_${J2V_VM:-asm}"

make -C "$WS_ROOT/javacall/configuration/phoneMEFeature/vita_arm" \
  JAVACALL_DIR="$WS_ROOT/javacall" JAVACALL_OUTPUT_DIR="$JC_OUT" \
  USE_NATIVE_AMS=false USE_JSR_135=false USE_JSR_75=true USE_JSR_179=false \
  USE_PROPERTIES_FROM_FS=true

make -C "$WS_ROOT/pcsl" JAVACALL_OUTPUT_DIR="$JC_OUT" \
  PCSL_PLATFORM=javacall_vita_gcc PCSL_OUTPUT_DIR="$PCSL_OUT"

make -C "$CLDC_BUILD" JDK_DIR="$JDK_DIR" VITA_TARGET=true \
  ENABLE_ISOLATES=true ENABLE_PCSL=true PCSL_OUTPUT_DIR="$PCSL_OUT" \
  PCSL_DIST_DIR="$PCSL_OUT/javacall_arm" JAVACALL_OUTPUT_DIR="$JC_OUT" \
  JVMWorkSpace="$WS_ROOT/cldc"

# MIDP's incremental Java build keeps stale classes and never rebuilds the ROM image
MIDP_OUT="$WS_ROOT/midp/build/javacall_vita/output"
# natives are compiled against the chosen VM's headers
[ "$(cat "$MIDP_OUT/.vm" 2>/dev/null)" = "$CLDC_BUILD" ] || rm -rf "$MIDP_OUT/obj" "$MIDP_OUT/lib" "$MIDP_OUT/bin"
mkdir -p "$MIDP_OUT" && echo "$CLDC_BUILD" > "$MIDP_OUT/.vm"
rm -rf "$MIDP_OUT/classes.zip" "$MIDP_OUT/classes" "$MIDP_OUT/tmpclasses" "$MIDP_OUT/ROMImage.cpp" "$MIDP_OUT/obj/arm/ROMImage.o"

make -C "$WS_ROOT/midp/build/javacall_vita" JDK_DIR="$JDK_DIR" USE_NATIVE_AMS=false \
  JAVACALL_PLATFORM=vita_arm JAVACALL_OUTPUT_DIR="$JC_OUT" \
  PCSL_OUTPUT_DIR="$PCSL_OUT" CLDC_DIST_DIR="$CLDC_BUILD/dist" \
  TOOLS_DIR="$WS_ROOT/tools" RESTRICTED_CRYPTO_DIR="$WS_ROOT/restricted_crypto" \
  USE_MULTIPLE_ISOLATES=true skip_ams_executables=true JPEG_DIR="$WS_ROOT/jpeg" \
  USE_JSR_120=true JSR_120_DIR="$WS_ROOT/jsr120" \
  USE_JSR_75=true JSR_75_DIR="$WS_ROOT/jsr75" USE_JSR_135=false USE_JSR_179=false \
  USE_NOKIA_UI=true NOKIA_UI_DIR="$WS_ROOT/ext/nokia" \
  USE_PSPKVM_EXT=true PSPKVM_EXT_DIR="$WS_ROOT/ext/pspkvm" \
  USE_MICRO3D=true MICRO3D_DIR="$WS_ROOT/ext/micro3d" \
  SOFT_KEYBOARD_ENABLED=false JSR_172_DIR="$WS_ROOT/jsr172" USE_JSR_172=true \
  USE_JSR_226=true JSR_226_DIR="$WS_ROOT/jsr226" PISCES_DIR="$WS_ROOT/pisces"

FREEPATS="${FREEPATS:-$WS_ROOT/build/freepats}"
VITAGL="${VITAGL:-$WS_ROOT/build/vitagl}"
FFAMR="${FFAMR:-$WS_ROOT/build/ffmpeg-amr}"
"$WS_ROOT/scripts/build-vitagl.sh" "$VITAGL"
"$WS_ROOT/scripts/fetch-freepats.sh" "$FREEPATS"
"$WS_ROOT/scripts/build-ffmpeg-amr.sh" "$FFAMR"
# compat.o and the link depend on the VM flavour
rm -f "$WS_ROOT"/vita/*.o "$WS_ROOT/vita/j2vita.elf"
make -C "$WS_ROOT/vita" -j"$JOBS" J2V_VM="${J2V_VM:-asm}" FREEPATS="$FREEPATS" VITAGL="$VITAGL" FFAMR="$FFAMR"
