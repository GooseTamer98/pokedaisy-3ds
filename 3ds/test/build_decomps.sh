#!/bin/bash
# Builds pret's pokefirered (rev 1), pokeemerald and pokeruby (Ruby / Sapphire rev 0 / 1 / 2) from source into
# 3ds/build/decomps/ - byte-identical to the retail ROMs - so the preview and
# the Azahar test have a ROM for the tabs that read one (MAP, POKéDEX, GUIDE).
# Never committed. `make decomps`; needs git, a host C compiler, libpng and
# devkitARM ($DEVKITARM, or /opt/devkitpro/devkitARM).
set -euo pipefail
cd "$(dirname "$0")/.."
D=build/decomps
mkdir -p "$D"
export DEVKITARM="${DEVKITARM:-/opt/devkitpro/devkitARM}"
export PATH="$DEVKITARM/bin:$PATH"

# The commits these were checked at.
clone() {
    if [ ! -d "$D/$1" ]; then
        git clone -q "https://github.com/pret/$1" "$D/$1"
        (cd "$D/$1" && git -c advice.detachedHead=false checkout -q "$2")
    fi
}
clone agbcc da598c1d918402c42c0c0d7128ba14567f3175e9
clone pokefirered 037335f4c725d7c9aecdac87066f2002b4bd7e14
clone pokeemerald 731ad5bfd6e6f265508d0efcca0ba42f9dcf5881
clone pokeruby 5784633ce4ef7ade1a7f2d2d0c288e3d5e6cdd7f

(cd "$D/agbcc" && ./build.sh > build.log 2>&1 && ./install.sh ../pokefirered > /dev/null &&
    ./install.sh ../pokeemerald > /dev/null && ./install.sh ../pokeruby > /dev/null)
make -C "$D/pokefirered" firered_rev1 -j"$(nproc)" > "$D/pokefirered.log" 2>&1
make -C "$D/pokeemerald" -j"$(nproc)" > "$D/pokeemerald.log" 2>&1
for t in ruby ruby_rev1 ruby_rev2 sapphire sapphire_rev1 sapphire_rev2; do
    make -C "$D/pokeruby" $t CPP=cpp -j"$(nproc)" > "$D/pokeruby-$t.log" 2>&1
done

# Retail FireRed rev 1, Emerald, Ruby / Sapphire rev 0 / 1 / 2.
cat > "$D/sha1" <<EOF
dd5945db9b930750cb39d00c84da8571feebf417  $D/pokefirered/pokefirered_rev1.gba
f3ae088181bf583e55daf962a92bb46f4f1d07b7  $D/pokeemerald/pokeemerald.gba
f28b6ffc97847e94a6c21a63cacf633ee5c8df1e  $D/pokeruby/pokeruby.gba
610b96a9c9a7d03d2bafb655e7560ccff1a6d894  $D/pokeruby/pokeruby_rev1.gba
5b64eacf892920518db4ec664e62a086dd5f5bc8  $D/pokeruby/pokeruby_rev2.gba
3ccbbd45f8553c36463f13b938e833f652b793e4  $D/pokeruby/pokesapphire.gba
4722efb8cd45772ca32555b98fd3b9719f8e60a9  $D/pokeruby/pokesapphire_rev1.gba
89b45fb172e6b55d51fc0e61989775187f6fe63c  $D/pokeruby/pokesapphire_rev2.gba
EOF
sha1sum -c "$D/sha1"
