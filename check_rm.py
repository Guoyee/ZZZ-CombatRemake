import unreal

seq = unreal.load_asset('/Game/Asset/珂蕾妲/test01/K_Attack_01')
al = unreal.AnimationLibrary

print('=== Root (does it exist? have track?) ===')
print(f'  exists: {al.does_bone_name_exist(seq, "Root")}')
for f in [0, 10, 20]:
    tr = al.get_bone_pose_for_frame(seq, 'Root', f, False)
    print(f'  F{f}: pos=({tr.translation.x:.4f},{tr.translation.y:.4f},{tr.translation.z:.4f})')

print('\n=== Bip001 ===')
for f in [0, 10, 20]:
    tr = al.get_bone_pose_for_frame(seq, 'Bip001', f, False)
    print(f'  F{f}: pos=({tr.translation.x:.4f},{tr.translation.y:.4f},{tr.translation.z:.4f})')
