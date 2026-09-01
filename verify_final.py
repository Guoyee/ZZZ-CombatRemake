import unreal

seq = unreal.load_asset('/Game/Asset/珂蕾妲/attacktest/K_Attack_01_Anim')
sk = unreal.load_asset('/Game/Asset/珂蕾妲/attacktest/K_Attack_01_Skeleton')

print('=== Skeleton ===')
print(f'Bone count: {len(sk.get_bone_names())}')
bn = sk.get_bone_names()
print(f'Root bone: {bn[0]} (parent={sk.get_bone_parent_index(0)})')
print(f'Bone 1: {bn[1]} (parent={sk.get_bone_parent_index(1)})')

print('\n=== Animation Properties ===')
print(f'bEnableRootMotion: {seq.get_editor_property("bEnableRootMotion")}')
print(f'RootMotionRootLock: {seq.get_editor_property("RootMotionRootLock")}')
print(f'Bone count: {len(unreal.AnimationLibrary.get_animation_track_names(seq))}')

print('\n=== Root Track (extract_root_track_transform) ===')
for f in [0, 10, 21]:
    rt = unreal.AnimationLibrary.extract_root_track_transform(seq, f)
    if rt:
        print(f'Frame {f}: pos=({rt.translation.x:.2f},{rt.translation.y:.2f},{rt.translation.z:.2f})')
    else:
        print(f'Frame {f}: None')

print('\n=== Bip001 Bone Pose ===')
for f in [0, 10, 21]:
    tr = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, 'Bip001', f, False)
    print(f'Frame {f}: loc=({tr.translation.x:.2f},{tr.translation.y:.2f},{tr.translation.z:.2f}) scale=({tr.scale3d.x:.0f},{tr.scale3d.y:.0f},{tr.scale3d.z:.0f})')
