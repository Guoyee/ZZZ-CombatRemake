import unreal

# Read skeleton Bip001 ref pose
sk_path = '/Game/Asset/珂蕾妲/attacktest/K_Attack_01_Skeleton'
sk = unreal.load_asset(sk_path)

# Get ref pose from skeletal mesh
mesh = unreal.load_asset('/Game/Asset/珂蕾妲/attacktest/K_Attack_01')
ref_pose = mesh.get_ref_pose_base_transform()
bone_names = sk.get_bone_names()

# Get Bip001 ref pose
for i, name in enumerate(bone_names):
    if str(name) == 'Bip001':
        tr = ref_pose[i]
        q = tr.rotation
        euler = q.rotator()
        print(f'=== Skeleton Bip001 Ref Pose (after Av removal) ===')
        print(f'  pos=({tr.translation.x:.4f},{tr.translation.y:.4f},{tr.translation.z:.4f})')
        print(f'  rot=({euler.pitch:.4f},{euler.yaw:.4f},{euler.roll:.4f})')
        print(f'  quat=({q.x:.6f},{q.y:.6f},{q.z:.6f},{q.w:.6f})')
        print(f'  scale=({tr.scale3d.x:.2f},{tr.scale3d.y:.2f},{tr.scale3d.z:.2f})')
        break

# Now compute what the composition SHOULD be
print('\n=== Computing expected composition ===')
av_rot = unreal.Quat(-0.70710678, 0.0, 0.0, 0.70710678)  # X-90

# Original Bip001 ref pose values (from old skeleton before Av removal)
bip_old_ref_rot = unreal.Rotator(pitch=57.946857, yaw=-148.155304, roll=118.054062).quaternion()
bip_old_ref_pos = unreal.Vector(0, -0.750770, 0)
av_scale = 500.0

print(f'  Av_rot (X-90): ({av_rot.x:.6f},{av_rot.y:.6f},{av_rot.z:.6f},{av_rot.w:.6f})')
print(f'  Bip001_old_ref_quat: ({bip_old_ref_rot.x:.6f},{bip_old_ref_rot.y:.6f},{bip_old_ref_rot.z:.6f},{bip_old_ref_rot.w:.6f})')

# Expected composed transform (using FTransform multiplication)
expected_q = av_rot * bip_old_ref_rot
expected_pos = av_rot.rotate_vector(bip_old_ref_pos * av_scale)
expected_euler = expected_q.rotator()

print(f'\n  Expected composed quat: ({expected_q.x:.6f},{expected_q.y:.6f},{expected_q.z:.6f},{expected_q.w:.6f})')
print(f'  Expected composed euler: ({expected_euler.pitch:.4f},{expected_euler.yaw:.4f},{expected_euler.roll:.4f})')
print(f'  Expected composed pos: ({expected_pos.x:.4f},{expected_pos.y:.4f},{expected_pos.z:.4f})')

# Compare with skeleton actual
print(f'\n  Skeleton actual quat: ({q.x:.6f},{q.y:.6f},{q.z:.6f},{q.w:.6f})')
print(f'  Skeleton actual euler: ({euler.pitch:.4f},{euler.yaw:.4f},{euler.roll:.4f})')

# Check if quaternions match (same rotation up to sign)
dot = abs(expected_q.x*q.x + expected_q.y*q.y + expected_q.z*q.z + expected_q.w*q.w)
print(f'\n  Quaternion dot product: {dot:.6f} (should be ~1.0 if same rotation)')

# Also verify position
print(f'  Skeleton actual pos: ({tr.translation.x:.4f},{tr.translation.y:.4f},{tr.translation.z:.4f})')
pos_diff = expected_pos - tr.translation
print(f'  Position diff: ({pos_diff.x:.4f},{pos_diff.y:.4f},{pos_diff.z:.4f})')

# Test different Av rotations
print('\n=== Testing different Av rotations ===')
for roll_val in [90, -90]:
    q_test = unreal.Rotator(pitch=0, yaw=0, roll=roll_val).quaternion()
    euler_test = q_test.rotator()
    print(f'  Rotator(0,0,{roll_val}).quat: ({q_test.x:.6f},{q_test.y:.6f},{q_test.z:.6f},{q_test.w:.6f}) euler=({euler_test.pitch:.2f},{euler_test.yaw:.2f},{euler_test.roll:.2f})')
    # Test rotate_vector
    v = q_test.rotate_vector(unreal.Vector(0, -0.751, 0))
    print(f'    rotate(0,-0.751,0): ({v.x:.4f},{v.y:.4f},{v.z:.4f})')
