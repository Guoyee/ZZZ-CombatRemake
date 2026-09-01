import unreal
import math

seq_path = '/Game/Asset/珂蕾妲/attacktest/K_Attack_01_Anim'
seq = unreal.load_asset(seq_path)
ctrl = seq.controller

# X-90° for position only (Z→Y)
av_rot = unreal.Quat(-0.70710678, 0.0, 0.0, 0.70710678)

# Original animation keyframes
orig = {
    0:  {'loc': unreal.Vector(0, -0.750770, 0),              'pitch': 57.946857,  'yaw': -148.155304, 'roll': 118.054062},
    10: {'loc': unreal.Vector(0.155518, -0.426662, 0.991923), 'pitch': -15.585036, 'yaw': -115.967369, 'roll': 55.815517},
    21: {'loc': unreal.Vector(0.010740, -0.480535, 2.644777), 'pitch': 25.959532,  'yaw': -21.458426,  'roll': -117.978424},
}

# Position: rotated X-90°, Rotation: ORIGINAL, Scale: 500
kf = {}
for f, d in orig.items():
    kf[f] = {
        'loc': av_rot.rotate_vector(d['loc']),
        'quat': unreal.Rotator(d['pitch'], d['yaw'], d['roll']).quaternion(),
        'scl': unreal.Vector(500, 500, 500)
    }

d21 = kf[21]['loc'] - kf[0]['loc']
print(f'Frame 0:  pos=({kf[0]["loc"].x:.4f},{kf[0]["loc"].y:.4f},{kf[0]["loc"].z:.4f})')
print(f'Frame 21: pos=({kf[21]["loc"].x:.4f},{kf[21]["loc"].y:.4f},{kf[21]["loc"].z:.4f})')
print(f'Delta: ΔY={d21.y:.4f}')

# Interpolate
def slerp(q1, q2, t):
    dot = q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w
    if dot < 0: q2 = unreal.Quat(-q2.x, -q2.y, -q2.z, -q2.w); dot = -dot
    if dot > 0.9995:
        r = unreal.Quat(q1.x+t*(q2.x-q1.x), q1.y+t*(q2.y-q1.y), q1.z+t*(q2.z-q1.z), q1.w+t*(q2.w-q1.w))
        mag = math.sqrt(r.x**2+r.y**2+r.z**2+r.w**2)
        return unreal.Quat(r.x/mag, r.y/mag, r.z/mag, r.w/mag)
    t0 = math.acos(dot); st0 = math.sin(t0); th = t0*t; sth = math.sin(th)
    s0 = math.cos(th) - dot*sth/st0; s1 = sth/st0
    return unreal.Quat(s0*q1.x+s1*q2.x, s0*q1.y+s1*q2.y, s0*q1.z+s1*q2.z, s0*q1.w+s1*q2.w)

def lerp(v1, v2, t):
    return unreal.Vector(v1.x+t*(v2.x-v1.x), v1.y+t*(v2.y-v1.y), v1.z+t*(v2.z-v1.z))

pos, rot, scl = [], [], []
for frame in range(22):
    if frame <= 10:
        t = frame / 10.0
        q = slerp(kf[0]['quat'], kf[10]['quat'], t)
        loc = lerp(kf[0]['loc'], kf[10]['loc'], t)
        sc = lerp(kf[0]['scl'], kf[10]['scl'], t)
    else:
        t = (frame - 10) / 11.0
        q = slerp(kf[10]['quat'], kf[21]['quat'], t)
        loc = lerp(kf[10]['loc'], kf[21]['loc'], t)
        sc = lerp(kf[10]['scl'], kf[21]['scl'], t)
    pos.append(loc)
    rot.append(q)
    scl.append(sc)

ctrl.set_bone_track_keys('Bip001', pos, rot, scl)

# KEY CHANGE: Use AnimFirstFrame instead of RefPose
# This uses frame 0's transform as the reference (instead of skeleton ref pose)
seq.set_editor_property('bEnableRootMotion', True)
seq.set_editor_property('RootMotionRootLock', unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
seq.set_editor_property('bForceRootLock', False)
seq.set_editor_property('bUseNormalizedRootMotionScale', True)

unreal.EditorAssetLibrary.save_asset(seq.get_path_name())
print('\nDone! RootMotionRootLock = AnimFirstFrame')
print('Position Z→Y rotated, rotation ORIGINAL, scale=500')

for f in [0, 10, 21]:
    tr = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, 'Bip001', f, False)
    euler = tr.rotation.rotator()
    if f > 0:
        d = tr.translation - unreal.AnimationLibrary.get_bone_pose_for_frame(seq, 'Bip001', 0, False).translation
        print(f'Frame {f}: loc=({tr.translation.x:.4f},{tr.translation.y:.4f},{tr.translation.z:.4f}) dY={d.y:.4f} rot=({euler.pitch:.1f},{euler.yaw:.1f},{euler.roll:.1f})')
    else:
        print(f'Frame {f}: loc=({tr.translation.x:.4f},{tr.translation.y:.4f},{tr.translation.z:.4f}) rot=({euler.pitch:.1f},{euler.yaw:.1f},{euler.roll:.1f})')
