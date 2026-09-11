"""
Inspect binary FBX (read-only): locate the Root / Bip001 AnimationCurveNode
patterns and report their offsets and nearby curve/key counts.

Does NOT write the file. Use it to confirm which curve carries which axis
before running fix_animation.py or rotate_root_bone.py inside the editor.
"""
import sys

def search_fbx(input_path):
    with open(input_path, 'rb') as f:
        data = f.read()

    # Search for "Root" + "\x00\x01T" pattern (AnimationCurveNode for Root translation)
    patterns = [
        (b'Root\x00\x01T', 'Root translation'),
        (b'Bip001\x00\x01T', 'Bip001 translation'),
        (b'Root\x00\x01S', 'Root scale'),
        (b'Bip001\x00\x01S', 'Bip001 scale'),
        (b'Root\x00\x01R', 'Root rotation'),
        (b'Bip001\x00\x01R', 'Bip001 rotation'),
    ]

    for pattern, desc in patterns:
        pos = data.find(pattern)
        if pos >= 0:
            # Show context around the match
            ctx_start = max(0, pos - 10)
            ctx_end = min(len(data), pos + len(pattern) + 60)
            print(f'\n=== {desc} at offset {pos} ===')
            print(f'  Pattern found: {pattern}')
            # Check if there are sub AnimationCurve nodes nearby
            nearby = data[pos:pos+200]
            curve_count = nearby.count(b'AnimationCurve')
            key_count = nearby.count(b'KeyCount')
            print(f'  AnimationCurve nodes nearby: {curve_count}')
            print(f'  KeyCount nearby: {key_count}')
        else:
            print(f'\n=== {desc}: NOT FOUND ===')

    # Also search for the string "Root" in general
    print('\n=== All "Root" occurrences ===')
    pos = 0
    for i in range(20):
        pos = data.find(b'Root', pos)
        if pos < 0:
            break
        ctx = data[pos:pos+30]
        print(f'  offset {pos}: {ctx[:30]}')
        pos += 1

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__.strip())
        print('\nusage: python Tools/AnimFix/convert_fbx.py <path-to.fbx>')
        raise SystemExit(2)
    search_fbx(sys.argv[1])
