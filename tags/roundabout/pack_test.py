#!/usr/bin/env python3
"""Host tests for the small indexed8 codec used by pack_roundabout.py."""
import random
import struct
import unittest

from pack_roundabout import decode_frame, encode_frame, parse_image, rebuild_image


class Indexed8CodecTests(unittest.TestCase):
    def test_repeat_and_literal_boundaries(self):
        colors = [bytes((n, 255 - n, n ^ 0x5A, n)) for n in range(256)]
        rng = random.Random(0x903)
        for width in (1, 2, 3, 126, 127, 128, 254, 255, 256, 1024):
            for indices in ([0] * width,
                            [n % 256 for n in range(width)],
                            [rng.randrange(256) for _ in range(width)]):
                pixels = b"".join(colors[n] for n in indices)
                self.assertEqual(decode_frame(encode_frame(pixels, width, 1), width, 1),
                                 pixels)

    def test_resource_boundaries_and_unchanged_frames(self):
        a = encode_frame(bytes((255, 0, 127, 0)) * 127, 127, 1)
        b = encode_frame(bytes((1, 2, 3, 255)) * 127, 127, 1)
        for kind, frames in ((2, [a]), (3, [a, b, a])):
            resource = rebuild_image(kind, (127, 1), frames)
            self.assertEqual(parse_image(resource, kind), ((127, 1), frames))
            with self.assertRaises(ValueError):
                parse_image(resource[:-1], kind)
            with self.assertRaises(ValueError):
                parse_image(resource + b"\0", kind)

    def test_malformed_rle(self):
        header = bytes.fromhex("e021a55a") + struct.pack("<I", (1025 << 4) | 1)
        for malformed in (b"", header, header + b"\0", header + b"\x01",
                          header + b"\x82\x01", header + b"\xff" + bytes(126)):
            with self.assertRaises(ValueError):
                decode_frame(malformed, 1, 1)

    def test_no_lossy_palette_fallback(self):
        colors = b"".join(bytes((n & 255, n >> 8, 0, 255)) for n in range(257))
        with self.assertRaisesRegex(ValueError, "exceeds indexed8 palette"):
            encode_frame(colors, 257, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
