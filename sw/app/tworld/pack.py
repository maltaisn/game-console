from dataclasses import dataclass
from hashlib import sha256
from pathlib import Path
import sys

sys.path.append(str(Path(__file__).absolute().parent / "utils"))
from dat_convert import Config, EncodeError, DatFileReader, DatFileWriter, create_level_data
from assets_packer import Packer, DataObject, PackResult, ArrayType, PackError

p = Packer(cover_image="cover.png")


@dataclass(frozen=True)
class LevelPackObject(DataObject):
    file: Path
    name: str

    def pack(self) -> PackResult:
        try:
            with open(self.file, "rb") as file:
                sha = sha256(file.read()).hexdigest()
        except IOError as e:
            raise PackError(f"encoding error: could not read file: {e}")

        # level conversion takes some time, used cached result whenever possible.
        cache_dir = Path("assets/cache")
        cache_dir.mkdir(exist_ok=True)
        cached_file = cache_dir / f"{sha[:10]}.dat"
        if cached_file.exists():
            with open(cached_file, "rb") as file:
                return PackResult(file.read())

        config = Config(self.file, cached_file, self.name)
        try:
            data = create_level_data(config)
            return PackResult(data)
        except EncodeError as e:
            raise PackError(f"encoding error: {e}")

    def get_type_name(self) -> str:
        return "level pack"


@p.file_builder
def level_pack(filename: Path, name: str) -> LevelPackObject:
    return LevelPackObject(filename, name)


# music
tempo = 60
with p.group("music"):
    music_args = {"tempo": tempo, "channels": {0, 1}, "merge_midi_tracks": True}
    # p.sound("music0.mid", name="theme0", **music_args)
    # p.sound("music1.mid", name="theme1", **music_args)
    p.define("tempo", tempo)

# fonts
with p.group("font"):
    p.font("font5x7.png", name="5x7", glyph_width=5, glyph_height=7)
    p.font("font7x7.png", name="7x7", glyph_width=7, glyph_height=7)

# images
with p.group("image"):
    p.image("arrow-up.png")
    p.image("arrow-down.png")

    p.image("pack/pack-password.png")
    p.image("pack/pack-locked.png")
    with p.array("pack_progress", ArrayType.REGULAR):
        for i in range(9):
            p.image(f"pack/pack-progress-{i}.png")

# levels
with p.array("level_packs", ArrayType.INDEXED_ABS):
    p.level_pack("cclp1.dat", "CCLP1")
    p.level_pack("cclp2.dat", "CCLP2")
    p.level_pack("cclp3.dat", "CCLP3")
    p.level_pack("cclp4.dat", "CCLP4")

p.pack()
