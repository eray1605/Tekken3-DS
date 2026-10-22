"""The costumes' models for the DS: each fighting costume's .kmd and textures from the disc into
`models/costume_NN.bin` (read by ds/source/content/model_file.c), built from the same steps as the
remake's (tools/remake_import/character.py: drawn rows, the seam exchange, the materials' pages).

The DS draws the PlayStation's faces as they are: triangles and quads (one polygon each against
the 3D engine's 2048), every corner one part-local vertex or the 0.5/0.5 blend of two (the seams),
posed on the CPU from the joint frames each frame. Each material becomes its own small texture,
4 or 8 bits per texel as on the PlayStation: the rectangle of its page its faces use (sizes powers
of two), with its CLUT as the palette. The DS shows only palette index 0 transparent, so the
CLUT's transparent colours (0x0000) are moved there.

Not yet: the hands' vertex variants (the hands keep the base block's shape), the closed eyes and
the normals' smooth halves (each corner keeps the normal the game lights it with).

    char[4] "T3MD", u32 version, u16 vertex count, u16 normal count, u16 material count,
    u16 batch count, u32 corner count;
    vertices: u8 joint, u8 pad, s16 x, y, z (part-local);
    normals: u8 joint, u8 pad, s16 x, y, z (part-local, 4.12);
    materials: u8 depth (4, 8), u8 flags (1: index 0 transparent), u16 width, u16 height,
        u16 palette size, u32 texel bytes, then u16 palette[], the texels (rows of `width`, 4-bit
        texels low nibble first), zero-padded to four bytes;
    batches: u16 material, u8 corners per polygon (3, 4), u8 flags (1: double-sided),
        u32 first corner, u32 polygon count;
    corners: u16 vertex a, u16 vertex b (a when not a seam), u16 normal, u8 weight of b (/256),
        u8 u, u8 v (texels in the material's texture), u8 pad; a quad's corners in drawing order
    (0, 1, 3, 2 of the PlayStation's).
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

import character
import kmd as kmdlib

MAGIC = b"T3MD"
VERSION = 1
TARGET_DIR = "models"
TRANSPARENT_INDEX0 = 1
DOUBLE_SIDED = 1
PAGE = 256                      # texels per page side
MIN_TEXTURE = 8                 # the DS's smallest texture side
QUAD_ORDER = (0, 1, 3, 2)       # the GPU's v0 v1 v2 v3 (two triangles 012, 132) as a polygon
SEAM_WEIGHT = 128               # 0.5 in 1/256


@dataclass
class Material:
    depth: int
    width: int
    height: int
    u0: int
    v0: int
    palette: list[int]
    texels: np.ndarray          # height × width indices
    transparent: bool

    def encode(self) -> bytes:
        flat = self.texels.astype(np.uint8).reshape(-1)
        if self.depth == 4:
            flat = flat[0::2] | (flat[1::2] << 4)
        data = bytes(flat)
        data += bytes(-len(data) % 4)
        head = struct.pack("<BBHHHI", self.depth, TRANSPARENT_INDEX0 if self.transparent else 0, self.width,
                           self.height, len(self.palette), len(data))
        return head + struct.pack(f"<{len(self.palette)}H", *self.palette) + data


@dataclass
class Model:
    vertices: list[tuple[int, int, int, int]] = field(default_factory=list)   # joint, x, y, z
    normals: list[tuple[int, int, int, int]] = field(default_factory=list)
    materials: list[Material] = field(default_factory=list)
    batches: list[tuple[int, int, int, int, int]] = field(default_factory=list)  # material, n, flags, first, count
    corners: list[tuple[int, int, int, int, int, int]] = field(default_factory=list)  # a, b, normal, wb, u, v


def texture_side(lo: int, hi: int) -> tuple[int, int]:
    """The start and power-of-two size of a texture covering texels lo…hi of a page."""
    size = MIN_TEXTURE
    while size < hi - lo + 1:
        size *= 2
    return min(lo, PAGE - size), size


def material_texture(page: np.ndarray, clut: list[int], depth: int, uvs: list[tuple[int, int]]) -> Material:
    """The rectangle of a page (256 × 256 indices) its faces' UVs cover, with the CLUT as palette;
    transparent colours (0x0000) moved to index 0."""
    u0, width = texture_side(min(u for u, _ in uvs), max(u for u, _ in uvs))
    v0, height = texture_side(min(v for _, v in uvs), max(v for _, v in uvs))
    texels = page[v0:v0 + height, u0:u0 + width].astype(np.int64)
    palette = [c & 0xFFFF for c in clut]
    clear = [i for i, c in enumerate(palette) if c == 0]
    transparent = bool(clear)
    if clear and clear != [0]:
        # Every transparent index reads 0; an opaque colour at 0 moves to the first freed index.
        target = np.arange(len(palette))
        target[clear] = 0
        if palette[0] != 0:
            moved = next(i for i in clear if i != 0)
            target[0] = moved
            palette[moved] = palette[0]
        palette[0] = 0
        texels = target[texels]
    return Material(depth, width, height, u0, v0, [c & 0x7FFF for c in palette], texels, transparent)


def build(disc, slot: int, kmd_id: int, arc_id: int, name: str) -> Model:
    """A costume's model from the disc (a common.Disc in Japan Rev.1's layout)."""
    data = disc.bns(kmd_id)
    kmd = kmdlib.parse(data)
    order = character.drawn_rows(kmd, character.enabled_attachments(disc, slot))
    positions = character.exchange_positions(kmd, order)
    normal_slots = character.exchange_normals(kmd, order)
    vram = character.model_textures(disc, slot, arc_id)
    out = Model()
    vertex_of: dict[character.Term, int] = {}
    normal_of: dict[character.Term, int] = {}

    def vertex(term: character.Term) -> int:
        if term not in vertex_of:
            vertex_of[term] = len(out.vertices)
            out.vertices.append((kmdlib.PART_MATRIX[term.part], *kmd.rows[term.row].verts.coords[term.index]))
        return vertex_of[term]

    def normal(term: character.Term) -> int:
        if term not in normal_of:
            normal_of[term] = len(out.normals)
            out.normals.append((kmdlib.PART_MATRIX[term.part], *kmd.rows[term.row].normals.coords[term.index]))
        return normal_of[term]

    # Faces by (material, corner count, sidedness): corners (terms, normal term, u, v).
    groups: dict[tuple[int, int, bool], list[list]] = {}
    for part, row_index in order:
        row = kmd.rows[row_index]
        slots = positions[(part, row_index)]
        colours = normal_slots[(part, row_index)]
        for face, tex in zip(row.faces, row.tex.faces):
            corners = []
            for c, vertex_slot in enumerate(face.verts):
                terms = character.blend_terms(slots[vertex_slot], 2, name)
                colour = face.colors[0 if face.kind in ("ft3", "ft4") else c]
                corners.append((terms, colours[colour >> 2], *row.tex.uvs[tex.uvs[c]]))
            key = (row.tex.materials[tex.material], len(corners), face.double_sided)
            groups.setdefault(key, []).append(corners)

    used: dict[int, list[tuple[int, int]]] = {}
    for (material, _, _), faces in groups.items():
        used.setdefault(material, []).extend((u, v) for corners in faces for *_, u, v in corners)
    index_of = {}
    for material in sorted(used):
        page_x, page_y, depth, clut_x, clut_y = character.material_page(material)
        index_of[material] = len(out.materials)
        clut = [int(c) for c in vram.clut(clut_x, clut_y, 1 << depth)]
        out.materials.append(material_texture(vram.page_indices(page_x, page_y, depth), clut, depth, used[material]))

    for (material, count, double_sided), faces in sorted(groups.items()):
        m = out.materials[index_of[material]]
        out.batches.append((index_of[material], count, DOUBLE_SIDED if double_sided else 0, len(out.corners), len(faces)))
        for corners in faces:
            for c in (QUAD_ORDER if count == 4 else (0, 1, 2)):
                terms, normal_term, u, v = corners[c]
                a = vertex(terms[0][0])
                b, wb = (vertex(terms[1][0]), SEAM_WEIGHT) if len(terms) == 2 else (a, 0)
                if len(terms) == 2 and terms[0][1] != 0.5:
                    raise ValueError(f"{name}: a seam weighs {terms}")
                out.corners.append((a, b, normal(normal_term), wb, u - m.u0, v - m.v0))
    return out


def encode(model: Model) -> bytes:
    out = bytearray(MAGIC + struct.pack("<IHHHHI", VERSION, len(model.vertices), len(model.normals),
                                        len(model.materials), len(model.batches), len(model.corners)))
    for joint, x, y, z in model.vertices + model.normals:
        out += struct.pack("<Bx3h", joint, x, y, z)
    for material in model.materials:
        out += material.encode()
    for material, count, flags, first, polygons in model.batches:
        out += struct.pack("<HBBII", material, count, flags, first, polygons)
    for a, b, n, wb, u, v in model.corners:
        out += struct.pack("<HHHBBBx", a, b, n, wb, u, v)
    return bytes(out)


def convert(disc, out: Path) -> list[Path]:
    paths = []
    for name, slot, kmd_id, arc_id in character.COSTUMES:
        target = out / TARGET_DIR / f"{name}.bin"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(encode(build(disc, slot, kmd_id, arc_id, name)))
        paths.append(target)
    return paths
