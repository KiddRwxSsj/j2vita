#!/usr/bin/env bash
# Downloads the FreePats GUS patch set (GPL) used by SDL_mixer's Timidity for MIDI
# and writes a timidity.cfg for it. FreePats only covers part of General MIDI, so
# missing programs borrow the closest patch from the same GM family of 8.
# Usage: fetch-freepats.sh <dest-dir>
set -euo pipefail
dest="${1:?destination dir}"
[ -f "$dest/timidity.cfg" ] && exit 0

url=http://deb.debian.org/debian/pool/main/f/freepats/freepats_20060219.orig.tar.gz
sha=70bf8ca084df3903d6c9de43fe20539fc0a553d95cfba4d525da3fe66fda5f10

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -fsSL "$url" -o "$tmp/fp.tar.gz"
echo "$sha  $tmp/fp.tar.gz" | sha256sum -c --quiet
tar xzf "$tmp/fp.tar.gz" -C "$tmp"
tar xjf "$tmp"/freepats-20060219/upstream/freepats-20060219.tar.bz2 -C "$tmp"

mkdir -p "$dest"
cp -r "$tmp"/freepats/Tone_000 "$tmp"/freepats/Drum_000 "$tmp"/freepats/COPYING "$tmp"/freepats/README "$dest"/

python3 - "$tmp/freepats/freepats.cfg" "$dest/timidity.cfg" <<'PY'
import re, sys
src, out = sys.argv[1], sys.argv[2]
section, drums, tones = None, [], {}
for line in open(src):
    s = line.strip()
    if s.startswith("drumset"): section = "drum"; continue
    if s.startswith("bank"): section = "tone"; continue
    m = re.match(r"(\d+)\s+(\S+)(.*)", s)
    if not m: continue
    if section == "drum": drums.append(s)
    elif section == "tone": tones[int(m.group(1))] = (m.group(2) + m.group(3)).strip()
def pick(p):
    fam = range(p // 8 * 8, p // 8 * 8 + 8)
    near = sorted((q for q in tones if q in fam), key=lambda q: abs(q - p))
    return tones[near[0]] if near else tones[min(tones, key=lambda q: abs(q - p))]
with open(out, "w") as f:
    f.write("dir app0:/timidity\n\ndrumset 0\n")
    for d in drums: f.write(" " + d + "\n")
    f.write("\nbank 0\n")
    for p in range(128): f.write(" %d %s\n" % (p, tones.get(p) or pick(p)))
PY
